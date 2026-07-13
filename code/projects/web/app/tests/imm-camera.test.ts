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

    test("adds native mono eye height to floor-level viewpoints", () => {
        const result = desktopSpawnTransform(authored, "floor");
        expect(result.translation).toEqual([2, 3 + IMM_MONO_EYE_HEIGHT_METERS, 4]);
        expect(authored.translation).toEqual([2, 3, 4]);
    });
});
