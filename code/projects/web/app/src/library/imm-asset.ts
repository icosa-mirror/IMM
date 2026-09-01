import * as THREE from "three";
import { cameraAudioTransform, ImmWebAudio } from "../audio/imm-web-audio";
import { desktopSpawnTransform } from "../desktop-spawn-transform";
import type { ImmDocument, ImmTransform } from "../format/imm-document";
import { ImmThreeView } from "../render-three/imm-three-view";
import {
    ImmPlaybackController,
    resolveActiveSpawnArea,
    type ImmActiveSpawnArea,
    type ImmPlaybackSnapshot,
} from "../runtime/imm-playback";
import type { StagedLoadWork } from "../staged-loading";
import type { ImmStagedDelta } from "../decoder-client";
import { IMMLoadSession, type IMMLoadTelemetry } from "./imm-load-session";

export interface IMMViewpointPose {
    layerId: number;
    name: string;
    tracking: "eye" | "floor" | null;
    transform: ImmTransform;
    activationKey: string;
}

export interface IMMFrameResult {
    snapshot: ImmPlaybackSnapshot;
    authoredCamera?: IMMViewpointPose;
}

export interface IMMAssetOptions {
    renderer: THREE.WebGLRenderer;
    audio?: boolean;
    audioContext?: AudioContext;
    onBackgroundError?: (error: unknown) => void;
}

export class IMMAsset {
    readonly scene: THREE.Group;
    readonly document: ImmDocument;
    readonly playback: ImmPlaybackController;
    readonly view: ImmThreeView;
    readonly backgroundComplete: Promise<void>;
    readonly loadTelemetry: IMMLoadTelemetry;

    #session: IMMLoadSession | null;
    #audio: ImmWebAudio | null;
    #previousAnimationTime: number | null = null;
    #cameraActivationKey = "";
    #disposed = false;

    constructor(
        document: ImmDocument,
        session: IMMLoadSession,
        remainingWork: readonly StagedLoadWork[],
        loadTelemetry: IMMLoadTelemetry,
        options: IMMAssetOptions,
    ) {
        this.document = document;
        this.loadTelemetry = loadTelemetry;
        this.#session = session;
        this.view = new ImmThreeView(document, { renderer: options.renderer });
        this.scene = this.view.object3d;
        this.scene.name = "IMM content";
        this.playback = new ImmPlaybackController(document);
        this.playback.play();
        this.#audio = options.audio === false ? null : new ImmWebAudio(document, { context: options.audioContext });
        void this.#audio?.prepare();
        this.backgroundComplete = remainingWork.length === 0
            ? Promise.resolve()
            : session.continue(document, remainingWork, (delta, item) => this.#applyDelta(delta, item))
                .catch((error) => {
                    options.onBackgroundError?.(error);
                    throw error;
                });
        const releaseSession = () => {
            if (this.#session === session) {
                session.dispose();
                this.#session = null;
            }
        };
        void this.backgroundComplete.then(releaseSession, releaseSession);
    }

    get playing(): boolean { return this.playback.playing; }
    get waiting(): boolean { return this.playback.waiting; }
    get timeSeconds(): number { return this.playback.timeTicks / this.document.ticksPerSecond; }
    get durationSeconds(): number { return this.playback.durationTicks / this.document.ticksPerSecond; }
    get chapters() { return this.document.chapters; }
    get viewpoints() { return this.document.layers.filter((layer) => layer.type === 8); }

    play(): void {
        this.#throwIfDisposed();
        this.playback.play();
        void this.#audio?.setTransportPlaying(true);
    }

    pause(): void {
        this.#throwIfDisposed();
        this.playback.pause();
        void this.#audio?.setTransportPlaying(false);
    }

    continue(): void {
        this.#throwIfDisposed();
        this.playback.continue();
        void this.#audio?.setTransportPlaying(true);
    }

    restart(): IMMViewpointPose | undefined {
        this.#throwIfDisposed();
        this.playback.restart();
        this.#previousAnimationTime = null;
        return this.#resolveAuthoredCamera(true);
    }

    seek(seconds: number): IMMViewpointPose | undefined {
        this.#throwIfDisposed();
        this.playback.seekSeconds(seconds);
        return this.#resolveAuthoredCamera(true);
    }

    selectChapter(index: number): IMMViewpointPose | undefined {
        this.#throwIfDisposed();
        this.playback.selectChapter(index);
        return this.#resolveAuthoredCamera(true);
    }

    selectViewpoint(layerId: number): IMMViewpointPose {
        this.#throwIfDisposed();
        const state = this.playback.evaluate().layers.get(layerId);
        if (state === undefined || state.layer.type !== 8) throw new RangeError(`IMM viewpoint ${layerId} does not exist`);
        const pose = viewpointPose(state.layer.id, state.layer.name, state.layer.spawnTracking, state.worldTransform,
            `selected:${state.layer.id}:${this.playback.chapterIndex}`);
        this.#cameraActivationKey = pose.activationKey;
        return pose;
    }

    initialAuthoredCamera(): IMMViewpointPose | undefined {
        this.#throwIfDisposed();
        return this.#resolveAuthoredCamera(true);
    }

    update(animationTimeMs: number, camera: THREE.Camera): IMMFrameResult {
        this.#throwIfDisposed();
        const rawDelta = this.#previousAnimationTime === null ? 0 : (animationTimeMs - this.#previousAnimationTime) / 1_000;
        this.#previousAnimationTime = animationTimeMs;
        const delta = this.#audio?.timelineDeltaSeconds(THREE.MathUtils.clamp(rawDelta, 0, 0.1)) ??
            THREE.MathUtils.clamp(rawDelta, 0, 0.1);
        const snapshot = this.playback.advance(delta);
        this.view.applySnapshot(snapshot, camera);
        this.#audio?.update(snapshot, cameraAudioTransform(camera));
        void this.#audio?.setTransportPlaying(this.playback.playing);
        return { snapshot, authoredCamera: this.#resolveAuthoredCamera(false, snapshot) };
    }

    setMuted(muted: boolean): void { this.#audio?.setMuted(muted); }
    async enableAudio(): Promise<void> { await this.#audio?.enable(); }

    async dispose(): Promise<void> {
        if (this.#disposed) return;
        this.#disposed = true;
        this.#session?.dispose();
        this.#session = null;
        this.view.dispose();
        await this.#audio?.dispose();
        this.#audio = null;
    }

    async #applyDelta(delta: ImmStagedDelta, item: StagedLoadWork): Promise<void> {
        if (this.#disposed) return;
        const layer = this.document.layers.find((candidate) => candidate.id === item.layerId);
        if (item.type === "drawing" || layer?.type === 3 || layer?.type === 4) {
            this.view.refreshLayer(delta.layerId, delta.type === "drawing" ? delta.drawingId : undefined);
        }
        if (layer?.type === 5 && this.#audio !== null) {
            await this.#audio.dispose();
            this.#audio = new ImmWebAudio(this.document);
            await this.#audio.prepare();
        }
    }

    #resolveAuthoredCamera(
        force: boolean,
        snapshot = this.playback.evaluate(),
    ): IMMViewpointPose | undefined {
        const active = resolveActiveSpawnArea(this.document, this.playback.timeTicks, snapshot);
        if (active === undefined) return undefined;
        const activationKey = spawnActivationKey(active, this.playback.chapterIndex);
        if (!force && activationKey === this.#cameraActivationKey) return undefined;
        this.#cameraActivationKey = activationKey;
        return viewpointPose(
            active.state.layer.id,
            active.state.layer.name,
            active.state.layer.spawnTracking,
            active.state.worldTransform,
            activationKey,
        );
    }

    #throwIfDisposed(): void {
        if (this.#disposed) throw new Error("IMMAsset is disposed");
    }
}

export function desktopIMMViewpoint(pose: IMMViewpointPose): IMMViewpointPose {
    return { ...pose, transform: desktopSpawnTransform(pose.transform, pose.tracking) };
}

function spawnActivationKey(active: ImmActiveSpawnArea, chapterIndex: number): string {
    return `${active.state.layer.id}:${active.actionTimeTicks ?? "initial"}:${chapterIndex}`;
}

function viewpointPose(
    layerId: number,
    name: string,
    tracking: "eye" | "floor" | null | undefined,
    transform: ImmTransform,
    activationKey: string,
): IMMViewpointPose {
    return {
        layerId,
        name,
        tracking: tracking ?? null,
        transform: {
            rotation: [...transform.rotation],
            translation: [...transform.translation],
            scale: transform.scale,
            flip: transform.flip,
        },
        activationKey,
    };
}
