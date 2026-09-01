export interface ImmDocumentSummary {
    schemaVersion: number;
    formatVersion: number;
    sourceSize: bigint;
    chunkCount: number;
    chunkFlags: number;
    sequenceType: number;
    sequenceCapabilities: number;
    sequenceOffset: bigint;
    sequenceSize: bigint;
    resourceTableOffset: bigint;
    resourceTableSize: bigint;
    assetCount: number;
}
export declare class ImmDecoderClient {
    #private;
    constructor(workerUrl?: string);
    inspect(source: ArrayBuffer): Promise<ImmDocumentSummary>;
    decode(source: ArrayBuffer): Promise<ImmDocument>;
    openMetadata(source: ArrayBuffer): Promise<ImmDocument>;
    decodeDrawing(layerId: number, drawingId: number): Promise<ImmStagedDelta>;
    decodeLayerAsset(layerId: number): Promise<ImmStagedDelta>;
    fallbackEager(reason: string): Promise<ImmDocument>;
    diagnostics(): Promise<ImmDecoderDiagnostics>;
    release(): Promise<void>;
    dispose(): void;
}
import type { ImmDocument, ImmDrawing, ImmPicture, ImmSound } from "./format/imm-document";
export type ImmStagedDelta = {
    type: "drawing";
    layerId: number;
    drawingId: number;
    drawing: ImmDrawing;
    metrics: ImmStagedRequestMetrics;
} | {
    type: "asset";
    layerId: number;
    picture?: ImmPicture;
    sound?: ImmSound;
    metrics: ImmStagedRequestMetrics;
};
export interface ImmStagedRequestMetrics {
    type: "drawing" | "asset";
    layerId: number;
    drawingId?: number;
    decodeMs: number;
    nativeBuildMs: number;
    wasmCopyMs: number;
    packetParseMs: number;
    transferMs: number;
    adapterMs: number;
    lookupMs: number;
    packetBytes: number;
}
export interface ImmDecoderDiagnostics {
    peakWasmHeapBytes: number;
    peakPaintPacketBytes: number;
    geometryTransferBytes: number;
    requestedLoadMode: "eager" | "staged";
    effectiveLoadMode: "eager" | "staged";
    fallbackReason: string | null;
    stagedRequests: ImmStagedRequestMetrics[];
}
//# sourceMappingURL=decoder-client.d.ts.map