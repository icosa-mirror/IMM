import * as THREE from "three";
import type { ImmDocument, ImmTransform } from "../format/imm-document";
import { ImmThreeView } from "../render-three/imm-three-view";
import { ImmPlaybackController, type ImmPlaybackSnapshot } from "../runtime/imm-playback";
import type { StagedLoadWork } from "../staged-loading";
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
export declare class IMMAsset {
    #private;
    readonly scene: THREE.Group;
    readonly document: ImmDocument;
    readonly playback: ImmPlaybackController;
    readonly view: ImmThreeView;
    readonly backgroundComplete: Promise<void>;
    readonly loadTelemetry: IMMLoadTelemetry;
    constructor(document: ImmDocument, session: IMMLoadSession, remainingWork: readonly StagedLoadWork[], loadTelemetry: IMMLoadTelemetry, options: IMMAssetOptions);
    get playing(): boolean;
    get waiting(): boolean;
    get timeSeconds(): number;
    get durationSeconds(): number;
    get chapters(): import("../format/imm-document").ImmChapter[];
    get viewpoints(): import("../format/imm-document").ImmLayer[];
    play(): void;
    pause(): void;
    continue(): void;
    restart(): IMMViewpointPose | undefined;
    seek(seconds: number): IMMViewpointPose | undefined;
    selectChapter(index: number): IMMViewpointPose | undefined;
    selectViewpoint(layerId: number): IMMViewpointPose;
    initialAuthoredCamera(): IMMViewpointPose | undefined;
    update(animationTimeMs: number, camera: THREE.Camera): IMMFrameResult;
    setMuted(muted: boolean): void;
    enableAudio(): Promise<void>;
    dispose(): Promise<void>;
}
export declare function desktopIMMViewpoint(pose: IMMViewpointPose): IMMViewpointPose;
//# sourceMappingURL=imm-asset.d.ts.map