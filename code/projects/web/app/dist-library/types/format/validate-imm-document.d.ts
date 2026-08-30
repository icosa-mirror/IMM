import { type ImmDocument, type ImmDrawing } from "./imm-document";
interface VersionedSummary {
    schemaVersion: number;
}
interface DrawingDelta {
    type: "drawing";
    layerId: number;
    drawingId: number;
    drawing: ImmDrawing;
}
interface AssetDelta {
    type: "asset";
    layerId: number;
}
export type ValidatedStagedDelta = DrawingDelta | AssetDelta;
export declare function assertSupportedSummary(summary: VersionedSummary): void;
export declare function assertSupportedDocument(document: ImmDocument): void;
export declare function assertSupportedDelta(delta: ValidatedStagedDelta): void;
export {};
//# sourceMappingURL=validate-imm-document.d.ts.map