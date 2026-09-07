import { IMMLoadSession, ImmFrameEvaluator } from "/imm-library/imm-three-loader.js";

// Diagnostic only: retain the real queue and adapter, stop at an explicit budget.
const parameters = new URLSearchParams(location.search);
const seconds = Number(parameters.get("time-seconds"));
const state = { done: false, requests: 0, packetBytes: 0, adapterMs: 0, nativeMs: 0,
    startedAt: 0, firstReadyMs: null, windowReadyMs: null, tasks: [],
    soundUpdates: 0, soundAdapterMs: 0, otherAdapterMs: 0, sceneRefreshMs: 0,
    audioDecodeCalls: 0, audioDecodeMs: 0, audioDecodeFailures: 0, resourceHash: 2166136261 };
window.__immNavigation = state;
const key = item => item.type === "drawing" ? `drawing:${item.layerId}:${item.drawingId}` : `asset:${item.layerId}`;
const originalContinue = IMMLoadSession.prototype.continue;
IMMLoadSession.prototype.continue = async function(document, work, onDelta, options) {
    state.queueLength = work.length;
    await new Promise(resolve => { state.begin = resolve; });
    const asset = window.__galleryImm.viewer.immAsset;
    asset.pause();
    state.startedAt = performance.now();
    const evaluator = new ImmFrameEvaluator(document);
    const first = new Set(), windowWork = new Set();
    for (let frame = 0; frame <= 300; frame++) {
        const snapshot = evaluator.evaluateFrame(Math.round((seconds + frame / 60) * document.ticksPerSecond));
        for (const evaluated of snapshot.layers.values()) {
            if (!evaluated.visible) continue;
            const layer = evaluated.layer;
            const id = layer.type === 1 && evaluated.drawingIndex >= 0
                ? `drawing:${layer.id}:${evaluated.drawingIndex}`
                : [3, 4, 5, 8].includes(layer.type) ? `asset:${layer.id}` : null;
            if (id) { windowWork.add(id); if (frame === 0) first.add(id); }
        }
    }
    const pending = new Set(work.map(key));
    for (const id of [...first]) if (!pending.has(id)) first.delete(id);
    for (const id of [...windowWork]) if (!pending.has(id)) windowWork.delete(id);
    state.planningMs = performance.now() - state.startedAt;
    state.firstRequired = first.size; state.windowRequired = windowWork.size;
    state.firstLastQueueIndex = work.findLastIndex(item => first.has(key(item)));
    state.windowLastQueueIndex = work.findLastIndex(item => windowWork.has(key(item)));
    state.targetKeys = [...windowWork].sort();
    window.__galleryImm.viewer.applyIMMAuthoredCamera(asset.seek(seconds));
    const refresh = asset.view.refreshLayer;
    asset.view.refreshLayer = function(...args) {
        const start = performance.now();
        try { return refresh.apply(this, args); }
        finally { state.sceneRefreshMs += performance.now() - start; }
    };
    const audioPrototype = globalThis.BaseAudioContext?.prototype;
    const decode = audioPrototype?.decodeAudioData;
    if (decode) audioPrototype.decodeAudioData = function(...args) {
        const start = performance.now();
        state.audioDecodeCalls++;
        return decode.apply(this, args).catch(error => { state.audioDecodeFailures++; throw error; })
            .finally(() => { state.audioDecodeMs += performance.now() - start; });
    };
    const layerTypes = new Map(document.layers.map(layer => [layer.id, layer.type]));
    const observer = new PerformanceObserver(list => {
        for (const task of list.getEntries()) state.tasks.push({ start: task.startTime, duration: task.duration });
    });
    observer.observe({ type: "longtask" });
    const finish = reason => {
        if (state.done) return;
        state.elapsedMs = performance.now() - state.startedAt;
        state.firstMissing = first.size; state.windowMissing = windowWork.size;
        state.reason = reason;
        // The run is censored on a budget stop; this is not background completion.
        this.dispose();
        setTimeout(() => { observer.disconnect(); state.done = true; }, 100);
    };
    const timeout = setTimeout(() => finish("time-budget"), Number(parameters.get("budget-ms") ?? 30_000));
    try {
        await originalContinue.call(this, document, work, async (delta, item) => {
            const start = performance.now();
            await onDelta(delta, item);
            const adapterMs = performance.now() - start;
            state.adapterMs += adapterMs;
            if (layerTypes.get(item.layerId) === 5) { state.soundUpdates++; state.soundAdapterMs += adapterMs; }
            else state.otherAdapterMs += adapterMs;
            state.nativeMs += delta.metrics.decodeMs + delta.metrics.nativeBuildMs;
            state.requests++; state.packetBytes += delta.metrics.packetBytes;
            for (const character of `${key(item)}:${delta.metrics.packetBytes};`) {
                state.resourceHash = Math.imul(state.resourceHash ^ character.charCodeAt(0), 16777619) >>> 0;
            }
            first.delete(key(item)); windowWork.delete(key(item));
            const elapsed = performance.now() - state.startedAt;
            if (!first.size && state.firstReadyMs === null) state.firstReadyMs = elapsed;
            if (!windowWork.size) { state.windowReadyMs = elapsed; finish("target-resident"); }
            else if (state.packetBytes >= 256 * 1024 * 1024) finish("byte-budget");
            else if (state.requests >= Number(parameters.get("item-budget") ?? 2048)) finish("resource-budget");
        }, options);
        if (!state.reason) finish("queue-ended");
    } catch (error) { state.error = String(error); finish("error"); }
    finally {
        clearTimeout(timeout);
        asset.view.refreshLayer = refresh;
        if (decode) audioPrototype.decodeAudioData = decode;
    }
};
