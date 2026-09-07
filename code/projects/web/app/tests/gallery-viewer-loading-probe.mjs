import { selectSceneResources } from "/imm-scene-probes.mjs";
import { REVISION } from "three";
import { IMMLoader, IMMLoadSession, ImmFrameEvaluator } from "/imm-library/imm-three-loader.js";

// Instrument the real Gallery loader without changing its production integration.
const parameters = new URLSearchParams(location.search);
const mode = parameters.get("evaluation");
const deliveryMode = parameters.get("delivery") ?? "single";
const seconds = Number(parameters.get("time-seconds") ?? 1);
const limit = Number(parameters.get("background-limit") ?? Infinity);
const sceneDuration = Number(parameters.get("scene-duration") ?? 30);
const sceneSelection = parameters.get("resource-selection") === "scene";
const state = { mode, deliveryMode, seconds, sceneDuration, threeRevision: REVISION, startedAt: 0, readyAt: 0, completedAt: 0, originalDeferred: 0, selectedDeferred: 0,
    frames: [], updates: [], tasks: [], peakHeapBytes: 0 };
window.__galleryLoading = state;
const observer = new PerformanceObserver(list => {
    for (const task of list.getEntries()) state.tasks.push({ start: task.startTime, duration: task.duration });
});
observer.observe({ type: "longtask", buffered: true });
let previous;
function sample(now) {
    if (state.startedAt && !state.completedAt) {
        if (previous !== undefined) state.frames.push({ start: previous, end: now, duration: now - previous });
        previous = now;
        state.peakHeapBytes = Math.max(state.peakHeapBytes, performance.memory?.usedJSHeapSize ?? 0);
    }
    if (!state.completedAt) requestAnimationFrame(sample);
}
requestAnimationFrame(sample);
const originalContinue = IMMLoadSession.prototype.continue;
IMMLoadSession.prototype.continue = function(document, work, onDelta) {
    state.originalDeferred = work.length;
    const selected = sceneSelection
        ? selectSceneResources(document, work, new ImmFrameEvaluator(document), seconds, sceneDuration)
        : work.slice(0, limit);
    state.selectedDeferred = selected.length;
    if (sceneSelection) console.log(`IMM_SCENE_20260907: preloading ${selected.length} resources for the scene segment`);
    return originalContinue.call(this, document, selected, onDelta, { mode: deliveryMode });
};
const originalLoad = IMMLoader.prototype.loadAsync;
IMMLoader.prototype.loadAsync = function(...args) {
    state.startedAt = performance.now();
    return originalLoad.apply(this, args);
};
const originalParse = IMMLoader.prototype.parseAsync;
IMMLoader.prototype.parseAsync = async function(...args) {
    const asset = await originalParse.apply(this, args);
    // Hold authored time constant so scheduling cannot change the rendered workload.
    asset.pause();
    asset.seek(seconds);
    if (mode === "owned") asset.playback.advanceFrame = function(delta) { return this.advance(delta); };
    const update = asset.update;
    asset.update = function(...updateArgs) {
        const start = performance.now();
        const result = update.apply(this, updateArgs);
        if (!state.completedAt) state.updates.push({ start, duration: performance.now() - start });
        return result;
    };
    state.expectedTicks = asset.playback.timeTicks;
    state.readyAt = performance.now();
    asset.backgroundComplete.then(() => {
        state.completedAt = performance.now();
        const telemetry = asset.loadTelemetry;
        state.telemetry = { requestedMode: telemetry.requestedMode, effectiveMode: telemetry.effectiveMode,
            fallbackReason: telemetry.fallbackReason, initiallyLoadedItems: telemetry.initiallyLoadedItems,
            completedItems: telemetry.backgroundCompletedItems, requests: telemetry.requests.length,
            packetBytes: telemetry.requests.reduce((sum, item) => sum + item.packetBytes, 0) };
        state.resources = telemetry.requests.slice(-state.selectedDeferred).map(item =>
            [item.type, item.layerId, item.drawingId ?? null, item.packetBytes]);
        state.timeTicks = asset.playback.timeTicks;
        state.playing = asset.playing;
        // Let Chrome deliver the final long-task record before stopping observation.
        setTimeout(() => { observer.disconnect(); state.done = true; }, 100);
    }, error => { state.error = String(error); state.done = true; });
    return asset;
};
