import * as THREE from "three";
import { type ImmDocument, type ImmSound, type ImmTransform } from "../format/imm-document";
import type { ImmPlaybackSnapshot } from "../runtime/imm-playback";
export declare const IMM_ATTENUATION_NONE = 0;
export declare const IMM_ATTENUATION_LINEAR = 1;
export declare const IMM_ATTENUATION_LOGARITHMIC = 2;
export declare const IMM_MODIFIER_NONE = 0;
export declare const IMM_MODIFIER_CONE = 1;
export declare const IMM_MODIFIER_FRUSTUM = 2;
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
    timelineClock: "animation-frame" | "audio-context";
    baseLatencySeconds: number | null;
    outputLatencySeconds: number | null;
    driftSampleCount: number;
    maximumAbsoluteDriftSeconds: number;
    currentDrift: ReadonlyArray<{
        layerId: number;
        driftSeconds: number;
    }>;
    lastStartOffsets: ReadonlyArray<{
        layerId: number;
        offsetSeconds: number;
    }>;
    activeTimings: ReadonlyArray<{
        layerId: number;
        contextStartSeconds: number;
        contextTimeSeconds: number;
        actualSeconds: number;
        expectedSeconds: number | null;
        durationSeconds: number;
    }>;
    decodeFailures: ReadonlyArray<{
        layerId: number;
        name: string;
        reason: string;
    }>;
    codecs: ImmAudioCodecCapabilities;
    ambisonicSupported: false;
    unsupportedAmbisonicLayers: number;
}
interface ImmAudioOptions {
    context?: AudioContext;
}
export declare function probeImmAudioCapabilities(): ImmAudioCodecCapabilities;
export declare class ImmWebAudio {
    #private;
    readonly document: ImmDocument;
    readonly codecs: ImmAudioCodecCapabilities;
    constructor(document: ImmDocument, options?: ImmAudioOptions);
    get diagnostics(): ImmAudioDiagnostics;
    prepare(): Promise<void>;
    enable(): Promise<void>;
    setMuted(muted: boolean): void;
    setPageVisible(visible: boolean): Promise<void>;
    setTransportPlaying(playing: boolean): Promise<void>;
    /** Uses Web Audio's monotonic clock while audible sources run so visuals cannot free-run against it. */
    timelineDeltaSeconds(animationFrameDeltaSeconds: number): number;
    update(snapshot: ImmPlaybackSnapshot, listener: ImmTransform, restart?: boolean): void;
    dispose(): Promise<void>;
}
export declare function audioContextTimelineDelta(currentContextTime: number, previousContextTime: number | null, animationFrameDeltaSeconds: number): number;
export declare function computeAudioDriftSeconds(actualSeconds: number, expectedSeconds: number, durationSeconds: number, looping: boolean): number;
export declare function computeSpatialGain(sound: ImmSound, transform: ImmTransform, listener: readonly [number, number, number]): number;
export declare function computeDistanceGain(sound: ImmSound, distance: number, scale?: number): number;
export declare function computeDirectionalGain(sound: ImmSound, transform: ImmTransform, listener: readonly [number, number, number]): number;
export declare function immAudioMimeType(assetFormat: number): string | undefined;
export declare function cameraAudioTransform(camera: THREE.Camera): ImmTransform;
export {};
//# sourceMappingURL=imm-web-audio.d.ts.map