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
const traced = args.includes("--trace");
const only = option("--workload", "all");
const evaluationComparison = args.includes("--evaluation-comparison");
const modes = evaluationComparison ? ["owned", "reusable"] : ["single", "single-paced", "batch-paced"];
const server = await createServer({root:resolve(import.meta.dirname,".."),server:{host:"127.0.0.1",port:4191,strictPort:true}});
await server.listen();
const browser = await chromium.launch({channel:"chrome",headless:false,args:["--window-size=1280,720","--disable-background-timer-throttling"]});
const report = { browserVersion:browser.version(), cpu:cpus()[0]?.model, profile:"temporary", headless:false,
    viewport:{width:1280,height:720}, trace:traced, rounds, evaluationComparison, results:[] };
try {
    for (let round=0; round<rounds; round++) {
        for (const [sourceIndex,source] of sources.entries()) {
            const workload = sourceIndex === 0 ? "medium" : "upper";
            if (only !== "all" && only !== workload) continue;
            for (let offset=0;offset<modes.length;offset++) {
                const mode = modes[(round+offset)%modes.length];
                const context = await browser.newContext({viewport:report.viewport,deviceScaleFactor:1});
                const page = await context.newPage();
                const errors=[];
                page.on("pageerror", error=>errors.push(error.message));
                page.on("console", message=>{if(message.type()==="error") errors.push(message.text());});
                const params=new URLSearchParams({src:"","visual-test":"1","benchmark-delivery":evaluationComparison?"single":mode,
                    "benchmark-evaluation":evaluationComparison?mode:"owned",
                    "benchmark-background-limit":sourceIndex===0?"357":"600","benchmark-trace":traced?"1":"0"});
                await page.goto(`http://127.0.0.1:4191/?${params}`);
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
                await page.setInputFiles("#file-input",source.path);
                await page.waitForFunction(()=>window.__immDeliveryDiagnostics().completedAt>0,undefined,{timeout:300000,polling:100});
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
                const expected=sourceIndex===0?357:600;
                if(result.delivery.items!==expected||result.diagnostics.effectiveMode!=="staged"||errors.length
                    ||result.playback.playing||result.playback.timeTicks!==0) {
                    throw new Error(`Invalid ${workload}/${mode} trial: items=${result.delivery.items}, errors=${errors.length}`);
                }
                report.results.push({round,workload,mode,sourceBytes:source.bytes,...result,errors});
                await writeFile(output,JSON.stringify(report,null,2));
                process.stdout.write(`IMM_REBALANCE: ${workload} ${mode} round=${round+1} items=${result.delivery.items} elapsedMs=${result.elapsedMs.toFixed(1)} worstTaskMs=${result.worstLongTaskMs}\n`);
                await context.close();
            }
        }
    }
} finally { await browser.close(); await server.close(); }
