import { ImmDecoderClient } from "../src/decoder-client";
import { ImmFrameEvaluator, evaluateImmDocument as ownedEvaluate } from "../src/runtime/imm-playback";
import { evaluateImmDocument as referenceEvaluate } from "./fixtures/imm-playback-reference";
import type { ImmPlaybackSnapshot } from "../src/runtime/imm-playback";
import type { ImmDocument } from "../src/format/imm-document";

let document: ImmDocument;
let evaluator: ImmFrameEvaluator;
const api = window as unknown as Record<string, unknown>;
function values(snapshot: ImmPlaybackSnapshot) {
    return JSON.stringify([snapshot.timeTicks, snapshot.chapterIndex, snapshot.waiting,
        [...snapshot.layers].map(([id,s]) => [id,s.timelineTicks,s.localTimeTicks,s.visible,s.opacity,
            s.transform,s.worldTransform,s.drawInTime,s.drawingIndex])]);
}
api.__loadEvaluationDocument = async () => {
    const file = (window.document.querySelector("#file-input") as HTMLInputElement).files![0]!;
    const decoder = new ImmDecoderClient("/decoder/imm-web-decoder-worker.mjs");
    try { document = await decoder.openMetadata(await file.arrayBuffer()); await decoder.release(); }
    finally { decoder.dispose(); }
    const start = performance.now();
    evaluator = new ImmFrameEvaluator(document);
    const compileMs = performance.now() - start;
    let comparisons = 0;
    const ticks = new Set([0,document.durationTicks,...document.chapters.map(chapter=>chapter.startTicks)]);
    for(let i=0;i<32;i++) ticks.add(Math.round(document.durationTicks*i/31));
    for(const tick of ticks) {
        if(values(evaluator.evaluateFrame(tick))!==values(referenceEvaluate(document,tick))) {
            throw new Error(`Evaluation parity failed at tick ${tick}`);
        }
        comparisons++;
    }
    return {layers:document.layers.length, keys:document.layers.reduce((n,l)=>n+l.keys.length,0), compileMs, comparisons};
};
api.__evaluationSample = (mode: string, count: number) => {
    const start = performance.now();
    let snapshot: ImmPlaybackSnapshot;
    for(let i=0;i<count;i++) {
        const tick = Math.round(document.durationTicks * ((i*37)%count) / Math.max(1,count-1));
        snapshot = mode === "reference" ? referenceEvaluate(document,tick)
            : mode === "owned" ? ownedEvaluate(document,tick) : evaluator.evaluateFrame(tick);
    }
    api.__lastEvaluationFrame = snapshot!;
    return {elapsedMs:performance.now()-start, evaluations:count};
};
