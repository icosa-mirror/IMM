import type { ImmDecoderClient, ImmStagedDelta } from "./decoder-client";
import type { StagedLoadWork } from "./staged-loading";
export type StagedDeliveryMode = "single" | "single-paced" | "batch-paced";
export declare function createDeliveryMetrics(): {
    messages: number;
    items: number;
    requestMs: number;
    validationMs: number;
    adapterMs: number;
    yieldMs: number;
    yields: number;
    packetBytes: number;
    nativeMs: number;
    transferMs: number;
    startedAt: number;
    completedAt: number;
};
export interface StagedDeliveryOptions {
    mode?: StagedDeliveryMode;
    metrics?: ReturnType<typeof createDeliveryMetrics>;
    trace?: boolean;
}
export declare function stagedDelivery(decoder: Pick<ImmDecoderClient, "decodeBatch"> & Partial<Pick<ImmDecoderClient, "decodeDrawing" | "decodeLayerAsset">>, work: readonly StagedLoadWork[], cancelled: () => boolean, options?: StagedDeliveryOptions): AsyncGenerator<{
    delta: ImmStagedDelta;
    item: StagedLoadWork;
    index: number;
}>;
//# sourceMappingURL=staged-delivery.d.ts.map