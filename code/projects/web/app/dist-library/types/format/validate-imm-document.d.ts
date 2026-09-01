import { type ImmDocument, type ImmDrawing } from "./imm-document";
interface VersionedSummary {
    schemaVersion: number;
}
interface DrawingDelta {
    type: "drawing";
    layerId: number;
    drawingId: number;
    drawing: ImmDrawing;
    metrics: StagedRequestMetrics;
}
interface AssetDelta {
    type: "asset";
    layerId: number;
    metrics: StagedRequestMetrics;
}
interface StagedRequestMetrics {
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
export type ValidatedStagedDelta = DrawingDelta | AssetDelta;
export declare function assertSupportedSummary(summary: VersionedSummary): void;
export declare function assertSupportedDocument(document: ImmDocument): void;
export declare function assertSupportedDelta(delta: ValidatedStagedDelta): void;
export {};
//# sourceMappingURL=validate-imm-document.d.ts.map