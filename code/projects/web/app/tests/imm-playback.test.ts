import { describe, expect, test } from "bun:test";
import {
    IMM_ACTION_PLAY,
    IMM_ACTION_LOOP,
    IMM_ACTION_MAKE_DEFAULT,
    IMM_ACTION_STOP,
    IMM_ANIM_ACTION,
    IMM_ANIM_OFFSET,
    IMM_ANIM_OPACITY,
    IMM_ANIM_TRANSFORM,
    IMM_ANIM_VISIBILITY,
    IMM_INTERPOLATION_LINEAR,
    IMM_INTERPOLATION_NONE,
    IMM_INTERPOLATION_SMOOTHSTEP,
    IMM_INTERPOLATION_EASE_IN,
    IMM_INTERPOLATION_EASE_OUT,
    type ImmAnimationKey,
    type ImmDocument,
    type ImmLayer,
    type ImmTransform,
} from "../src/format/imm-document";
import { evaluateImmDocument, ImmPlaybackController, resolveActiveSpawnArea } from "../src/runtime/imm-playback";

const identity: ImmTransform = {
    rotation: [0, 0, 0, 1],
    scale: 1,
    flip: 0,
    translation: [0, 0, 0],
};

function key(property: number, timeTicks: number, values: Partial<ImmAnimationKey>): ImmAnimationKey {
    return {
        property,
        interpolation: IMM_INTERPOLATION_NONE,
        timeTicks,
        boolValue: false,
        uintValue: 0,
        floatValue: 0,
        doubleValue: 0,
        transformValue: identity,
        ...values,
    };
}

function layer(values: Partial<ImmLayer> & Pick<ImmLayer, "id" | "parentId" | "type">): ImmLayer {
    return {
        id: values.id,
        parentId: values.parentId,
        type: values.type,
        name: `layer-${values.id}`,
        visible: true,
        isTimeline: false,
        opacity: 1,
        defaultSpawn: false,
        localTransform: identity,
        worldTransform: identity,
        pivotTransform: identity,
        frameRate: 0,
        frameCount: 0,
        maxRepeatCount: 1,
        durationTicks: 0,
        keys: [],
        frameBuffer: new Uint32Array(),
        drawings: [],
        ...values,
    };
}

function fixture(): ImmDocument {
    const root = layer({
        id: 0,
        parentId: -1,
        type: 0,
        isTimeline: true,
        durationTicks: 1_000,
        keys: [
            key(IMM_ANIM_ACTION, 400, { uintValue: IMM_ACTION_STOP }),
            key(IMM_ANIM_ACTION, 600, { uintValue: IMM_ACTION_PLAY }),
        ],
    });
    const timeline = layer({
        id: 1,
        parentId: 0,
        type: 0,
        isTimeline: true,
        opacity: 0.5,
        durationTicks: 200,
        maxRepeatCount: 0,
        keys: [
            key(IMM_ANIM_VISIBILITY, 100, { boolValue: true }),
            key(IMM_ANIM_OFFSET, 100, { uintValue: 50 }),
            key(IMM_ANIM_TRANSFORM, 0, {
                interpolation: IMM_INTERPOLATION_LINEAR,
                transformValue: identity,
            }),
            key(IMM_ANIM_TRANSFORM, 100, {
                transformValue: { ...identity, translation: [10, 0, 0] },
            }),
        ],
    });
    const paint = layer({
        id: 2,
        parentId: 1,
        type: 1,
        opacity: 0.8,
        frameRate: 10,
        frameCount: 3,
        maxRepeatCount: 1,
        frameBuffer: new Uint32Array([2, 1, 0]),
        drawings: Array.from({ length: 3 }, () => ({
            biggestStroke: 1,
            strokeCount: 0,
            pointCount: 0,
            geometries: [],
        })),
        keys: [
            key(IMM_ANIM_VISIBILITY, 0, { boolValue: true }),
            key(IMM_ANIM_OPACITY, 0, { interpolation: IMM_INTERPOLATION_LINEAR, floatValue: 0.5 }),
            key(IMM_ANIM_OPACITY, 100, { floatValue: 1 }),
        ],
    });
    return {
        schemaVersion: 2,
        backgroundColor: [0, 0, 0],
        ticksPerSecond: 100,
        animateOnStart: true,
        durationTicks: 1_000,
        chapters: [
            { startTicks: 0, endTicks: 600, markerAction: IMM_ACTION_PLAY },
            { startTicks: 600, endTicks: 1_000, markerAction: IMM_ACTION_PLAY },
        ],
        layers: [root, timeline, paint],
        metrics: { decodeMs: 0, marshalMs: 0, packMs: 0 },
    };
}

describe("IMM deterministic playback evaluation", () => {
    test("evaluates nested timeline offsets, looping, transforms, opacity, and drawing frames", () => {
        const snapshot = evaluateImmDocument(fixture(), 150);
        const timeline = snapshot.layers.get(1);
        const paint = snapshot.layers.get(2);
        expect(timeline?.visible).toBe(true);
        expect(timeline?.localTimeTicks).toBe(100);
        expect(timeline?.transform.translation[0]).toBeCloseTo(10);
        expect(paint?.timelineTicks).toBe(100);
        expect(paint?.opacity).toBeCloseTo(0.5);
        expect(paint?.drawingIndex).toBe(0);

        const looped = evaluateImmDocument(fixture(), 300);
        expect(looped.layers.get(2)?.timelineTicks).toBe(50);
        expect(looped.layers.get(2)?.drawingIndex).toBe(0);
    });

    test("fresh seeks equal evaluation after arbitrary prior playback", () => {
        const document = fixture();
        const fresh = evaluateImmDocument(document, 725);
        const controller = new ImmPlaybackController(document);
        controller.advance(1.5);
        controller.seekTicks(725);
        expect(controller.evaluate()).toEqual(fresh);
    });

    test("waits one tick before stop markers and continue crosses deterministically", () => {
        const controller = new ImmPlaybackController(fixture());
        controller.advance(5);
        expect(controller.timeTicks).toBe(399);
        expect(controller.waiting).toBe(true);
        controller.play();
        controller.advance(0.5);
        expect(controller.timeTicks).toBe(399);
        expect(controller.waiting).toBe(true);
        controller.continue();
        expect(controller.timeTicks).toBe(400);
        expect(controller.waiting).toBe(false);
        controller.advance(0.01);
        expect(controller.timeTicks).toBe(401);
    });

    test("keeps active child timelines running during an authored root wait", () => {
        const controller = new ImmPlaybackController(fixture());
        controller.advance(4);
        expect(controller.timeTicks).toBe(399);
        expect(controller.waiting).toBe(true);
        const childAtStop = controller.evaluate().layers.get(2)?.timelineTicks;

        controller.advance(0.5);
        expect(controller.timeTicks).toBe(399);
        expect(controller.evaluate().layers.get(2)?.timelineTicks).not.toBe(childAtStop);

        controller.pause();
        const childAtPause = controller.evaluate().layers.get(2)?.timelineTicks;
        controller.advance(0.5);
        expect(controller.evaluate().layers.get(2)?.timelineTicks).toBe(childAtPause);

        controller.play();
        controller.advance(0.25);
        expect(controller.waiting).toBe(true);
        expect(controller.evaluate().layers.get(2)?.timelineTicks).not.toBe(childAtPause);
        controller.continue();
        expect(controller.waiting).toBe(false);
    });

    test("selects and skips chapters in both directions", () => {
        const controller = new ImmPlaybackController(fixture());
        controller.skipForward();
        expect(controller.timeTicks).toBe(600);
        expect(controller.chapterIndex).toBe(1);
        controller.skipBack();
        expect(controller.timeTicks).toBe(0);
        controller.selectChapter(1);
        expect(controller.timeTicks).toBe(600);
    });

    test("resolves initial and chapter-authored spawn areas with evaluated world transforms", () => {
        const document = fixture();
        const initial = layer({
            id: 3,
            parentId: 0,
            type: 8,
            name: "Opening view",
            defaultSpawn: true,
            localTransform: { ...identity, translation: [1, 2, 3] },
        });
        const chapterView = layer({
            id: 4,
            parentId: 0,
            type: 8,
            name: "Chapter two view",
            localTransform: { ...identity, translation: [9, 2, 3] },
            keys: [key(IMM_ANIM_ACTION, 600, { uintValue: IMM_ACTION_MAKE_DEFAULT })],
        });
        document.layers.push(initial, chapterView);

        expect(resolveActiveSpawnArea(document, 0)?.state.layer.id).toBe(3);
        const selected = resolveActiveSpawnArea(document, 600);
        expect(selected?.state.layer.id).toBe(4);
        expect(selected?.actionTimeTicks).toBe(600);
        expect(selected?.state.worldTransform.translation).toEqual([9, 2, 3]);
    });

    test("play, pause, manual wait, restart, and chapter selection preserve exact state", () => {
        const document = fixture();
        const controller = new ImmPlaybackController(document);
        controller.pause();
        controller.advance(1);
        expect(controller.timeTicks).toBe(0);
        controller.play();
        controller.advance(0.25);
        expect(controller.timeTicks).toBe(25);
        controller.wait();
        controller.advance(1);
        expect(controller.timeTicks).toBe(25);
        controller.continue();
        expect(controller.timeTicks).toBe(26);
        controller.restart();
        expect(controller.timeTicks).toBe(0);
        expect(controller.playing).toBe(true);

        const freshChapter = new ImmPlaybackController(document);
        freshChapter.selectChapter(1);
        controller.advance(1.5);
        controller.selectChapter(1);
        expect(controller.playing).toBe(true);
        expect(controller.evaluate()).toEqual(freshChapter.evaluate());
    });

    test("restarts deterministically at root loop actions", () => {
        const document = fixture();
        const root = document.layers[0];
        root?.keys.push(key(IMM_ANIM_ACTION, 800, { uintValue: IMM_ACTION_LOOP }));
        const controller = new ImmPlaybackController(document);
        controller.seekTicks(750);
        controller.advance(1);
        expect(controller.timeTicks).toBe(50);
        expect(controller.playing).toBe(true);
    });

    test("applies pivot compensation while interpolating transforms", () => {
        const document = fixture();
        const paint = document.layers[2];
        if (paint === undefined) throw new Error("Paint fixture is missing");
        paint.pivotTransform = { ...identity, translation: [1, 0, 0] };
        paint.keys = [
            key(IMM_ANIM_VISIBILITY, 0, { boolValue: true }),
            key(IMM_ANIM_TRANSFORM, 0, {
                interpolation: IMM_INTERPOLATION_LINEAR,
                transformValue: identity,
            }),
            key(IMM_ANIM_TRANSFORM, 100, {
                transformValue: { ...identity, rotation: [0, 0, 1, 0] },
            }),
        ];
        const state = evaluateImmDocument(document, 50).layers.get(2);
        expect(state?.transform.translation[0]).toBeCloseTo(0, 5);
        expect(state?.transform.translation[1]).toBeCloseTo(-1, 5);
    });

    test("matches native easing and stepped interpolation curves", () => {
        const cases = [
            [IMM_INTERPOLATION_LINEAR, 0.25],
            [IMM_INTERPOLATION_SMOOTHSTEP, 0.15625],
            [IMM_INTERPOLATION_EASE_IN, 0.015625],
            [IMM_INTERPOLATION_EASE_OUT, 0.578125],
            [IMM_INTERPOLATION_NONE, 0],
        ] as const;
        for (const [interpolation, expected] of cases) {
            const document = fixture();
            const paint = document.layers[2];
            if (paint === undefined) throw new Error("Paint fixture is missing");
            paint.keys = [
                key(IMM_ANIM_VISIBILITY, 0, { boolValue: true }),
                key(IMM_ANIM_OPACITY, 0, { interpolation, floatValue: 0 }),
                key(IMM_ANIM_OPACITY, 100, { floatValue: 1 }),
            ];
            expect(evaluateImmDocument(document, 25).layers.get(2)?.opacity).toBeCloseTo(expected * 0.5, 7);
        }
    });
});
