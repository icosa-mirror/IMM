import * as THREE from "three";
import {
    IMM_ANIM_VISIBILITY,
    IMM_ASSET_OGG,
    IMM_ASSET_OPUS,
    IMM_ASSET_WAV,
    IMM_LAYER_SOUND,
    IMM_SOUND_AMBISONIC,
    IMM_SOUND_FLAT,
    IMM_SOUND_POSITIONAL,
    type ImmDocument,
    type ImmSound,
    type ImmTransform,
} from "../format/imm-document";
import type { ImmPlaybackSnapshot } from "../runtime/imm-playback";

export const IMM_ATTENUATION_NONE = 0;
export const IMM_ATTENUATION_LINEAR = 1;
export const IMM_ATTENUATION_LOGARITHMIC = 2;
export const IMM_MODIFIER_NONE = 0;
export const IMM_MODIFIER_CONE = 1;
export const IMM_MODIFIER_FRUSTUM = 2;

export interface ImmAudioCodecCapabilities {
    webAudio: boolean;
    wav: CanPlayTypeResult;
    oggVorbis: CanPlayTypeResult;
    oggOpus: CanPlayTypeResult;
}

export interface ImmAudioDiagnostics {
    available: boolean;
    contextState: AudioContextState | "unavailable";
    userEnabled: boolean;
    muted: boolean;
    soundLayers: number;
    decodedSounds: number;
    playingSounds: number;
    loopingSounds: number;
    positionalSounds: number;
    sourceStarts: number;
    lastStartOffsets: ReadonlyArray<{ layerId: number; offsetSeconds: number }>;
    decodeFailures: ReadonlyArray<{ layerId: number; name: string; reason: string }>;
    codecs: ImmAudioCodecCapabilities;
    ambisonicSupported: false;
    unsupportedAmbisonicLayers: number;
}

interface DecodedSound {
    buffer: AudioBuffer;
    sound: ImmSound;
}

interface ActiveSound {
    source: AudioBufferSourceNode;
    gain: GainNode;
    panner?: PannerNode;
}

interface ImmAudioOptions {
    context?: AudioContext;
}

const forward = new THREE.Vector3(0, 0, -1);
const up = new THREE.Vector3(0, 1, 0);
const sourcePosition = new THREE.Vector3();
const sourceDirection = new THREE.Vector3();
const sourceUp = new THREE.Vector3();
const listenerPosition = new THREE.Vector3();
const listenerDirection = new THREE.Vector3();
const listenerUp = new THREE.Vector3();
const inverseRotation = new THREE.Quaternion();
const relativePosition = new THREE.Vector3();

export function probeImmAudioCapabilities(): ImmAudioCodecCapabilities {
    const audio = typeof document === "undefined" ? undefined : document.createElement("audio");
    const AudioContextConstructor = globalThis.AudioContext ??
        (globalThis as typeof globalThis & { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
    return {
        webAudio: AudioContextConstructor !== undefined,
        wav: audio?.canPlayType('audio/wav; codecs="1"') ?? "",
        oggVorbis: audio?.canPlayType('audio/ogg; codecs="vorbis"') ?? "",
        oggOpus: audio?.canPlayType('audio/ogg; codecs="opus"') ?? "",
    };
}

export class ImmWebAudio {
    readonly document: ImmDocument;
    readonly codecs = probeImmAudioCapabilities();

    #context: AudioContext | null;
    #master: GainNode | null;
    #decoded = new Map<number, DecodedSound>();
    #active = new Map<number, ActiveSound>();
    #failures: Array<{ layerId: number; name: string; reason: string }> = [];
    #sourceStarts = 0;
    #lastStartOffsets = new Map<number, number>();
    #disposed = false;
    #prepared = false;
    #userEnabled = false;
    #muted = false;
    #transportPlaying = false;
    #lastSnapshot: ImmPlaybackSnapshot | null = null;
    #listenerTransform: ImmTransform | null = null;

    constructor(document: ImmDocument, options: ImmAudioOptions = {}) {
        this.document = document;
        const AudioContextConstructor = globalThis.AudioContext ??
            (globalThis as typeof globalThis & { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
        this.#context = options.context ?? (AudioContextConstructor === undefined ? null : new AudioContextConstructor());
        this.#master = this.#context?.createGain() ?? null;
        this.#master?.connect(this.#context!.destination);
        if (this.#context?.state === "running") void this.#context.suspend();
    }

    get diagnostics(): ImmAudioDiagnostics {
        return {
            available: this.#context !== null,
            contextState: this.#context?.state ?? "unavailable",
            userEnabled: this.#userEnabled,
            muted: this.#muted,
            soundLayers: this.document.layers.filter((layer) => layer.type === IMM_LAYER_SOUND).length,
            decodedSounds: this.#decoded.size,
            playingSounds: this.#active.size,
            loopingSounds: [...this.#active.values()].filter((active) => active.source.loop).length,
            positionalSounds: [...this.#active.values()].filter((active) => active.panner !== undefined).length,
            sourceStarts: this.#sourceStarts,
            lastStartOffsets: [...this.#lastStartOffsets].map(([layerId, offsetSeconds]) => ({
                layerId,
                offsetSeconds,
            })),
            decodeFailures: [...this.#failures],
            codecs: this.codecs,
            ambisonicSupported: false,
            unsupportedAmbisonicLayers: this.document.layers.filter(
                (layer) => layer.sound?.type === IMM_SOUND_AMBISONIC,
            ).length,
        };
    }

    async prepare(): Promise<void> {
        if (this.#prepared || this.#disposed || this.#context === null) return;
        this.#prepared = true;
        for (const layer of this.document.layers) {
            if (this.#disposed) return;
            const sound = layer.sound;
            if (sound === undefined || sound.bytes.length === 0) continue;
            try {
                const encoded = sound.bytes.buffer.slice(
                    sound.bytes.byteOffset,
                    sound.bytes.byteOffset + sound.bytes.byteLength,
                ) as ArrayBuffer;
                const buffer = await this.#context.decodeAudioData(encoded);
                if (this.#disposed) return;
                this.#decoded.set(layer.id, { buffer, sound });
            } catch (error) {
                if (this.#disposed) return;
                this.#failures.push({
                    layerId: layer.id,
                    name: layer.name,
                    reason: error instanceof Error ? error.message : String(error),
                });
            }
        }
        this.#reconcile(true);
    }

    async enable(): Promise<void> {
        if (this.#disposed || this.#context === null) return;
        this.#userEnabled = true;
        this.#muted = false;
        this.#setMasterGain(1);
        if (this.#transportPlaying && pageIsVisible()) await this.#context.resume();
        else await this.#context.suspend();
        this.#reconcile(true);
    }

    setMuted(muted: boolean): void {
        this.#muted = muted;
        this.#setMasterGain(muted ? 0 : 1);
    }

    async setPageVisible(visible: boolean): Promise<void> {
        if (this.#disposed || this.#context === null || !this.#userEnabled) return;
        if (visible && this.#transportPlaying) await this.#context.resume();
        else await this.#context.suspend();
    }

    async setTransportPlaying(playing: boolean): Promise<void> {
        if (this.#transportPlaying === playing) return;
        this.#transportPlaying = playing;
        if (this.#disposed || this.#context === null || !this.#userEnabled) return;
        if (playing && pageIsVisible()) await this.#context.resume();
        else await this.#context.suspend();
    }

    update(snapshot: ImmPlaybackSnapshot, listener: ImmTransform, restart = false): void {
        this.#lastSnapshot = snapshot;
        this.#listenerTransform = listener;
        this.#updateListener(listener);
        this.#reconcile(restart);
    }

    async dispose(): Promise<void> {
        if (this.#disposed) return;
        this.#disposed = true;
        this.#stopAll();
        this.#decoded.clear();
        this.#master?.disconnect();
        this.#master = null;
        const context = this.#context;
        this.#context = null;
        if (context !== null && context.state !== "closed") await context.close();
    }

    #reconcile(restart: boolean): void {
        if (this.#disposed || this.#context === null || this.#lastSnapshot === null) return;
        if (restart) this.#stopAll();
        if (!this.#userEnabled) {
            this.#stopAll();
            return;
        }
        for (const [layerId, decoded] of this.#decoded) {
            const state = this.#lastSnapshot.layers.get(layerId);
            let active = this.#active.get(layerId);
            if (state === undefined || decoded.sound.type === IMM_SOUND_AMBISONIC ||
                !shouldPlaySound(decoded.sound, state.visible, state.layer.keys)) {
                if (active !== undefined) this.#stop(layerId, active);
                continue;
            }
            if (active === undefined) {
                active = this.#start(layerId, decoded, state.localTimeTicks / this.document.ticksPerSecond);
                if (active === undefined) continue;
            }
            const spatialGain = decoded.sound.type === IMM_SOUND_POSITIONAL
                ? computeSpatialGain(decoded.sound, state.worldTransform, this.#listenerTransform?.translation ?? [0, 0, 0])
                : 1;
            setAudioParam(active.gain.gain, decoded.sound.gain * state.opacity * spatialGain, this.#context.currentTime);
            if (active.panner !== undefined) updatePanner(active.panner, state.worldTransform, this.#context.currentTime);
        }
    }

    #start(layerId: number, decoded: DecodedSound, offsetSeconds: number): ActiveSound | undefined {
        if (this.#context === null || this.#master === null || decoded.sound.type === IMM_SOUND_AMBISONIC) return undefined;
        const duration = decoded.buffer.duration;
        const offset = decoded.sound.looping && duration > 0
            ? ((offsetSeconds % duration) + duration) % duration
            : Math.max(0, offsetSeconds);
        if (!decoded.sound.looping && offset >= duration) return undefined;
        const source = this.#context.createBufferSource();
        const gain = this.#context.createGain();
        source.buffer = decoded.buffer;
        source.loop = decoded.sound.looping;
        let panner: PannerNode | undefined;
        if (decoded.sound.type === IMM_SOUND_POSITIONAL) {
            panner = this.#context.createPanner();
            panner.panningModel = "HRTF";
            panner.distanceModel = "inverse";
            panner.refDistance = 1;
            panner.maxDistance = 100_000;
            panner.rolloffFactor = 0;
            panner.coneInnerAngle = 360;
            panner.coneOuterAngle = 360;
            panner.coneOuterGain = 1;
            source.connect(panner).connect(gain);
        } else {
            source.connect(gain);
        }
        gain.connect(this.#master);
        const active = { source, gain, panner };
        this.#active.set(layerId, active);
        this.#sourceStarts++;
        this.#lastStartOffsets.set(layerId, offset);
        source.addEventListener("ended", () => {
            if (this.#active.get(layerId)?.source === source) {
                this.#active.delete(layerId);
                gain.disconnect();
                panner?.disconnect();
            }
        }, { once: true });
        source.start(0, offset);
        return active;
    }

    #stop(layerId: number, active: ActiveSound): void {
        this.#active.delete(layerId);
        active.source.stop();
        active.source.disconnect();
        active.panner?.disconnect();
        active.gain.disconnect();
    }

    #stopAll(): void {
        for (const [layerId, active] of this.#active) this.#stop(layerId, active);
    }

    #setMasterGain(value: number): void {
        if (this.#context !== null && this.#master !== null) {
            setAudioParam(this.#master.gain, value, this.#context.currentTime);
        }
    }

    #updateListener(transform: ImmTransform): void {
        if (this.#context === null) return;
        listenerPosition.fromArray(transform.translation);
        listenerDirection.copy(forward).applyQuaternion(new THREE.Quaternion().fromArray(transform.rotation)).normalize();
        listenerUp.copy(up).applyQuaternion(new THREE.Quaternion().fromArray(transform.rotation)).normalize();
        const audioListener = this.#context.listener;
        setAudioParam(audioListener.positionX, listenerPosition.x, this.#context.currentTime);
        setAudioParam(audioListener.positionY, listenerPosition.y, this.#context.currentTime);
        setAudioParam(audioListener.positionZ, listenerPosition.z, this.#context.currentTime);
        setAudioParam(audioListener.forwardX, listenerDirection.x, this.#context.currentTime);
        setAudioParam(audioListener.forwardY, listenerDirection.y, this.#context.currentTime);
        setAudioParam(audioListener.forwardZ, listenerDirection.z, this.#context.currentTime);
        setAudioParam(audioListener.upX, listenerUp.x, this.#context.currentTime);
        setAudioParam(audioListener.upY, listenerUp.y, this.#context.currentTime);
        setAudioParam(audioListener.upZ, listenerUp.z, this.#context.currentTime);
    }
}

function shouldPlaySound(
    sound: ImmSound | undefined,
    visible: boolean,
    keys: ReadonlyArray<{ property: number }>,
): boolean {
    if (sound === undefined || !visible) return false;
    return keys.some((key) => key.property === IMM_ANIM_VISIBILITY) || sound.playOnLoad;
}

export function computeSpatialGain(
    sound: ImmSound,
    transform: ImmTransform,
    listener: readonly [number, number, number],
): number {
    sourcePosition.fromArray(transform.translation);
    listenerPosition.fromArray(listener);
    const distance = sourcePosition.distanceTo(listenerPosition);
    return computeDistanceGain(sound, distance, Math.abs(transform.scale)) *
        computeDirectionalGain(sound, transform, listener);
}

export function computeDistanceGain(sound: ImmSound, distance: number, scale = 1): number {
    if (sound.attenuationType === IMM_ATTENUATION_NONE) return 1;
    const minimum = Math.max(0, sound.attenuationMin * scale);
    const maximum = Math.max(minimum, sound.attenuationMax * scale);
    if (distance <= minimum) return 1;
    if (distance >= maximum || maximum <= minimum) return 0;
    if (sound.attenuationType === IMM_ATTENUATION_LINEAR) {
        return 1 - (distance - minimum) / (maximum - minimum);
    }
    if (sound.attenuationType === IMM_ATTENUATION_LOGARITHMIC) {
        const factor = 5 / Math.max(0.001, Math.log2(maximum / (minimum + 0.001)));
        return Math.max(0, Math.min(1, (minimum / Math.max(distance, 0.001)) ** factor));
    }
    return 1;
}

export function computeDirectionalGain(
    sound: ImmSound,
    transform: ImmTransform,
    listener: readonly [number, number, number],
): number {
    if (sound.modifierType === IMM_MODIFIER_NONE) return 1;
    relativePosition.fromArray(listener).sub(sourcePosition.fromArray(transform.translation));
    inverseRotation.fromArray(transform.rotation).invert();
    relativePosition.applyQuaternion(inverseRotation);
    if (transform.flip === 1) relativePosition.x *= -1;
    if (transform.flip === 2) relativePosition.y *= -1;
    if (transform.flip === 3) relativePosition.z *= -1;
    if (relativePosition.lengthSq() === 0) return 1;
    const attenuationOutside = sound.modifierParameters[sound.modifierType === IMM_MODIFIER_CONE ? 2 : 3] ?? 0;
    if (sound.modifierType === IMM_MODIFIER_CONE) {
        const inner = sound.modifierParameters[0] ?? 0;
        const band = sound.modifierParameters[1] ?? 0;
        const forwardDot = Math.max(0, Math.min(1, -relativePosition.normalize().z));
        const attenuation = 1 - smoothstep(inner, inner + band, Math.acos(forwardDot));
        return clamp01(attenuation) * (1 - attenuationOutside) + attenuationOutside;
    }
    if (sound.modifierType === IMM_MODIFIER_FRUSTUM) {
        relativePosition.z *= -1;
        if (relativePosition.z < 0) return 0;
        const innerX = Math.tan(sound.modifierParameters[0] ?? 0);
        const innerY = Math.tan(sound.modifierParameters[1] ?? 0);
        const band = sound.modifierParameters[2] ?? 0;
        const outerX = Math.tan((sound.modifierParameters[0] ?? 0) + band);
        const outerY = Math.tan((sound.modifierParameters[1] ?? 0) + band);
        const projectedX = Math.abs(relativePosition.x / Math.max(relativePosition.z, 1e-12));
        const projectedY = Math.abs(relativePosition.y / Math.max(relativePosition.z, 1e-12));
        const attenuationX = smoothstep(0, outerX - innerX, projectedX - innerX);
        const attenuationY = smoothstep(0, outerY - innerY, projectedY - innerY);
        const attenuation = 1 - Math.max(attenuationX, attenuationY);
        return clamp01(attenuation * attenuation) * (1 - attenuationOutside) + attenuationOutside;
    }
    return 1;
}

function updatePanner(panner: PannerNode, transform: ImmTransform, time: number): void {
    const rotation = new THREE.Quaternion().fromArray(transform.rotation);
    sourcePosition.fromArray(transform.translation);
    sourceDirection.copy(forward);
    sourceUp.copy(up);
    if (transform.flip === 3) sourceDirection.z *= -1;
    if (transform.flip === 2) sourceUp.y *= -1;
    sourceDirection.applyQuaternion(rotation).normalize();
    sourceUp.applyQuaternion(rotation).normalize();
    setAudioParam(panner.positionX, sourcePosition.x, time);
    setAudioParam(panner.positionY, sourcePosition.y, time);
    setAudioParam(panner.positionZ, sourcePosition.z, time);
    setAudioParam(panner.orientationX, sourceDirection.x, time);
    setAudioParam(panner.orientationY, sourceDirection.y, time);
    setAudioParam(panner.orientationZ, sourceDirection.z, time);
}

export function immAudioMimeType(assetFormat: number): string | undefined {
    if (assetFormat === IMM_ASSET_WAV) return "audio/wav";
    if (assetFormat === IMM_ASSET_OGG) return 'audio/ogg; codecs="vorbis"';
    if (assetFormat === IMM_ASSET_OPUS) return 'audio/ogg; codecs="opus"';
    return undefined;
}

function setAudioParam(parameter: AudioParam, value: number, time: number): void {
    parameter.cancelScheduledValues(time);
    parameter.setValueAtTime(Number.isFinite(value) ? value : 0, time);
}

function smoothstep(edge0: number, edge1: number, value: number): number {
    if (edge0 === edge1) return value < edge0 ? 0 : 1;
    const t = clamp01((value - edge0) / (edge1 - edge0));
    return t * t * (3 - 2 * t);
}

function clamp01(value: number): number {
    return Math.max(0, Math.min(1, value));
}

function pageIsVisible(): boolean {
    return typeof document === "undefined" || document.visibilityState !== "hidden";
}

export function cameraAudioTransform(camera: THREE.Camera): ImmTransform {
    camera.updateMatrixWorld(true);
    camera.getWorldPosition(listenerPosition);
    camera.getWorldQuaternion(inverseRotation);
    return {
        rotation: inverseRotation.toArray(),
        scale: 1,
        flip: 0,
        translation: listenerPosition.toArray(),
    };
}
