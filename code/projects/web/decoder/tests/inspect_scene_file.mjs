import { readFile } from "node:fs/promises";
import { pathToFileURL } from "node:url";
import { Worker } from "node:worker_threads";

if (process.argv.length !== 4) throw new Error("Expected worker module and local IMM paths");
const worker = new Worker(pathToFileURL(process.argv[2]), { type: "module" });
try {
    const file = await readFile(process.argv[3]);
    const source = file.buffer.slice(file.byteOffset, file.byteOffset + file.byteLength);
    const decoded = await new Promise((resolve, reject) => {
        const timeout = setTimeout(() => reject(new Error("Timed out waiting for decoder worker")), 120_000);
        worker.once("error", reject);
        worker.once("message", (message) => {
            clearTimeout(timeout);
            resolve(message);
        });
        worker.postMessage({ requestId: 1, type: "decode", source }, [source]);
    });
    if (!decoded.ok) throw new Error(decoded.error.message);
    const layerTypes = Object.fromEntries([...decoded.document.layers.reduce((counts, layer) => {
        counts.set(layer.type, (counts.get(layer.type) ?? 0) + 1);
        return counts;
    }, new Map()).entries()].sort(([left], [right]) => left - right));
    const sounds = decoded.document.layers.filter((layer) => layer.sound !== undefined).map((layer) => ({
        id: layer.id,
        name: layer.name,
        ...layer.sound,
        bytes: layer.sound.bytes.byteLength,
    }));
    const models = decoded.document.layers.filter((layer) => layer.type === 3).map((layer) => ({
        id: layer.id,
        name: layer.name,
    }));
    console.log(JSON.stringify({ schemaVersion: decoded.document.schemaVersion, layerTypes, models, sounds }, null, 2));
    if (sounds.length === 0) process.exitCode = 2;
} finally {
    await worker.terminate();
}
