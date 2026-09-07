import { type ImmDocument, type ImmLayer, type ImmTransform } from "../format/imm-document";
export interface ImmEvaluatedLayer {
    layer: ImmLayer;
    timelineTicks: number;
    localTimeTicks: number;
    visible: boolean;
    opacity: number;
    transform: ImmTransform;
    worldTransform: ImmTransform;
    drawInTime: number;
    drawingIndex: number;
}
export interface ImmPlaybackSnapshot {
    timeTicks: number;
    chapterIndex: number;
    waiting: boolean;
    layers: ReadonlyMap<number, ImmEvaluatedLayer>;
}
export declare class ImmPlaybackController {
    #private;
    readonly document: ImmDocument;
    timeTicks: number;
    playing: boolean;
    waiting: boolean;
    playbackRate: number;
    constructor(document: ImmDocument);
    get durationTicks(): number;
    get chapterIndex(): number;
    play(): void;
    pause(): void;
    wait(): void;
    continue(): void;
    restart(): void;
    seekTicks(timeTicks: number): void;
    seekSeconds(timeSeconds: number): void;
    selectChapter(index: number): void;
    skipForward(): void;
    skipBack(): void;
    advance(deltaSeconds: number): ImmPlaybackSnapshot;
    /** Advances transport and returns a borrowed frame from the supplied evaluator. */
    advanceFrame(deltaSeconds: number, evaluator: ImmFrameEvaluator): ImmPlaybackSnapshot;
    evaluate(): ImmPlaybackSnapshot;
}
/**
 * Indexed metadata and reusable frame containers. evaluateFrame() returns borrowed
 * state that is overwritten by the next call. Use evaluateImmDocument() when a
 * snapshot must survive later evaluations. Call rebuild() after editing hierarchy
 * or animation-key structure; staged drawing/picture/sound replacement is supported.
 */
export declare class ImmFrameEvaluator {
    #private;
    readonly document: ImmDocument;
    constructor(document: ImmDocument);
    rebuild(): void;
    evaluateFrame(ticks: number, offsets?: ReadonlyMap<number, number>, waiting?: boolean): ImmPlaybackSnapshot;
}
export declare function evaluateImmDocument(document: ImmDocument, requestedTicks: number, timelineOffsets?: ReadonlyMap<number, number>, waiting?: boolean): ImmPlaybackSnapshot;
/** Resolves the authored spawn area at a playback time, including timed MakeDefault actions. */
export interface ImmActiveSpawnArea {
    state: ImmEvaluatedLayer;
    actionTimeTicks: number | null;
}
export declare function resolveActiveSpawnArea(document: ImmDocument, requestedTicks: number, snapshot?: ImmPlaybackSnapshot): ImmActiveSpawnArea | undefined;
//# sourceMappingURL=imm-playback.d.ts.map