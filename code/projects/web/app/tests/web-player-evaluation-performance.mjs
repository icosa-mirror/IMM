import {readFile,writeFile,stat} from "node:fs/promises";
import {resolve} from "node:path";
import {chromium} from "playwright-core";
import {createServer} from "vite";
const root=resolve(import.meta.dirname,"../../../../..");
const mapping=JSON.parse(await readFile(resolve(root,"artifacts/rebalance-phase3-corpus-paths.json"),"utf8"));
const sources=await Promise.all(Object.values(mapping).map(async path=>({path,bytes:(await stat(path)).size})));
sources.sort((a,b)=>a.bytes-b.bytes);
const server=await createServer({root:resolve(import.meta.dirname,".."),server:{host:"127.0.0.1",port:4192,strictPort:true}});
await server.listen();
const browser=await chromium.launch({channel:"chrome",headless:false});
const report={browserVersion:browser.version(),profile:"temporary",results:[]};
try {
    for(const [index,source] of sources.entries()) {
        const context=await browser.newContext({viewport:{width:1280,height:720}});
        const page=await context.newPage();
        const session=await context.newCDPSession(page);
        await page.goto("http://127.0.0.1:4192/tests/evaluation-fixture.html");
        await page.setInputFiles("#file-input",source.path);
        const metadata=await page.evaluate(()=>window.__loadEvaluationDocument());
        for(const mode of ["reference","reusable"])await page.evaluate(m=>window.__evaluationSample(m,20),mode);
        for(let round=0;round<5;round++) {
            for(const mode of round%2===0?["reference","reusable"]:["reusable","reference"]) {
                await session.send("HeapProfiler.collectGarbage");
                const sample=await page.evaluate(m=>window.__evaluationSample(m,100),mode);
                report.results.push({workload:index===0?"medium":"upper",mode,round,...metadata,...sample});
                await writeFile(resolve(root,"artifacts/rebalance-evaluation-experiment.json"),JSON.stringify(report,null,2));
            }
        }
        console.log(`IMM_REBALANCE: evaluation workload ${index+1} parity=${metadata.comparisons} and ten timing samples complete`);
        await context.close();
    }
}finally{await browser.close();await server.close();}
