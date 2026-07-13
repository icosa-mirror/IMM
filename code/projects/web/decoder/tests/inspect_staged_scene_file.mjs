import { readFile } from "node:fs/promises";
import { pathToFileURL } from "node:url";
import { Worker } from "node:worker_threads";

if (process.argv.length !== 4) throw new Error("Expected worker module and local IMM paths");
const worker = new Worker(pathToFileURL(process.argv[2]), { type: "module" });
let requestId = 0;

function request(message, transfer = []) {
    return new Promise((resolve, reject) => {
        const timeout = setTimeout(() => reject(new Error("Timed out waiting for decoder worker")), 120_000);
        worker.once("error", reject);
        worker.once("message", (response) => {
            clearTimeout(timeout);
            if (!response.ok) reject(new Error(response.error?.message ?? "Decoder request failed"));
            else resolve(response);
        });
        worker.postMessage({ requestId: ++requestId, ...message }, transfer);
    });
}

try {
    const file = await readFile(process.argv[3]);
    const source = file.buffer.slice(file.byteOffset, file.byteOffset + file.byteLength);
    const metadataStarted = performance.now();
    let response = await request({ type: "openMetadata", source }, [source]);
    const metadataMs = performance.now() - metadataStarted;
    const metadata = response.document;
    const paint = metadata.layers.find((layer) => layer.type === 1 && layer.drawings.length > 0);
    if (paint === undefined) throw new Error("Metadata contained no paint drawing candidate");
    if (metadata.layers.some((layer) => layer.drawings.some((drawing) => drawing.strokeCount !== 0))) {
        throw new Error("Metadata unexpectedly contained decoded strokes");
    }

    const commands = [
        { type: "decodeDrawing", layerId: paint.id, drawingId: 0 },
        ...metadata.layers.filter((layer) => layer.type === 8)
            .map((layer) => ({ type: "decodeLayerAsset", layerId: layer.id })),
    ];
    const payload = metadata.layers.find((layer) => layer.type === 4 || layer.type === 5);
    if (payload !== undefined) commands.push({ type: "decodeLayerAsset", layerId: payload.id });

    const stagedStarted = performance.now();
    let decodedStrokeCount = 0;
    for (const command of commands) {
        response = await request(command);
        if (response.delta?.type === "drawing") decodedStrokeCount += response.delta.drawing.strokeCount;
    }
    const stagedMs = performance.now() - stagedStarted;
    if (decodedStrokeCount === 0) throw new Error("Staged paint command returned no strokes");
    await request({ type: "release" });
    console.log(JSON.stringify({
        sourceBytes: file.byteLength,
        metadataMs: Math.round(metadataMs),
        stagedProbeMs: Math.round(stagedMs),
        commandCount: commands.length,
        layerCount: metadata.layers.length,
        paintLayerCount: metadata.layers.filter((layer) => layer.type === 1).length,
    }));
} finally {
    await worker.terminate();
}
