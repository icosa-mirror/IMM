import { describe, expect, test } from "bun:test";
import * as THREE from "three";
import { validateImmHostCompatibility } from "../src/render-three/imm-three-view";

describe("IMM embedded host compatibility", () => {
    test("accepts the native perspective and D24 linear-depth contract", () => {
        const camera = new THREE.PerspectiveCamera(70, 16 / 9, 0.01, 20_000);
        expect(validateImmHostCompatibility(camera, {
            depthBits: 24,
            logarithmicDepthBuffer: false,
            reversedDepthBuffer: false,
        })).toEqual([]);
    });

    test("reports every incompatible camera and depth setting without changing them", () => {
        const camera = new THREE.PerspectiveCamera(70, 16 / 9, 0.1, 1_000);
        const warnings = validateImmHostCompatibility(camera, {
            depthBits: 16,
            logarithmicDepthBuffer: true,
            reversedDepthBuffer: true,
        });
        expect(warnings.map((warning) => warning.code)).toEqual([
            "depth-buffer",
            "logarithmic-depth",
            "reversed-depth",
            "camera-near",
            "camera-far",
        ]);
        expect(camera.near).toBe(0.1);
        expect(camera.far).toBe(1_000);
    });

    test("reports a non-perspective camera", () => {
        const camera = new THREE.OrthographicCamera(-1, 1, 1, -1, 0.01, 20_000);
        expect(validateImmHostCompatibility(camera, {
            depthBits: 24,
            logarithmicDepthBuffer: false,
            reversedDepthBuffer: false,
        }).map((warning) => warning.code)).toEqual(["camera-projection"]);
    });
});
