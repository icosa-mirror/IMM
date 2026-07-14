import { describe, expect, test } from "bun:test";
import { desktopSpawnTransform, IMM_MONO_EYE_HEIGHT_METERS } from "../src/camera-controls";
import type { ImmTransform } from "../src/format/imm-document";

const authored: ImmTransform = {
    rotation: [0, 0, 0, 1],
    scale: 1,
    flip: 0,
    translation: [2, 3, 4],
};

describe("IMM native spawn tracking", () => {
    test("keeps authored eye-level viewpoints unchanged", () => {
        expect(desktopSpawnTransform(authored, "eye")).toBe(authored);
    });

    test("matches native inverse viewer offset for floor-level viewpoints", () => {
        const result = desktopSpawnTransform(authored, "floor");
        expect(result.translation).toEqual([2, 3 - IMM_MONO_EYE_HEIGHT_METERS, 4]);
        expect(authored.translation).toEqual([2, 3, 4]);
    });

    test("applies floor eye height in authored scaled, rotated, and flipped space", () => {
        const halfSqrt = Math.SQRT1_2;
        const result = desktopSpawnTransform({
            rotation: [0, 0, halfSqrt, halfSqrt],
            scale: 2,
            flip: 2,
            translation: [10, 20, 30],
        }, "floor");
        expect(result.translation[0]).toBeCloseTo(10 - 2 * IMM_MONO_EYE_HEIGHT_METERS);
        expect(result.translation[1]).toBeCloseTo(20);
        expect(result.translation[2]).toBeCloseTo(30);
        expect(result.scale).toBe(2);
        expect(result.flip).toBe(2);
    });
});
