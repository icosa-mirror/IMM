import { describe, expect, test } from "bun:test";
import { stagedDelivery, createDeliveryMetrics } from "../src/staged-delivery";
import type { StagedLoadWork } from "../src/staged-loading";
import type { ImmStagedDelta } from "../src/decoder-client";

const work: StagedLoadWork[] = Array.from({ length: 19 }, (_, layerId) => ({
    type: "asset", layerId, neededTicks: 0, initial: false,
}));
const delta = (layerId: number): ImmStagedDelta => ({
    type: "asset", layerId,
    metrics: { type: "asset", layerId, decodeMs: 0, nativeBuildMs: 0, wasmCopyMs: 0,
        packetParseMs: 0, transferMs: 0, adapterMs: 0, lookupMs: 0, packetBytes: 0 },
});

describe("bounded staged delivery", () => {
    test("resumes partial batches in order without skipping resources", async () => {
        const requests: number[][] = [];
        const delivered: number[] = [];
        const decoder = { async decodeBatch(items: readonly StagedLoadWork[]) {
            requests.push(items.map(item => item.layerId));
            return items.slice(0, 3).map(item => delta(item.layerId));
        } };
        for await (const result of stagedDelivery(decoder, work, () => false, { mode: "batch-paced" })) {
            delivered.push(result.delta.layerId);
            expect(result.index).toBe(delivered.length - 1);
        }
        expect(delivered).toEqual(work.map(item => item.layerId));
        expect(requests.every(items => items.length <= 8)).toBe(true);
        expect(requests.length).toBe(7);
    });

    test("cancellation after response prevents any delivery", async () => {
        let cancelled = false;
        const decoder = { async decodeBatch() { cancelled = true; return [delta(0)]; } };
        const results = [];
        for await (const result of stagedDelivery(decoder, work, () => cancelled, { mode: "batch-paced" })) results.push(result);
        expect(results).toEqual([]);
    });

    test("cancellation during adapter work discards the rest of the batch", async () => {
        let cancelled = false;
        let calls = 0;
        const decoder = { async decodeBatch(items: readonly StagedLoadWork[]) {
            calls++;
            return items.map(item => delta(item.layerId));
        } };
        const results = [];
        for await (const result of stagedDelivery(decoder, work, () => cancelled, { mode: "batch-paced" })) {
            results.push(result);
            cancelled = true;
        }
        expect(results.length).toBe(1);
        expect(calls).toBe(1);
    });

    test("rejects empty, oversized and mismatched batches", async () => {
        for (const response of [[], Array.from({length: 9}, () => delta(0)), [delta(999)]]) {
            const run = async () => {
                for await (const _ of stagedDelivery({ async decodeBatch() { return response; } }, work, () => false, { mode: "batch-paced" })) {}
            };
            await expect(run()).rejects.toThrow();
        }
    });

    test("yields to a browser task when adapter work exhausts the slice", async () => {
        let taskRan = false;
        let delivered = 0;
        const decoder = { async decodeBatch(items: readonly StagedLoadWork[]) {
            return items.map(item => delta(item.layerId));
        } };
        for await (const _ of stagedDelivery(decoder, work.slice(0, 2), () => false, { mode: "batch-paced" })) {
            if (delivered++ === 0) {
                setTimeout(() => { taskRan = true; }, 0);
                const start = performance.now();
                while (performance.now() - start < 5) { /* simulate adapter work */ }
            } else expect(taskRan).toBe(true);
        }
    });
});

describe("controlled delivery modes", () => {
    for (const mode of ["single", "single-paced", "batch-paced"] as const) {
        test(`${mode} delivers identical work and counts messages once`, async () => {
            const calls: string[] = [];
            const decoder = {
                async decodeBatch(items: readonly StagedLoadWork[]) {
                    calls.push("batch");
                    return items.map(item => delta(item.layerId));
                },
                async decodeLayerAsset(layerId: number) { calls.push("single"); return delta(layerId); },
            };
            const metrics = createDeliveryMetrics();
            const delivered: number[] = [];
            for await (const result of stagedDelivery(decoder, work, () => false, {mode, metrics})) {
                delivered.push(result.delta.layerId);
            }
            expect(delivered).toEqual(work.map(item => item.layerId));
            expect(metrics.items).toBe(work.length);
            expect(metrics.messages).toBe(mode === "batch-paced" ? 3 : 19);
            expect(calls.every(call => call === (mode === "batch-paced" ? "batch" : "single"))).toBe(true);
            expect(metrics.completedAt).toBeGreaterThanOrEqual(metrics.startedAt);
            if (mode === "single") expect(metrics.yields).toBe(0);
        });
    }
});

test("production default uses bounded batches and delivers all resources", async () => {
    const metrics = createDeliveryMetrics();
    const requestedSizes: number[] = [];
    const decoder = {
        async decodeBatch(requested: readonly StagedLoadWork[]): Promise<ImmStagedDelta[]> {
            requestedSizes.push(requested.length);
            return requested.map(item => delta(item.layerId));
        },
        async decodeLayerAsset(): Promise<ImmStagedDelta> { throw new Error("Unexpected single request"); },
    };
    for await (const _ of stagedDelivery(decoder, work, () => false, { metrics })) {}
    expect(metrics.messages).toBe(Math.ceil(work.length / 8));
    expect(metrics.items).toBe(work.length);
    expect(requestedSizes.every(size => size > 0 && size <= 8)).toBe(true);
});
