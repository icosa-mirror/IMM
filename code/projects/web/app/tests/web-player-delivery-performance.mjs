import { measureScenePlayback } from "./scene-playback-probe.mjs";
import { captureScenePixels } from "./scene-pixels.mjs";
import { readFile, writeFile, stat } from "node:fs/promises";
import { resolve } from "node:path";
import { cpus } from "node:os";
import { chromium } from "playwright-core";
import { createServer } from "vite";
import { startPerformanceTrace } from "./performance-trace.mjs";

const args = process.argv.slice(2);
const option = (key, fallback) => args.includes(key) ? args[args.indexOf(key) + 1] : fallback;
const root = resolve(import.meta.dirname, "../../../..", "..");
const paths = JSON.parse(await readFile(resolve(option("--paths", resolve(root, "artifacts/rebalance-phase3-corpus-paths.json"))), "utf8"));
const sources = await Promise.all(Object.values(paths).map(async path => ({path, bytes:(await stat(path)).size})));
sources.sort((a,b) => a.bytes - b.bytes);
const output = resolve(option("--output", resolve(root, "artifacts/rebalance-controlled.json")));
const rounds = Number(option("--rounds", "3"));
const roundOffset = Number(option("--round-offset", "0"));
const traced = args.includes("--trace");
const only = option("--workload", "all");
const evaluationComparison = args.includes("--evaluation-comparison");
const seconds = Number(option("--time-seconds", "1"));
const gallerySettings = args.includes("--match-gallery");
const audio = !gallerySettings || args.includes("--audio");
const scenePlayback = args.includes("--scene-playback");
const sceneDuration = Number(option("--scene-duration", "30"));
const modes = evaluationComparison ? ["owned", "reusable"] : ["single", "single-paced", "batch-paced"];
const server = await createServer({root:resolve(import.meta.dirname,".."),
    cacheDir:resolve(root,"artifacts/rebalance-vite-comparison-cache"),
    server:{host:"127.0.0.1",port:0}});
await server.listen();
const port = server.httpServer.address().port;
const browser = await chromium.launch({channel:"chrome",headless:false,args:[]});
const report = { browserVersion:browser.version(), cpu:cpus()[0]?.model, profile:"temporary", headless:false,
    viewport:gallerySettings?{width:1440,height:900}:{width:1280,height:720}, seconds, audio, gallerySettings, trace:traced, rounds, evaluationComparison, results:[] };
try {
    for (let round=roundOffset; round<roundOffset+rounds; round++) {
        for (const [sourceIndex,source] of sources.entries()) {
            const workload = sourceIndex === 0 ? "medium" : "upper";
            if (only !== "all" && only !== workload) continue;
            for (let offset=0;offset<modes.length;offset++) {
                const mode = modes[(round+offset)%modes.length];
                const context = await browser.newContext({viewport:report.viewport,deviceScaleFactor:1});
                const page = await context.newPage();
                const errors=[];
                page.on("pageerror", error=>errors.push(error.message));
                page.on("console", message=>{if(message.text().startsWith("IMM_SCENE_20260907:")) console.log(message.text()); if(message.type()==="error") errors.push(message.text());});
                const params=new URLSearchParams({src:"","visual-test":"1","benchmark-delivery":evaluationComparison?"single":mode,
                    "benchmark-evaluation":evaluationComparison?mode:"owned",
                    "benchmark-time-seconds":String(seconds),"benchmark-audio":audio?"1":"0",
                    "benchmark-gallery-camera":gallerySettings?"1":"0","benchmark-fixed-step":scenePlayback?"1":"0",
                    "benchmark-background-limit":sourceIndex===0?"357":"600","benchmark-trace":traced?"1":"0"});
                await page.goto(`http://127.0.0.1:${port}/?${params}`);
                const stopTrace = traced ? await startPerformanceTrace(page,`${output}.${workload}.${mode}.${round}.trace.json`) : null;
                await page.evaluate(()=>{
                    window.__deliveryFrames=[];
                    window.__deliveryTasks=[];
                    window.__deliveryPeakHeap=0;
                    window.__deliveryObserver=new PerformanceObserver(list=>{
                        for(const entry of list.getEntries()) window.__deliveryTasks.push({start:entry.startTime,duration:entry.duration});
                    });
                    window.__deliveryObserver.observe({type:"longtask",buffered:true});
                    let previous=performance.now();
                    function sample(now) {
                        window.__deliveryFrames.push({start:previous,end:now,duration:now-previous});
                        previous=now;
                        window.__deliveryPeakHeap=Math.max(window.__deliveryPeakHeap,performance.memory?.usedJSHeapSize??0);
                        window.__deliveryRAF=requestAnimationFrame(sample);
                    }
                    window.__deliveryRAF=requestAnimationFrame(sample);
                });
                if (scenePlayback) await page.evaluate(async ({ seconds, duration }) => {
                    const { ImmFrameEvaluator } = await import("/src/runtime/imm-playback.ts");
                    const { selectSceneResources } = await import("/tests/gallery-scene-probes.mjs");
                    window.__immSelectBenchmarkWork = (document, work) => {
                        const selected = selectSceneResources(document, work, new ImmFrameEvaluator(document), seconds, duration);
                        window.__expectedSceneItems = selected.length;
                        return selected;
                    };
                    document.querySelector("#viewport").style.pointerEvents = "none";
                }, { seconds, duration: sceneDuration });
                await page.setInputFiles("#file-input",source.path);
                await page.waitForFunction(()=>window.__immDeliveryDiagnostics().completedAt>0,undefined,{timeout:300000,polling:100});
                await page.evaluate(() => new Promise(done => requestAnimationFrame(() => requestAnimationFrame(done))));
                const result=await page.evaluate(async()=>{
                    cancelAnimationFrame(window.__deliveryRAF);
                    window.__deliveryTasks.push(...window.__deliveryObserver.takeRecords().map(e=>({start:e.startTime,duration:e.duration})));
                    window.__deliveryObserver.disconnect();
                    const delivery=window.__immDeliveryDiagnostics();
                    const frames=window.__deliveryFrames.filter(f=>f.end>=delivery.startedAt&&f.start<=delivery.completedAt).map(f=>f.duration).sort((a,b)=>a-b);
                    const tasks=window.__deliveryTasks.filter(t=>t.start+t.duration>=delivery.startedAt&&t.start<=delivery.completedAt);
                    return {delivery, elapsedMs:delivery.completedAt-delivery.startedAt,
                        frameCount:frames.length, meanFrameMs:frames.reduce((a,b)=>a+b,0)/frames.length,
                        p95FrameMs:frames[Math.min(frames.length-1,Math.floor(frames.length*.95))]??0,
                        maxFrameMs:frames.at(-1)??0, worstLongTaskMs:Math.max(0,...tasks.map(t=>t.duration)),
                        longTasks:tasks, peakJsHeapBytes:window.__deliveryPeakHeap,
                        playback:window.__immPlayback.snapshot(), diagnostics:window.__immDiagnostics(), decoder:await window.__immDecoderDiagnostics()};
                });
                if(stopTrace) await stopTrace();
                const expected=scenePlayback ? await page.evaluate(() => window.__expectedSceneItems) : sourceIndex===0?357:600;
                if(result.delivery.items!==expected||result.diagnostics.effectiveMode!=="staged"||errors.length
                    ||result.playback.playing||result.playback.timeTicks!==Math.round(seconds*result.playback.ticksPerSecond)) {
                    throw new Error(`Invalid ${workload}/${mode} trial: items=${result.delivery.items}, errors=${errors.length}`);
                }
                result.pixels = await captureScenePixels(page,args.includes("--screenshots")?`${output}.${workload}.${mode}.${round}.png`:undefined);
                if (args.includes("--measure-playback")) result.playbackSample = await page.evaluate(measureScenePlayback,
                    { harness: "app", seconds, segmentSeconds: sceneDuration });
                report.results.push({round,workload,mode,sourceBytes:source.bytes,...result,errors});
                await writeFile(output,JSON.stringify(report,null,2));
                process.stdout.write(`IMM_REBALANCE: ${workload} ${mode} round=${round+1} items=${result.delivery.items} elapsedMs=${result.elapsedMs.toFixed(1)} worstTaskMs=${result.worstLongTaskMs}\n`);
                await context.close();
            }
        }
    }
    for (const workload of new Set(report.results.map(r => r.workload))) {
        const rows = report.results.filter(r => r.workload === workload);
        if (new Set(rows.map(r => JSON.stringify([r.delivery.items, r.delivery.packetBytes, r.playback.timeTicks, r.pixels.hash, r.playbackSample?.workHash]))).size !== 1) {
            throw new Error("Browser-app resource work, authored time, or rendered pixels differ");
        }
    }
} finally { await browser.close(); await server.close(); }
