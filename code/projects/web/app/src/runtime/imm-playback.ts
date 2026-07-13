import {
    IMM_ACTION_LOOP,
    IMM_ACTION_MAKE_DEFAULT,
    IMM_ACTION_STOP,
    IMM_ANIM_ACTION,
    IMM_ANIM_DRAW_IN_TIME,
    IMM_ANIM_LOOP,
    IMM_ANIM_OFFSET,
    IMM_ANIM_OPACITY,
    IMM_ANIM_TRANSFORM,
    IMM_ANIM_VISIBILITY,
    IMM_INTERPOLATION_EASE_IN,
    IMM_INTERPOLATION_EASE_OUT,
    IMM_INTERPOLATION_NONE,
    IMM_INTERPOLATION_SMOOTHSTEP,
    type ImmAnimationKey,
    type ImmDocument,
    type ImmLayer,
    type ImmTransform,
} from "../format/imm-document";

export interface ImmEvaluatedLayer {
    layer: ImmLayer;
    timelineTicks: number;
    localTimeTicks: number;
    visible: boolean;
    opacity: number;
    transform: ImmTransform;
    worldTransform: ImmTransform;
    drawInTime: number;
    drawingIndex: number;
}

export interface ImmPlaybackSnapshot {
    timeTicks: number;
    chapterIndex: number;
    layers: ReadonlyMap<number, ImmEvaluatedLayer>;
}

interface EvaluationContext {
    visible: boolean;
    opacity: number;
    timelineTicks: number;
}

interface LocalEvaluation {
    visible: boolean;
    opacity: number;
    transform: ImmTransform;
    drawInTime: number;
    localTimeTicks: number;
    loop: boolean | undefined;
}

export class ImmPlaybackController {
    readonly document: ImmDocument;
    timeTicks = 0;
    playing: boolean;
    waiting = false;
    playbackRate = 1;

    #fractionalTicks = 0;

    constructor(document: ImmDocument) {
        this.document = document;
        this.playing = document.animateOnStart;
    }

    get durationTicks(): number {
        return this.document.durationTicks;
    }

    get chapterIndex(): number {
        return chapterAt(this.document, this.timeTicks);
    }

    play(): void {
        if (this.timeTicks >= this.durationTicks) this.restart();
        this.waiting = false;
        this.playing = true;
    }

    pause(): void {
        this.playing = false;
    }

    wait(): void {
        this.waiting = true;
    }

    continue(): void {
        if (!this.waiting) return;
        this.waiting = false;
        this.playing = true;
        this.timeTicks = Math.min(this.durationTicks, this.timeTicks + 1);
    }

    restart(): void {
        this.seekTicks(0);
        this.playing = true;
    }

    seekTicks(timeTicks: number): void {
        this.timeTicks = clampTicks(timeTicks, this.durationTicks);
        this.waiting = false;
        this.#fractionalTicks = 0;
    }

    seekSeconds(timeSeconds: number): void {
        this.seekTicks(Math.round(timeSeconds * this.document.ticksPerSecond));
    }

    selectChapter(index: number): void {
        const chapter = this.document.chapters[index];
        if (chapter === undefined) throw new RangeError(`Chapter ${index} does not exist`);
        this.seekTicks(chapter.startTicks);
    }

    skipForward(): void {
        const next = this.document.chapters[this.chapterIndex + 1];
        if (next !== undefined) this.seekTicks(next.startTicks);
    }

    skipBack(): void {
        const chapter = this.document.chapters[this.chapterIndex];
        const targetIndex = chapter !== undefined && this.timeTicks > chapter.startTicks + 1
            ? this.chapterIndex
            : Math.max(0, this.chapterIndex - 1);
        const target = this.document.chapters[targetIndex];
        this.seekTicks(target?.startTicks ?? 0);
    }

    advance(deltaSeconds: number): ImmPlaybackSnapshot {
        if (this.playing && !this.waiting && deltaSeconds > 0) {
            const exactTicks = deltaSeconds * this.document.ticksPerSecond * this.playbackRate + this.#fractionalTicks;
            const wholeTicks = Math.floor(exactTicks);
            this.#fractionalTicks = exactTicks - wholeTicks;
            const target = Math.min(this.durationTicks, this.timeTicks + wholeTicks);
            const stop = nextStopBetween(this.document, this.timeTicks, target);
            const loop = nextLoopBetween(this.document, this.timeTicks, target);
            if (loop !== undefined && (stop === undefined || loop < stop)) {
                this.timeTicks = loop > 0 ? (target - loop) % loop : 0;
            } else if (stop !== undefined) {
                this.timeTicks = Math.max(0, stop - 1);
                this.waiting = true;
            } else {
                this.timeTicks = target;
            }
            if (this.timeTicks >= this.durationTicks) this.playing = false;
        }
        return this.evaluate();
    }

    evaluate(): ImmPlaybackSnapshot {
        return evaluateImmDocument(this.document, this.timeTicks);
    }
}

export function evaluateImmDocument(document: ImmDocument, requestedTicks: number): ImmPlaybackSnapshot {
    const timeTicks = clampTicks(requestedTicks, document.durationTicks);
    const states = new Map<number, ImmEvaluatedLayer>();
    const contexts = new Map<number, EvaluationContext>();
    const rootTimelineTicks = wrapTimeline(timeTicks, document.durationTicks, 1);

    for (const layer of document.layers) {
        const parent = layer.parentId < 0 ? undefined : contexts.get(layer.parentId);
        const controllingTicks = parent?.timelineTicks ?? rootTimelineTicks;
        const local = evaluateLocalLayer(layer, controllingTicks, document.ticksPerSecond);
        const visible = (parent?.visible ?? true) && local.visible;
        const opacity = (parent?.opacity ?? 1) * local.opacity;
        const timelineTicks = layer.isTimeline
            ? wrapTimeline(local.localTimeTicks, layer.durationTicks, local.loop === true ? 0 : layer.maxRepeatCount)
            : controllingTicks;
        const drawingIndex = selectDrawing(layer, local.localTimeTicks, document.ticksPerSecond, local.loop);
        states.set(layer.id, {
            layer,
            timelineTicks: controllingTicks,
            localTimeTicks: local.localTimeTicks,
            visible,
            opacity,
            transform: local.transform,
            worldTransform: parent === undefined
                ? cloneTransform(local.transform)
                : composeTransform(states.get(layer.parentId)?.worldTransform ?? layer.worldTransform, local.transform),
            drawInTime: local.drawInTime,
            drawingIndex,
        });
        contexts.set(layer.id, { visible, opacity, timelineTicks });
    }

    return { timeTicks, chapterIndex: chapterAt(document, timeTicks), layers: states };
}

/** Resolves the authored spawn area at a playback time, including timed MakeDefault actions. */
export interface ImmActiveSpawnArea {
    state: ImmEvaluatedLayer;
    actionTimeTicks: number | null;
}

export function resolveActiveSpawnArea(
    document: ImmDocument,
    requestedTicks: number,
    snapshot = evaluateImmDocument(document, requestedTicks),
): ImmActiveSpawnArea | undefined {
    let active = [...snapshot.layers.values()].find((state) => state.layer.type === 8 && state.layer.defaultSpawn);
    let activeActionTime = Number.NEGATIVE_INFINITY;
    for (const state of snapshot.layers.values()) {
        if (state.layer.type !== 8) continue;
        for (const key of state.layer.keys) {
            if (key.property !== IMM_ANIM_ACTION || key.uintValue !== IMM_ACTION_MAKE_DEFAULT) continue;
            if (key.timeTicks <= state.timelineTicks && key.timeTicks >= activeActionTime) {
                active = state;
                activeActionTime = key.timeTicks;
            }
        }
    }
    return active === undefined ? undefined : {
        state: active,
        actionTimeTicks: Number.isFinite(activeActionTime) ? activeActionTime : null,
    };
}

function evaluateLocalLayer(layer: ImmLayer, timelineTicks: number, ticksPerSecond: number): LocalEvaluation {
    const visibilityKeys = keysFor(layer, IMM_ANIM_VISIBILITY);
    const visibilityKey = previousKey(visibilityKeys, timelineTicks);
    const visible = visibilityKeys.length === 0 ? layer.visible : visibilityKey?.boolValue === true;
    const offset = visibilityKey === undefined
        ? valueAt(keysFor(layer, IMM_ANIM_OFFSET), timelineTicks, "uintValue", 0)
        : keysFor(layer, IMM_ANIM_OFFSET).find((key) => key.timeTicks === visibilityKey.timeTicks)?.uintValue ?? 0;
    const localTimeTicks = visibilityKey !== undefined && visible
        ? Math.max(0, timelineTicks - visibilityKey.timeTicks + offset)
        : timelineTicks;
    return {
        visible,
        opacity: interpolateNumber(keysFor(layer, IMM_ANIM_OPACITY), timelineTicks, "floatValue", layer.opacity),
        transform: interpolateTransform(
            keysFor(layer, IMM_ANIM_TRANSFORM),
            timelineTicks,
            layer.localTransform,
            layer.pivotTransform,
        ),
        drawInTime: interpolateNumber(keysFor(layer, IMM_ANIM_DRAW_IN_TIME), timelineTicks, "doubleValue", 0),
        localTimeTicks,
        loop: previousKey(keysFor(layer, IMM_ANIM_LOOP), timelineTicks)?.boolValue,
    };
}

function selectDrawing(layer: ImmLayer, localTicks: number, ticksPerSecond: number, loop: boolean | undefined): number {
    if (layer.frameBuffer.length === 0 || layer.frameRate <= 0) return 0;
    const frame = Math.floor(localTicks * layer.frameRate / ticksPerSecond);
    const maxRepeatCount = loop === undefined ? layer.maxRepeatCount : loop ? 0 : 1;
    const frameIndex = maxRepeatCount !== 0 && frame >= layer.frameBuffer.length * maxRepeatCount
        ? layer.frameBuffer.length - 1
        : frame % layer.frameBuffer.length;
    const drawing = layer.frameBuffer[Math.max(0, frameIndex)] ?? 0;
    return drawing < layer.drawings.length ? drawing : 0;
}

function keysFor(layer: ImmLayer, property: number): ImmAnimationKey[] {
    return layer.keys.filter((key) => key.property === property);
}

function previousKey(keys: ImmAnimationKey[], ticks: number): ImmAnimationKey | undefined {
    let result: ImmAnimationKey | undefined;
    for (const key of keys) {
        if (key.timeTicks > ticks) break;
        result = key;
    }
    return result;
}

function interpolationPair(keys: ImmAnimationKey[], ticks: number): [ImmAnimationKey, ImmAnimationKey | undefined, number] | undefined {
    if (keys.length === 0) return undefined;
    let previous = keys[0];
    if (previous === undefined) return undefined;
    let next: ImmAnimationKey | undefined;
    for (let index = 0; index < keys.length; index++) {
        const key = keys[index];
        if (key === undefined) continue;
        if (key.timeTicks <= ticks) previous = key;
        if (key.timeTicks > ticks) {
            next = key;
            break;
        }
    }
    if (next === undefined || previous.interpolation === IMM_INTERPOLATION_NONE) return [previous, undefined, 0];
    const span = next.timeTicks - previous.timeTicks;
    const raw = span <= 0 ? 0 : Math.max(0, Math.min(1, (ticks - previous.timeTicks) / span));
    return [previous, next, ease(raw, previous.interpolation)];
}

function interpolateNumber(
    keys: ImmAnimationKey[],
    ticks: number,
    field: "floatValue" | "doubleValue",
    fallback: number,
): number {
    const pair = interpolationPair(keys, ticks);
    if (pair === undefined) return fallback;
    const [previous, next, t] = pair;
    return next === undefined ? previous[field] : previous[field] + (next[field] - previous[field]) * t;
}

function interpolateTransform(
    keys: ImmAnimationKey[],
    ticks: number,
    fallback: ImmTransform,
    pivot: ImmTransform,
): ImmTransform {
    const pair = interpolationPair(keys, ticks);
    if (pair === undefined) return cloneTransform(fallback);
    const [previous, next, t] = pair;
    if (next === undefined) return cloneTransform(previous.transformValue);
    const pivotedA = composeTransform(previous.transformValue, pivot);
    const pivotedB = composeTransform(next.transformValue, pivot);
    return composeTransform(mixTransform(pivotedA, pivotedB, t), invertTransform(pivot));
}

function valueAt(
    keys: ImmAnimationKey[],
    ticks: number,
    field: "uintValue",
    fallback: number,
): number {
    return previousKey(keys, ticks)?.[field] ?? fallback;
}

function ease(t: number, interpolation: number): number {
    if (interpolation === IMM_INTERPOLATION_EASE_IN) return t * t * t;
    if (interpolation === IMM_INTERPOLATION_EASE_OUT) return 1 - (1 - t) ** 3;
    if (interpolation === IMM_INTERPOLATION_SMOOTHSTEP) return t * t * (3 - 2 * t);
    return t;
}

function wrapTimeline(ticks: number, duration: number, maxRepeatCount: number): number {
    if (duration <= 0) return Math.max(0, ticks);
    const plays = Math.floor(Math.max(0, ticks) / duration);
    if (maxRepeatCount !== 0 && plays >= maxRepeatCount) return duration;
    return Math.max(0, ticks) % duration;
}

function chapterAt(document: ImmDocument, ticks: number): number {
    let result = 0;
    for (let index = 0; index < document.chapters.length; index++) {
        const chapter = document.chapters[index];
        if (chapter !== undefined && chapter.startTicks <= ticks) result = index;
    }
    return result;
}

function nextStopBetween(document: ImmDocument, start: number, end: number): number | undefined {
    const root = document.layers.find((layer) => layer.parentId < 0);
    return root?.keys.find((key) =>
        key.property === IMM_ANIM_ACTION &&
        key.uintValue === IMM_ACTION_STOP &&
        key.timeTicks > start &&
        key.timeTicks <= end)?.timeTicks;
}

function nextLoopBetween(document: ImmDocument, start: number, end: number): number | undefined {
    const root = document.layers.find((layer) => layer.parentId < 0);
    return root?.keys.find((key) =>
        key.property === IMM_ANIM_ACTION &&
        key.uintValue === IMM_ACTION_LOOP &&
        key.timeTicks > start &&
        key.timeTicks <= end)?.timeTicks;
}

function clampTicks(ticks: number, duration: number): number {
    if (!Number.isFinite(ticks)) throw new RangeError("Playback time must be finite");
    return Math.max(0, Math.min(duration, Math.round(ticks)));
}

function cloneTransform(value: ImmTransform): ImmTransform {
    return {
        rotation: [...value.rotation],
        scale: value.scale,
        flip: value.flip,
        translation: [...value.translation],
    };
}

function dot4(a: ImmTransform["rotation"], b: ImmTransform["rotation"]): number {
    return a.reduce((sum, value, index) => sum + value * (b[index] ?? 0), 0);
}

function normalize4(value: ImmTransform["rotation"]): ImmTransform["rotation"] {
    const length = Math.hypot(...value);
    return length === 0 ? [0, 0, 0, 1] : value.map((component) => component / length) as ImmTransform["rotation"];
}

function mixTransform(a: ImmTransform, b: ImmTransform, t: number): ImmTransform {
    return {
        rotation: slerp(a.rotation, b.rotation, t),
        scale: a.scale * (1 - t) + b.scale * t,
        flip: a.flip,
        translation: a.translation.map(
            (value, index) => value * (1 - t) + (b.translation[index] ?? value) * t,
        ) as ImmTransform["translation"],
    };
}

function composeTransform(a: ImmTransform, b: ImmTransform): ImmTransform {
    const scaled = flipVector(b.translation, a.flip).map((value) => value * a.scale) as ImmTransform["translation"];
    const translated = rotateVector(scaled, a.rotation).map(
        (value, index) => value + (a.translation[index] ?? 0),
    ) as ImmTransform["translation"];
    if (a.flip === 0) {
        return {
            rotation: multiplyQuaternion(a.rotation, b.rotation),
            scale: a.scale * b.scale,
            flip: b.flip,
            translation: translated,
        };
    }
    if (b.flip === 0) {
        return {
            rotation: flipQuaternion(
                multiplyQuaternion(flipQuaternion(a.rotation, a.flip), b.rotation),
                a.flip,
            ),
            scale: a.scale * b.scale,
            flip: a.flip,
            translation: translated,
        };
    }
    return { rotation: multiplyQuaternion(a.rotation, b.rotation), scale: a.scale * b.scale, flip: 0, translation: translated };
}

function invertTransform(value: ImmTransform): ImmTransform {
    const rotation = invertQuaternion(value.rotation);
    const scale = 1 / value.scale;
    const inverseTranslation = flipVector(
        rotateVector(value.translation.map((component) => -component) as ImmTransform["translation"], rotation)
            .map((component) => component * scale) as ImmTransform["translation"],
        value.flip,
    );
    return { rotation: flipQuaternion(rotation, value.flip), scale, flip: value.flip, translation: inverseTranslation };
}

function multiplyQuaternion(a: ImmTransform["rotation"], b: ImmTransform["rotation"]): ImmTransform["rotation"] {
    return normalize4([
        a[0] * b[3] + a[3] * b[0] + a[1] * b[2] - a[2] * b[1],
        a[1] * b[3] + a[3] * b[1] + a[2] * b[0] - a[0] * b[2],
        a[2] * b[3] + a[3] * b[2] + a[0] * b[1] - a[1] * b[0],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
    ]);
}

function invertQuaternion(value: ImmTransform["rotation"]): ImmTransform["rotation"] {
    const magnitude = dot4(value, value);
    return [-value[0] / magnitude, -value[1] / magnitude, -value[2] / magnitude, value[3] / magnitude];
}

function flipQuaternion(value: ImmTransform["rotation"], flip: number): ImmTransform["rotation"] {
    if (flip === 1) return [value[0], -value[1], -value[2], value[3]];
    if (flip === 2) return [-value[0], value[1], -value[2], value[3]];
    if (flip === 3) return [-value[0], -value[1], value[2], value[3]];
    return [...value];
}

function flipVector(value: ImmTransform["translation"], flip: number): ImmTransform["translation"] {
    return [flip === 1 ? -value[0] : value[0], flip === 2 ? -value[1] : value[1], flip === 3 ? -value[2] : value[2]];
}

function rotateVector(value: ImmTransform["translation"], rotation: ImmTransform["rotation"]): ImmTransform["translation"] {
    const [x, y, z] = value;
    const [qx, qy, qz, qw] = rotation;
    const ix = qw * x + qy * z - qz * y;
    const iy = qw * y + qz * x - qx * z;
    const iz = qw * z + qx * y - qy * x;
    const iw = -qx * x - qy * y - qz * z;
    return [
        ix * qw + iw * -qx + iy * -qz - iz * -qy,
        iy * qw + iw * -qy + iz * -qx - ix * -qz,
        iz * qw + iw * -qz + ix * -qy - iy * -qx,
    ];
}

function slerp(a: ImmTransform["rotation"], b: ImmTransform["rotation"], t: number): ImmTransform["rotation"] {
    let cosine = dot4(a, b);
    let target = b;
    if (cosine < 0) {
        cosine = -cosine;
        target = b.map((value) => -value) as ImmTransform["rotation"];
    }
    if (1 - cosine <= 1e-6) {
        return normalize4(a.map((value, index) => value * (1 - t) + (target[index] ?? value) * t) as ImmTransform["rotation"]);
    }
    const omega = Math.acos(Math.max(-1, Math.min(1, cosine)));
    const sine = Math.sin(omega);
    const scaleA = Math.sin((1 - t) * omega) / sine;
    const scaleB = Math.sin(t * omega) / sine;
    return normalize4(a.map((value, index) => value * scaleA + (target[index] ?? value) * scaleB) as ImmTransform["rotation"]);
}
