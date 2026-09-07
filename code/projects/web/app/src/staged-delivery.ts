import type { ImmDecoderClient, ImmStagedDelta } from "./decoder-client";
import { assertSupportedDelta } from "./format/validate-imm-document";
import type { StagedLoadWork } from "./staged-loading";

export type StagedDeliveryMode = "single" | "single-paced" | "batch-paced";

export function createDeliveryMetrics() {
    return { messages: 0, items: 0, requestMs: 0, validationMs: 0, adapterMs: 0,
        yieldMs: 0, yields: 0, packetBytes: 0, nativeMs: 0, transferMs: 0,
        startedAt: 0, completedAt: 0 };
}

export interface StagedDeliveryOptions {
    mode?: StagedDeliveryMode;
    metrics?: ReturnType<typeof createDeliveryMetrics>;
    trace?: boolean;
}

// Time and byte thresholds cannot preempt one indivisible resource.
// Keep one worker response resident and check cancellation after every await.
// Batching/pacing remain opt-in until a repeatable performance benefit is established.
export async function* stagedDelivery(
    decoder: Pick<ImmDecoderClient, "decodeBatch"> & Partial<Pick<ImmDecoderClient, "decodeDrawing" | "decodeLayerAsset">>,
    work: readonly StagedLoadWork[],
    cancelled: () => boolean,
    options: StagedDeliveryOptions = {},
): AsyncGenerator<{ delta: ImmStagedDelta; item: StagedLoadWork; index: number }> {
    const mode = options.mode ?? "single";
    const metrics = options.metrics;
    const timed = (name: string, start: number) => {
        const end = performance.now();
        if (options.trace) {
            performance.measure(`IMM_REBALANCE:${name}`, { start, end });
            performance.clearMeasures(`IMM_REBALANCE:${name}`);
        }
        return end - start;
    };
    if (metrics) metrics.startedAt = performance.now();
    let index = 0;
    let sliceStartedAt = performance.now();
    while (index < work.length && !cancelled()) {
        const requested = work.slice(index, index + (mode === "batch-paced" ? 8 : 1));
        const requestStartedAt = performance.now();
        const item = requested[0]!;
        const deltas = mode === "batch-paced" ? await decoder.decodeBatch(requested)
            : [await (item.type === "drawing"
                ? decoder.decodeDrawing!(item.layerId, item.drawingId)
                : decoder.decodeLayerAsset!(item.layerId))];
        const requestMs = timed("request", requestStartedAt);
        if (metrics) { metrics.messages++; metrics.requestMs += requestMs; }
        if (cancelled()) return;
        if (!Array.isArray(deltas) || deltas.length < 1 || deltas.length > requested.length) {
            throw new Error("Invalid staged batch response length");
        }
        for (const delta of deltas) {
            if (mode !== "single" && performance.now() - sliceStartedAt >= 4) {
                const yieldStartedAt = performance.now();
                await new Promise<void>((resolve) => setTimeout(resolve, 0));
                const yieldMs = timed("yield", yieldStartedAt);
                if (metrics) { metrics.yields++; metrics.yieldMs += yieldMs; }
                if (cancelled()) return;
                sliceStartedAt = performance.now();
            }
            const validationStartedAt = performance.now();
            // Single responses are already validated by the decoder client.
            if (mode === "batch-paced") assertSupportedDelta(delta);
            const validationMs = mode === "batch-paced"
                ? timed("validation", validationStartedAt) : delta.metrics.validationMs ?? 0;
            if (metrics) metrics.validationMs += validationMs;
            const item = work[index]!;
            if (delta.type !== item.type || delta.layerId !== item.layerId
                || (delta.type === "drawing" && item.type === "drawing" && delta.drawingId !== item.drawingId)) {
                throw new Error("Staged batch response does not match requested resource");
            }
            const adapterStartedAt = performance.now();
            yield { delta, item, index };
            const adapterMs = timed("adapter", adapterStartedAt);
            if (cancelled()) return;
            if (metrics) {
                metrics.adapterMs += adapterMs;
                metrics.items++;
                metrics.packetBytes += delta.metrics.packetBytes;
                metrics.nativeMs += delta.metrics.decodeMs + delta.metrics.nativeBuildMs;
                metrics.transferMs += delta.metrics.transferMs;
            }
            index++;
        }
    }
    if (metrics && !cancelled()) metrics.completedAt = performance.now();
}
