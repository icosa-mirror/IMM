/** Measure repeated, fully resident scene playback through each host's real render loop. */
export async function measureScenePlayback({ harness, seconds, segmentSeconds = 30, measuredLoops = 2 }) {
    const gallery = harness === "gallery" ? window.__galleryImm.viewer : null;
    const asset = gallery?.immAsset;
    const ticksPerSecond = asset?.document.ticksPerSecond ?? window.__immPlayback.snapshot().ticksPerSecond;
    const startTicks = Math.round(seconds * ticksPerSecond);
    const segmentFrames = Math.round(segmentSeconds * 60);
    const warmupFrames = segmentFrames;
    const measuredFrames = segmentFrames * measuredLoops;
    const totalFrames = warmupFrames + measuredFrames;
    const pause = () => asset ? asset.pause() : window.__immPlayback.pause();
    const play = () => asset ? asset.play() : window.__immPlayback.play();
    const seek = () => asset ? asset.playback.seekTicks(startTicks) : window.__immPlayback.seekTicks(startTicks);
    const transport = () => asset ? { timeTicks: asset.playback.timeTicks, waiting: asset.waiting } : window.__immPlayback.snapshot();
    const originalUpdater = gallery?.contentUpdater;
    let frame = 0, updateMs = 0, previous, measuredStart, measuredEnd;
    const intervals = [], updates = [], tasks = [], drawingFrames = [];
    let triangles = 0, draws = 0, workHash = 2166136261, peakHeap = 0;
    const label = document.createElement("div");
    label.style.cssText = "position:fixed;top:8px;left:12px;padding:6px 10px;background:#000b;color:white;font:16px sans-serif;z-index:9999;pointer-events:none";
    label.textContent = "Benchmark: warming the loaded scene";
    document.body.append(label);
    const observer = new PerformanceObserver(list => {
        for (const task of list.getEntries()) tasks.push({ start: task.startTime, duration: task.duration });
    });
    observer.observe({ type: "longtask" });
    pause(); seek();
    if (asset) {
        asset.update(0, gallery.activeCamera);
        gallery.contentUpdater = (_time, camera) => {
            const start = performance.now();
            originalUpdater((frame + 1) * 1000 / 60, camera);
            updateMs = performance.now() - start;
        };
    }
    let raf;
    try {
        console.log("IMM_SCENE_20260907: warming a complete scene segment");
        play();
        await new Promise((done, reject) => {
            const timeout = setTimeout(() => reject(new Error("Scene playback sampling timed out")), totalFrames * 100 + 60_000);
            function sample(now) {
                try {
                    const state = transport();
                    const expected = startTicks + Math.round(((frame % segmentFrames) + 1) * ticksPerSecond / 60);
                    if (state.waiting || Math.abs(state.timeTicks - expected) > 1) {
                        throw new Error(`Scene trajectory diverged at frame ${frame}: expected ${expected}, got ${state.timeTicks}, waiting ${state.waiting}`);
                    }
                    const metrics = gallery ? { updateMs, drawCalls: gallery.renderer.info.render.calls,
                        triangles: gallery.renderer.info.render.triangles } : window.__immFrameDiagnostics();
                    const drawing = window.__immDrawingProbe?.takeFrame();
                    if (frame === warmupFrames) {
                        measuredStart = previous;
                        label.textContent = "Benchmark: measuring loaded-scene playback";
                        console.log("IMM_SCENE_20260907: warm-up complete; measuring sustained playback");
                    }
                    if (frame >= warmupFrames) {
                        if (drawing) drawingFrames.push(drawing);
                        intervals.push(now - previous); updates.push(metrics.updateMs);
                        triangles += metrics.triangles; draws += metrics.drawCalls;
                        workHash = Math.imul(workHash ^ metrics.triangles, 16777619);
                        workHash = Math.imul(workHash ^ metrics.drawCalls, 16777619);
                        peakHeap = Math.max(peakHeap, performance.memory?.usedJSHeapSize ?? 0);
                    }
                    previous = now;
                    if (++frame === totalFrames) {
                        measuredEnd = now; clearTimeout(timeout); done(); return;
                    }
                    if (frame % segmentFrames === 0) seek();
                    raf = requestAnimationFrame(sample);
                } catch (error) { clearTimeout(timeout); reject(error); }
            }
            raf = requestAnimationFrame(sample);
        });
        const stats = values => {
            const sorted = [...values].sort((a, b) => a - b);
            return { mean: values.reduce((sum, x) => sum + x, 0) / values.length,
                median: sorted[Math.floor(sorted.length / 2)], p95: sorted[Math.floor(sorted.length * .95)], max: sorted.at(-1) };
        };
        const elapsedMs = measuredEnd - measuredStart;
        const longTasks = tasks.filter(task => task.start >= measuredStart && task.start + task.duration <= measuredEnd);
        console.log(`IMM_SCENE_20260907: completed ${measuredFrames} measured frames in ${(elapsedMs / 1000).toFixed(1)} s`);
        return { measuredFrames, warmupFrames, segmentSeconds, measuredLoops, elapsedMs,
            fps: measuredFrames * 1000 / elapsedMs, frameMs: stats(intervals), updateMs: stats(updates),
            triangles, draws, workHash: workHash >>> 0, peakHeap, longTasks, drawingFrames };
    } finally {
        cancelAnimationFrame(raf); observer.disconnect(); pause(); seek();
        if (gallery) gallery.contentUpdater = originalUpdater;
        label.remove();
    }
}
