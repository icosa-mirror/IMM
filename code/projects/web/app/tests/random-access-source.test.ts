import { describe, expect, test } from "bun:test";
import {
    ArrayBufferRandomAccessSource,
    FileRandomAccessSource,
} from "../src/random-access-source";

describe("Phase 4.5 random-access sources", () => {
    test("reads exact buffer ranges without exposing the source buffer", async () => {
        const sourceBytes = Uint8Array.from([10, 20, 30, 40, 50]);
        const source = new ArrayBufferRandomAccessSource(sourceBytes);
        const result = new Uint8Array(await source.read(1n, 3, new AbortController().signal));

        expect(source.size).toBe(5n);
        expect([...result]).toEqual([20, 30, 40]);
        result[0] = 99;
        expect(sourceBytes[1]).toBe(20);
    });

    test("uses File.slice for exact local-file ranges", async () => {
        const file = new File([Uint8Array.from([1, 2, 3, 4, 5, 6])], "fixture.imm");
        const source = new FileRandomAccessSource(file);
        const result = new Uint8Array(await source.read(2n, 3, new AbortController().signal));

        expect(source.size).toBe(6n);
        expect([...result]).toEqual([3, 4, 5]);
    });

    test("rejects invalid and out-of-bounds ranges", async () => {
        const source = new ArrayBufferRandomAccessSource(new ArrayBuffer(8));
        const signal = new AbortController().signal;

        expect(source.read(-1n, 1, signal)).rejects.toThrow(RangeError);
        expect(source.read(7n, 2, signal)).rejects.toThrow(RangeError);
        expect(source.read(0n, -1, signal)).rejects.toThrow(RangeError);
        expect(source.read(0n, Number.MAX_SAFE_INTEGER + 1, signal)).rejects.toThrow(RangeError);
    });

    test("honors cancellation before buffer and file reads", async () => {
        const controller = new AbortController();
        controller.abort(new DOMException("cancelled", "AbortError"));

        const bufferSource = new ArrayBufferRandomAccessSource(new ArrayBuffer(4));
        const fileSource = new FileRandomAccessSource(new File([new Uint8Array(4)], "fixture.imm"));
        expect(bufferSource.read(0n, 1, controller.signal)).rejects.toHaveProperty("name", "AbortError");
        expect(fileSource.read(0n, 1, controller.signal)).rejects.toHaveProperty("name", "AbortError");
    });
});
