import { readFile, writeFile, stat } from "node:fs/promises";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { Worker } from "node:worker_threads";
import { serialize, deserialize } from "node:v8";
import { createNativeLoadOrder } from "../src/staged-loading";
import { ImmFrameEvaluator } from "../src/runtime/imm-playback";
import type { ImmDocument } from "../src/format/imm-document";

const root = process.cwd();
const paths = JSON.parse(await readFile(resolve(root, "artifacts/rebalance-phase3-corpus-paths.json"), "utf8"));
const privateSources = await Promise.all(Object.values(paths).map(async path => ({ path: String(path), bytes: (await stat(String(path))).size })));
privateSources.sort((a, b) => a.bytes - b.bytes);
const sources = [
    { label: "sample1.imm", path: resolve(root, "exampleImmFiles/sample1.imm") },
    { label: "medium", path: privateSources[0]!.path },
    { label: "upper", path: privateSources[1]!.path },
];
const results = [];
for (const source of sources) {
    let worker: Worker | undefined;
    try {
        let document: ImmDocument;
        if (process.argv.includes("--cached")) {
            document = deserialize(await readFile(resolve(root, `artifacts/loading-demand-${source.label}.metadata.bin`)));
        } else {
            worker = new Worker(pathToFileURL(resolve(root, "code/projects/web/app/dist-library/decoder/imm-web-decoder-worker.mjs")));
            const bytes = await readFile(source.path);
            const buffer = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
            document = await new Promise<ImmDocument>((resolveDocument, reject) => {
                worker!.once("error", reject);
                worker!.once("message", response => response.ok ? resolveDocument(response.document) : reject(new Error(response.error?.message)));
                worker!.postMessage({ requestId: 1, type: "openMetadata", source: buffer }, [buffer]);
            });
        }
        await writeFile(resolve(root, `artifacts/loading-demand-${source.label}.metadata.bin`), serialize(document));
        const work = createNativeLoadOrder(document);
        const key = (item: (typeof work)[number]) => item.type === "drawing" ? `drawing:${item.layerId}:${item.drawingId}` : `asset:${item.layerId}`;
        const initial = new Set(work.filter(w => w.initial).map(key));
        const rank = new Map(work.map((w, i) => [key(w), i]));
        const evaluator = new ImmFrameEvaluator(document);
        const intervals = [];
        for (const [startSeconds, duration] of [[0, 5], [0, 30], ...(source.label === "upper" ? [[821.4375396825396, 30]] : [])]) {
            const needed = new Set<string>();
            const types = new Map<number, number>();
            for (let frame = 0; frame <= duration! * 60; frame++) {
                const snapshot = evaluator.evaluateFrame(Math.round((startSeconds! + frame / 60) * document.ticksPerSecond));
                for (const state of snapshot.layers.values()) {
                    if (!state.visible) continue;
                    const layer = state.layer;
                    let id;
                    if (layer.type === 1 && state.drawingIndex >= 0) id = `drawing:${layer.id}:${state.drawingIndex}`;
                    else if ([3, 4, 5, 8].includes(layer.type)) id = `asset:${layer.id}`;
                    if (id && !needed.has(id)) { needed.add(id); types.set(layer.type, (types.get(layer.type) ?? 0) + 1); }
                }
            }
            const lastNeededRank = Math.max(-1, ...[...needed].map(id => rank.get(id) ?? -1));
            intervals.push({ startSeconds, duration, samplingHz: 60, needed: needed.size,
                initiallyNeeded: [...needed].filter(id => initial.has(id)).length,
                missingFromInitial: [...needed].filter(id => !initial.has(id)).length,
                lastNeededRank, earlierUnneeded: work.slice(0, lastNeededRank + 1).filter(w => !needed.has(key(w))).length,
                types: Object.fromEntries(types) });
        }
        const result = { workload: source.label, layerCount: document.layers.length, totalResources: work.length,
            initialResources: initial.size, intervals };
        results.push(result);
        await writeFile(resolve(root, "artifacts/loading-demand-audit.json"), JSON.stringify({
            note: "Sampled visibility audit, not an exact scheduling algorithm or load-time benchmark. Includes zero-opacity visible layers and sound.", results }, null, 2));
        console.log(`IMM_DEMAND_20260907: ${source.label}: ${initial.size} initial, ${work.length} total; first 5 s needs ${intervals[0]!.needed} sampled resources.`);
    } finally { await worker?.terminate(); }
}
