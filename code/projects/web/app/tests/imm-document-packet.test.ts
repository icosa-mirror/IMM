import { describe, expect, test } from "bun:test";
import type { ImmDocument, ImmDrawing, ImmPaintGeometry } from "../src/format/imm-document";
import {
    assertSupportedDelta,
    assertSupportedDocument,
    assertSupportedSummary,
} from "../src/format/validate-imm-document";

function geometry(overrides: Partial<ImmPaintGeometry> = {}): ImmPaintGeometry {
    return {
        brushType: 0,
        triangleCount: 1,
        positions: new Float32Array(9),
        colors: new Float32Array(12),
        directions: new Float32Array(9),
        visibility: new Uint8Array(3),
        masks: new Uint8Array(3),
        progress: new Float32Array(3),
        indices: new Uint16Array([0, 1, 2]),
        ...overrides,
    };
}

function drawing(overrides: Partial<ImmDrawing> = {}): ImmDrawing {
    return {
        biggestStroke: 1,
        strokeCount: 1,
        pointCount: 2,
        geometries: [geometry()],
        ...overrides,
    };
}

function document(schemaVersion = 5): ImmDocument {
    const transform = {
        rotation: [0, 0, 0, 1] as [number, number, number, number],
        scale: 1,
        flip: 0,
        translation: [0, 0, 0] as [number, number, number],
    };
    return {
        schemaVersion,
        backgroundColor: [0, 0, 0],
        ticksPerSecond: 12600,
        animateOnStart: false,
        durationTicks: 0,
        chapters: [],
        metrics: { decodeMs: 0, marshalMs: 0, packMs: 0 },
        layers: [{
            id: 1,
            parentId: -1,
            type: 1,
            name: "paint",
            visible: true,
            isTimeline: false,
            opacity: 1,
            defaultSpawn: false,
            localTransform: transform,
            worldTransform: transform,
            pivotTransform: transform,
            frameRate: 0,
            frameCount: 0,
            maxRepeatCount: 0,
            durationTicks: 0,
            keys: [],
            frameBuffer: new Uint32Array(),
            drawings: [drawing()],
        }],
    };
}

describe("IMM decoder packet validation", () => {
    test("accepts the current schema and a well-formed paint packet", () => {
        expect(() => assertSupportedSummary({ schemaVersion: 5 })).not.toThrow();
        expect(() => assertSupportedDocument(document())).not.toThrow();
        expect(() => assertSupportedDelta({ type: "drawing", layerId: 1, drawingId: 0, drawing: drawing() }))
            .not.toThrow();
    });

    test("rejects unsupported summary and document schemas explicitly", () => {
        expect(() => assertSupportedSummary({ schemaVersion: 4 })).toThrow("schema 4 is unsupported; expected 5");
        expect(() => assertSupportedDocument(document(6))).toThrow("schema 6 is unsupported; expected 5");
    });

    test("rejects mismatched attribute lengths", () => {
        const malformed = drawing({ geometries: [geometry({ colors: new Float32Array(8) })] });
        expect(() => assertSupportedDelta({ type: "drawing", layerId: 1, drawingId: 0, drawing: malformed }))
            .toThrow("colors length 8 does not match expected length 12");
    });

    test("rejects incomplete triangles and out-of-range indices", () => {
        expect(() => assertSupportedDelta({
            type: "drawing", layerId: 1, drawingId: 0,
            drawing: drawing({ geometries: [geometry({ indices: new Uint16Array([0, 1]) })] }),
        })).toThrow("indices must contain complete triangles");
        expect(() => assertSupportedDelta({
            type: "drawing", layerId: 1, drawingId: 0,
            drawing: drawing({ geometries: [geometry({ indices: new Uint16Array([0, 1, 3]) })] }),
        })).toThrow("index 3 exceeds vertex count 3");
    });

    test("rejects duplicate and unsupported brush records", () => {
        expect(() => assertSupportedDelta({
            type: "drawing", layerId: 1, drawingId: 0,
            drawing: drawing({ geometries: [geometry(), geometry()] }),
        })).toThrow("repeats brush type 0");
        expect(() => assertSupportedDelta({
            type: "drawing", layerId: 1, drawingId: 0,
            drawing: drawing({ geometries: [geometry({ brushType: 5 })] }),
        })).toThrow("brushType must be an integer from 0 through 4");
    });
});
