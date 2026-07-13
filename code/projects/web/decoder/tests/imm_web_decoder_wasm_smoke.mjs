import { readFile } from "node:fs/promises";
import { pathToFileURL } from "node:url";
import { Worker } from "node:worker_threads";


if (process.argv.length !== 4) {
    throw new Error("Expected worker module and sample IMM paths");
}

const workerUrl = pathToFileURL(process.argv[2]);
const sourceBytes = await readFile(process.argv[3]);
const source = sourceBytes.buffer.slice(
    sourceBytes.byteOffset,
    sourceBytes.byteOffset + sourceBytes.byteLength,
);
const worker = new Worker(workerUrl, { type: "module" });

try {
    const request = (message, transfer) => new Promise((resolve, reject) => {
        const timeout = setTimeout(() => reject(new Error("Timed out waiting for decoder worker")), 10_000);
        worker.once("error", (error) => {
            clearTimeout(timeout);
            reject(error);
        });
        worker.once("message", (message) => {
            clearTimeout(timeout);
            resolve(message);
        });
        worker.postMessage(message, transfer);
    });
    const result = await request({ requestId: 1, type: "inspect", source }, [source]);

    if (!result.ok) {
        throw new Error(`Wasm inspection failed at ${result.error.byteOffset}: ${result.error.message}`);
    }
    if (result.requestId !== 1 || result.summary.schemaVersion !== 1) {
        throw new Error("Worker response identity or schema version mismatch");
    }
    if (result.summary.sourceSize !== 5_831_101n) {
        throw new Error(`Source size mismatch: ${result.summary.sourceSize}`);
    }
    if (result.summary.chunkCount !== 5 || result.summary.chunkFlags !== 31) {
        throw new Error("Top-level chunk summary mismatch");
    }
    if (result.summary.sequenceType !== 1 || result.summary.sequenceCapabilities !== 2) {
        throw new Error("Sequence category mismatch");
    }
    if (result.summary.assetCount !== 38) {
        throw new Error(`Asset count mismatch: ${result.summary.assetCount}`);
    }

    const decodeBytes = await readFile(process.argv[3]);
    const decodeSource = decodeBytes.buffer.slice(
        decodeBytes.byteOffset,
        decodeBytes.byteOffset + decodeBytes.byteLength,
    );
    const decoded = await request({ requestId: 2, type: "decode", source: decodeSource }, [decodeSource]);
    if (!decoded.ok) {
        throw new Error(`Wasm scene decode failed: ${decoded.error.message}`);
    }
    const paintLayers = decoded.document.layers.filter((layer) => layer.type === 1);
    const pictureLayers = decoded.document.layers.filter((layer) => layer.type === 4);
    if (paintLayers.length !== 30 || pictureLayers.length !== 1) {
        throw new Error(`Unexpected visible content layers: ${paintLayers.length} paint, ${pictureLayers.length} picture`);
    }
    const strokeCount = paintLayers.reduce((sum, layer) => sum + layer.drawings.reduce(
        (drawingSum, drawing) => drawingSum + drawing.descriptors.length / 4, 0), 0);
    const pointCount = paintLayers.reduce((sum, layer) => sum + layer.drawings.reduce(
        (drawingSum, drawing) => drawingSum + drawing.points.length / 14, 0), 0);
    if (strokeCount === 0 || pointCount === 0) {
        throw new Error(`Scene decode returned no paint data: ${strokeCount} strokes, ${pointCount} points`);
    }
    if (pictureLayers[0].picture?.pixels.length === 0) {
        throw new Error("Scene decode returned no picture pixels");
    }
    if (pictureLayers[0].picture.contentType !== 1 ||
        pictureLayers[0].picture.width !== 2_000 || pictureLayers[0].picture.height !== 1_000) {
        throw new Error("Scene decode returned incorrect mono equirectangular picture metadata");
    }
    if (decoded.document.backgroundColor.length !== 3 ||
        !decoded.document.backgroundColor.every(Number.isFinite)) {
        throw new Error("Scene decode returned an invalid background color");
    }
    for (const layer of decoded.document.layers) {
        for (const transform of [layer.localTransform, layer.worldTransform, layer.pivotTransform]) {
            if (!transform.rotation.every(Number.isFinite) || !transform.translation.every(Number.isFinite) ||
                !Number.isFinite(transform.scale) || transform.flip < 0 || transform.flip > 3) {
                throw new Error(`Layer ${layer.id} returned an invalid transform`);
            }
        }
        if (typeof layer.visible !== "boolean" || !Number.isFinite(layer.opacity)) {
            throw new Error(`Layer ${layer.id} returned invalid visibility or opacity`);
        }
        for (const drawing of layer.drawings) {
            if (drawing.bounds.length !== drawing.descriptors.length / 4 * 6 ||
                !Array.from(drawing.bounds).every(Number.isFinite)) {
                throw new Error(`Layer ${layer.id} returned invalid paint bounds`);
            }
        }
    }
    if (!decoded.document.layers.some((layer) => layer.worldTransform.flip !== 0) ||
        !decoded.document.layers.some((layer) => layer.opacity > 0 && layer.opacity < 1) ||
        !decoded.document.layers.some((layer) => layer.pivotTransform.translation.some((value) => value !== 0))) {
        throw new Error("Sample did not exercise flip, opacity, and pivot contracts");
    }
    const defaultSpawns = decoded.document.layers.filter((layer) => layer.type === 8 && layer.defaultSpawn);
    if (defaultSpawns.length !== 1) {
        throw new Error(`Expected one default spawn area, found ${defaultSpawns.length}`);
    }
    const geometries = paintLayers.flatMap((layer) => layer.drawings.flatMap((drawing) => drawing.geometries));
    if (geometries.length !== 41 || geometries.some((geometry) => geometry.positions.length === 0 || geometry.indices.length === 0)) {
        throw new Error(`Unexpected packed geometry result: ${geometries.length} batches`);
    }
    const triangleCount = geometries.reduce((sum, geometry) => sum + geometry.triangleCount, 0);
    if (triangleCount !== 798_922) {
        throw new Error(`Packed paint triangle count mismatch: ${triangleCount}`);
    }
    if (!(decoded.document.metrics.packMs >= 0)) {
        throw new Error("Worker did not report geometry packing time");
    }

    console.log(`IMM_WEB_WASM_WORKER_SMOKE: passed (${strokeCount} strokes, ${pointCount} points, ${triangleCount} paint triangles)`);
} finally {
    await worker.terminate();
}
