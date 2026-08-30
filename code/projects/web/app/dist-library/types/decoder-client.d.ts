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
    fallbackEager(): Promise<ImmDocument>;
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
} | {
    type: "asset";
    layerId: number;
    picture?: ImmPicture;
    sound?: ImmSound;
};
export interface ImmDecoderDiagnostics {
    peakWasmHeapBytes: number;
    peakPaintPacketBytes: number;
    geometryTransferBytes: number;
}
//# sourceMappingURL=decoder-client.d.ts.map