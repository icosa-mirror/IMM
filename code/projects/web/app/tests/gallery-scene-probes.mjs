/** Inspect authored scene density without decoding all deferred resources. */
export function inspectGalleryScenes() {
    const asset = window.__galleryImm.viewer.immAsset;
    const document = asset.document;
    const savedTicks = asset.playback.timeTicks;
    const candidates = [];
    try {
        for (let chapter = 0; chapter < document.chapters.length; chapter++) {
            const start = document.chapters[chapter].startTicks;
            const end = document.chapters[chapter + 1]?.startTicks ?? document.durationTicks;
            for (const ticks of new Set([Math.min(start + 5 * document.ticksPerSecond, end - 1), Math.round((start + end) / 2)])) {
                asset.playback.seekTicks(Math.max(start, ticks));
                const snapshot = asset.playback.evaluate();
                let paintLayers = 0, points = 0, strokes = 0, animatedLayers = 0, models = 0, pictures = 0;
                for (const state of snapshot.layers.values()) {
                    if (!state.visible || state.opacity <= 0) continue;
                    const layer = state.layer;
                    if (layer.type === 1 && state.drawingIndex >= 0) {
                        const drawing = layer.drawings[state.drawingIndex];
                        paintLayers++;
                        points += drawing?.pointCount ?? 0;
                        strokes += drawing?.strokeCount ?? 0;
                        if (layer.drawings.length > 1) animatedLayers++;
                    } else if (layer.type === 3) models++;
                    else if (layer.type === 4) pictures++;
                }
                candidates.push({ chapter, seconds: snapshot.timeTicks / document.ticksPerSecond,
                    paintLayers, points, strokes, animatedLayers, models, pictures });
            }
        }
        return { layers: document.layers.length, durationSeconds: document.durationTicks / document.ticksPerSecond,
            candidates: candidates.sort((a, b) => b.points - a.points || b.paintLayers - a.paintLayers) };
    } finally { asset.playback.seekTicks(savedTicks); }
}

/** Collect every resource used by the fixed 60 Hz playback trajectory. */
export function selectSceneResources(document, work, evaluator, seconds, duration) {
    const required = new Set();
    for (let frame = 0; frame <= Math.ceil(duration * 60); frame++) {
        const snapshot = evaluator.evaluateFrame(Math.round((seconds + frame / 60) * document.ticksPerSecond));
        for (const state of snapshot.layers.values()) {
            if (!state.visible) continue; // The renderer also visits zero-opacity visible layers.
            const layer = state.layer;
            if (layer.type === 1 && state.drawingIndex >= 0) required.add(`drawing:${layer.id}:${state.drawingIndex}`);
            else if ([3, 4, 8].includes(layer.type)) required.add(`asset:${layer.id}`);
        }
    }
    return work.filter(item => required.has(item.type === "drawing"
        ? `drawing:${item.layerId}:${item.drawingId}` : `asset:${item.layerId}`));
}
