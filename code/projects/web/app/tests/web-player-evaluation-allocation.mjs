import {readFile,writeFile,stat} from "node:fs/promises";
import {resolve} from "node:path";
import {chromium} from "playwright-core";
import {createServer} from "vite";
const root=resolve(import.meta.dirname,"../../../../..");
const mapping=JSON.parse(await readFile(resolve(root,"artifacts/rebalance-phase3-corpus-paths.json"),"utf8"));
const sources=await Promise.all(Object.values(mapping).map(async path=>({path,bytes:(await stat(path)).size})));
const source=sources.sort((a,b)=>b.bytes-a.bytes)[0];
const server=await createServer({root:resolve(import.meta.dirname,".."),server:{host:"127.0.0.1",port:4192,strictPort:true}});
await server.listen();
const browser=await chromium.launch({channel:"chrome",headless:false});
const report={browserVersion:browser.version(),timings:[],allocations:[]};
try {
 const context=await browser.newContext();const page=await context.newPage();
 const session=await context.newCDPSession(page);
 await page.goto("http://127.0.0.1:4192/tests/evaluation-fixture.html");
 await page.setInputFiles("#file-input",source.path);
 report.metadata=await page.evaluate(()=>window.__loadEvaluationDocument());
 const modes=["reference","owned","reusable"];
 for(const mode of modes)await page.evaluate(m=>window.__evaluationSample(m,20),mode);
 for(let round=0;round<3;round++) for(let offset=0;offset<3;offset++) {
  const mode=modes[(round+offset)%3];await session.send("HeapProfiler.collectGarbage");
  const sample=await page.evaluate(m=>window.__evaluationSample(m,100),mode);
  report.timings.push({mode,round,...sample});
 }
 for(const mode of modes) {
  await session.send("HeapProfiler.collectGarbage");
  await session.send("HeapProfiler.startSampling",{samplingInterval:16384,
   includeObjectsCollectedByMajorGC:true,includeObjectsCollectedByMinorGC:true});
  await page.evaluate(m=>window.__evaluationSample(m,100),mode);
  const {profile}=await session.send("HeapProfiler.stopSampling");
  await writeFile(resolve(root,`artifacts/rebalance-allocation-${mode}.json`),JSON.stringify(profile));
  const functions=new Map();let totalBytes=0;
  function visit(node){totalBytes+=node.selfSize;const name=node.callFrame.functionName||"(anonymous)";
   functions.set(name,(functions.get(name)||0)+node.selfSize);for(const child of node.children)visit(child);}
  visit(profile.head);
  report.allocations.push({mode,totalBytes,topFunctions:[...functions].sort((a,b)=>b[1]-a[1]).slice(0,12)});
 }
 await writeFile(resolve(root,"artifacts/rebalance-evaluation-allocation.json"),JSON.stringify(report,null,2));
 console.log("IMM_REBALANCE: allocation sampling and owned-API comparison complete");
}finally{await browser.close();await server.close();}
