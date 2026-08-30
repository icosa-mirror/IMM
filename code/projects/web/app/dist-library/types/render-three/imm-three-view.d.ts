import * as THREE from "three";
import { type ImmDocument } from "../format/imm-document";
import { type ImmPlaybackSnapshot } from "../runtime/imm-playback";
type ImmCoverageMode = "sample-mask" | "alpha-to-coverage" | "alpha-hash";
export type ImmHostCompatibilityWarningCode = "depth-buffer" | "logarithmic-depth" | "reversed-depth" | "camera-projection" | "camera-near" | "camera-far";
export interface ImmHostCompatibilityWarning {
    code: ImmHostCompatibilityWarningCode;
    message: string;
}
export interface ImmHostDepthContract {
    depthBits: number | null;
    logarithmicDepthBuffer: boolean;
    reversedDepthBuffer: boolean;
}
export interface ImmThreeDiagnostics {
    paintLayerCount: number;
    modelLayerCount: number;
    pictureLayerCount: number;
    meshCount: number;
    triangleCount: number;
    geometryBuildMs: number;
    alphaMode: ImmCoverageMode;
    depthBits: number | null;
    stencilBits: number | null;
    sampleCount: number | null;
    maxSamples: number | null;
    programmableSampleMask: boolean;
    maxTextureSize: number | null;
    colorMode: "srgb-output-no-tone-mapping";
    activeDrawingCount: number;
    hostCompatibilityWarnings: ReadonlyArray<ImmHostCompatibilityWarning>;
}
export interface ImmThreeViewOptions {
    renderer?: THREE.WebGLRenderer;
    parent?: THREE.Object3D;
}
export declare class ImmThreeView {
    #private;
    readonly object3d: THREE.Group<THREE.Object3DEventMap>;
    readonly diagnostics: ImmThreeDiagnostics;
    constructor(document: ImmDocument, options?: ImmThreeViewOptions);
    get timeTicks(): number;
    setTimeSeconds(timeSeconds: number, camera?: THREE.Camera): void;
    setTimeTicks(timeTicks: number, camera?: THREE.Camera): void;
    /** Applies a caller-owned evaluation so multiple consumers can share one frame snapshot. */
    applySnapshot(snapshot: ImmPlaybackSnapshot, camera?: THREE.Camera): void;
    /** Pins stochastic coverage for deterministic captures and host-controlled render sequencing. */
    setCoverageFrame(frame: number): void;
    /** The host owns the renderer and clock; time is an explicit document-relative value. */
    update(timeSeconds: number, camera: THREE.Camera): void;
    /** Uploads newly decoded content without rebuilding already resident layers. */
    refreshLayer(layerId: number, drawingId?: number, camera?: THREE.Camera): void;
    dispose(): void;
}
export declare function validateImmHostCompatibility(camera: THREE.Camera | undefined, depth: ImmHostDepthContract): ImmHostCompatibilityWarning[];
export {};
//# sourceMappingURL=imm-three-view.d.ts.map