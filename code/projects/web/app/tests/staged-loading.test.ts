import { describe, expect, test } from "bun:test";
import type { ImmDocument, ImmLayer } from "../src/format/imm-document";
import { createNativeLoadOrder } from "../src/staged-loading";

const transform = {
    rotation: [0, 0, 0, 1] as [number, number, number, number],
    scale: 1,
    flip: 0,
    translation: [0, 0, 0] as [number, number, number],
};

function layer(overrides: Partial<ImmLayer> & Pick<ImmLayer, "id" | "parentId" | "type">): ImmLayer {
    return {
        name: "",
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
        drawings: [],
        ...overrides,
    };
}

function document(layers: ImmLayer[]): ImmDocument {
    return {
        schemaVersion: 5,
        backgroundColor: [0, 0, 0],
        ticksPerSecond: 100,
        animateOnStart: true,
        durationTicks: 2_000,
        chapters: [],
        layers,
        metrics: { decodeMs: 0, marshalMs: 0, packMs: 0 },
    };
}

describe("native staged load order", () => {
    test("orders drawings by root visibility plus first referenced frame", () => {
        const result = createNativeLoadOrder(document([
            layer({ id: 1, parentId: -1, type: 0 }),
            layer({ id: 2, parentId: 1, type: 0, keys: [{
                property: 0, interpolation: 0, timeTicks: 400, boolValue: true,
                uintValue: 0, floatValue: 0, doubleValue: 0, transformValue: transform,
            }] }),
            layer({
                id: 3,
                parentId: 2,
                type: 1,
                frameRate: 10,
                frameBuffer: new Uint32Array([1, 1, 0]),
                drawings: [
                    { biggestStroke: 0, strokeCount: 0, pointCount: 0, geometries: [] },
                    { biggestStroke: 0, strokeCount: 0, pointCount: 0, geometries: [] },
                ],
            }),
        ]));
        expect(result.map((item) => [item.type, item.layerId, "drawingId" in item ? item.drawingId : -1, item.neededTicks]))
            .toEqual([["drawing", 3, 1, 400], ["drawing", 3, 0, 420]]);
        expect(result.every((item) => item.initial)).toBe(true);
    });

    test("forces spawn assets into the initial set", () => {
        const visibility = [{
            property: 0, interpolation: 0, timeTicks: 900, boolValue: true,
            uintValue: 0, floatValue: 0, doubleValue: 0, transformValue: transform,
        }];
        const result = createNativeLoadOrder(document([
            layer({ id: 1, parentId: -1, type: 0 }),
            layer({ id: 2, parentId: 1, type: 4, keys: visibility }),
            layer({ id: 3, parentId: 1, type: 8, keys: visibility }),
        ]));
        expect(result.find((item) => item.layerId === 2)?.initial).toBe(false);
        expect(result.find((item) => item.layerId === 3)?.initial).toBe(true);
    });
});
