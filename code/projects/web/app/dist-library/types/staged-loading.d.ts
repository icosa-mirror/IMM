import { type ImmDocument } from "./format/imm-document";
export type StagedLoadWork = {
    type: "drawing";
    layerId: number;
    drawingId: number;
    neededTicks: number;
    initial: boolean;
} | {
    type: "asset";
    layerId: number;
    neededTicks: number;
    initial: boolean;
};
export declare function createNativeLoadOrder(document: ImmDocument, bufferSeconds?: number): StagedLoadWork[];
//# sourceMappingURL=staged-loading.d.ts.map