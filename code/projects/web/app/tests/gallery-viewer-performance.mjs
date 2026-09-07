import { measureScenePlayback } from "./scene-playback-probe.mjs";
import { inspectGalleryScenes } from "./gallery-scene-probes.mjs";
import { captureScenePixels } from "./scene-pixels.mjs";
import { installDrawingChurnProbe } from "./drawing-churn-probe.mjs";
import { instrumentDrawingActivation, installDrawingActivationTimer } from "./drawing-activation-timer.mjs";
import { createReadStream } from "node:fs";
import { readFile, writeFile, stat } from "node:fs/promises";
import { createServer } from "node:http";
import { resolve, extname, sep } from "node:path";
import { chromium } from "playwright-core";

const args = process.argv.slice(2);
const option = (key, fallback) => args.includes(key) ? args[args.indexOf(key) + 1] : fallback;
const root = resolve(import.meta.dirname, "../../../../..");
const gallery = resolve(option("--gallery", resolve(root, "../gallery-viewer")));
const libraryComparison = args.includes("--library-comparison");
const navigationAudit = args.includes("--navigation-audit");
const batchingComparison = args.includes("--batching-comparison");
const initialComparison = args.includes("--initial-load-comparison");
const audioEnabled = args.includes("--audio");
const loadingComparison = args.includes("--loading-comparison") || batchingComparison || initialComparison || navigationAudit || libraryComparison;
const activationTimer = args.includes("--activation-timer");
if (libraryComparison && !args.includes("--candidate-library")) {
    throw new Error("Library comparisons require an explicit --candidate-library artifact");
}
if (initialComparison && !args.includes("--baseline-library")) {
    throw new Error("Initial comparisons require an explicit --baseline-library artifact");
}
const workload = option("--workload", "sample1.imm");
let input = resolve(root, "exampleImmFiles/sample1.imm");
if (workload !== "sample1.imm") {
    const paths = JSON.parse(await readFile(option("--paths", resolve(root, "artifacts/rebalance-phase3-corpus-paths.json")), "utf8"));
    const sources = await Promise.all(Object.values(paths).map(async path => ({ path, bytes: (await stat(path)).size })));
    sources.sort((a, b) => a.bytes - b.bytes);
    if (!["medium", "upper"].includes(workload)) throw new Error("Unknown workload");
    input = sources[workload === "medium" ? 0 : 1].path;
}
const output = option("--output", resolve(root, `artifacts/rebalance-gallery-${workload}${loadingComparison ? "-loading" : ""}.json`));
const routes = [["/imm-library/", resolve(option("--library-dir", resolve(root, "code/projects/web/app/dist-library")))],
    ["/imm-decoder/", resolve(root, "code/projects/web/app/dist-library/decoder")]];
const mime = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript",
    ".css": "text/css", ".json": "application/json", ".wasm": "application/wasm", ".svg": "image/svg+xml" };
const server = createServer(async (request, response) => {
    const pathname = decodeURIComponent(new URL(request.url, "http://localhost").pathname);
    try {
        if (pathname === "/imm-scene-probes.mjs") {
            response.writeHead(200, { "Content-Type": "text/javascript" }).end(await readFile(resolve(import.meta.dirname, "gallery-scene-probes.mjs")));
            return;
        }
        if (pathname === "/imm-loading-probe.mjs") {
            response.writeHead(200, { "Content-Type": "text/javascript" }).end(await readFile(resolve(import.meta.dirname, navigationAudit ? "gallery-navigation-probe.mjs" : "gallery-viewer-loading-probe.mjs")));
            return;
        }
        if (pathname === "/workload.imm") {
            response.writeHead(200, { "Content-Type": "application/octet-stream", "Content-Length": (await stat(input)).size });
            createReadStream(input).pipe(response); return;
        }
        let base = gallery, relative = `.${pathname}`;
        const route = routes.find(([prefix]) => pathname.startsWith(prefix));
        if (route) { base = route[1]; relative = pathname.slice(route[0].length); }
        const path = resolve(base, relative);
        if (path !== base && !path.startsWith(`${base}${sep}`)) { response.writeHead(403).end(); return; }
        if (pathname === "/test/browser-imm.html") {
            let html = (await readFile(path, "utf8")).replace('"/sample1.imm"', '"/workload.imm"').replace("audio: true", `audio: ${audioEnabled}`);
            if (loadingComparison) html = html.replace('window.__galleryImm.ready = true;', `
                if (viewer.cameraControls) { viewer.cameraControls.update(0); viewer.cameraControls.enabled = false; viewer.cameraControls.update = () => false; }
                if (viewer.trackballControls) { viewer.trackballControls.enabled = false; viewer.trackballControls.update = () => {}; }
                window.__galleryImm.ready = true;`);
            if (loadingComparison) html = html.replace('import { Viewer }', 'import "/imm-loading-probe.mjs";\n    import { Viewer }');
            response.writeHead(200, { "Content-Type": "text/html" }).end(html); return;
        }
        if (!(await stat(path)).isFile()) throw new Error("Not a file");
        if (activationTimer && pathname === "/imm-library/imm-three-loader.js") {
            response.writeHead(200, { "Content-Type": "text/javascript" }).end(instrumentDrawingActivation(await readFile(path, "utf8")));
            return;
        }
        response.writeHead(200, { "Content-Type": mime[extname(path)] ?? "application/octet-stream" });
        createReadStream(path).pipe(response);
    } catch { response.writeHead(404).end(); }
});
await new Promise(done => server.listen(0, "127.0.0.1", done));
let browser;
try {
    browser = await chromium.launch({ channel: "chrome", headless: false });
    if (navigationAudit) {
        const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
        try {
            const errors = [];
            page.on("pageerror", error => errors.push(error.message));
            await page.goto(`http://127.0.0.1:${server.address().port}/test/browser-imm.html?time-seconds=${Number(option("--time-seconds", "821.4375396825396"))}&item-budget=${Number(option("--navigation-items", "2048"))}&budget-ms=${Number(option("--navigation-budget-ms", "30000"))}`);
            await page.waitForFunction(() => window.__galleryImm?.ready && window.__immNavigation?.begin, null, { timeout: 180_000 });
            await page.evaluate(() => window.__immNavigation.begin());
            await page.waitForFunction(() => window.__immNavigation.done, null, { timeout: 90_000 });
            const result = await page.evaluate(() => ({ ...window.__immNavigation, begin: undefined,
                diagnostics: window.__galleryImmDiagnostics() }));
            await page.screenshot({ path: `${output}.png` });
            const { createHash } = await import("node:crypto");
            const hashes = {};
            for (const [label, path] of Object.entries({ library: `${routes[0][1]}/imm-three-loader.js`,
                worker: `${routes[1][1]}/imm-web-decoder-worker.mjs`, wasm: `${routes[1][1]}/imm-web-decoder.wasm`,
                host: `${gallery}/dist/icosa-viewer.module.js`, input })) {
                const hash = createHash("sha256");
                for await (const chunk of createReadStream(path)) hash.update(chunk);
                hashes[label] = hash.digest("hex");
            }
            await writeFile(output, JSON.stringify({ workload, browser: browser.version(), profile: "fresh temporary",
                audio: audioEnabled, hashes, result, errors }, null, 2));
            if (errors.length || result.error) throw new Error(`Navigation diagnostic failed: ${errors.join("; ")} ${result.error ?? ""}`);
            console.log(`IMM_NAV_20260907: ${result.reason}, ${Math.round(result.elapsedMs)} ms, first scene missing ${result.firstMissing}/${result.firstRequired}, five-second window missing ${result.windowMissing}/${result.windowRequired}`);
        } finally { await page.close(); }
    } else if (loadingComparison) {
        const results = [];
        const rounds = Number(option("--rounds", "3"));
        const roundOffset = Number(option("--round-offset", "0"));
        const limit = option("--background-limit", workload === "upper" ? "600" : "Infinity");
        const seconds = Number(option("--time-seconds", "1"));
        if (!Number.isInteger(rounds) || rounds < 1 || !Number.isFinite(seconds) || seconds < 0
            || (Number(limit) !== Infinity && (!Number.isInteger(Number(limit)) || Number(limit) < 1))) {
            throw new Error("Invalid loading comparison rounds, time, or background limit");
        }
        const sceneDuration = Number(option("--scene-duration", "30"));
        const selection = args.includes("--scene-playback") ? "scene" : "slice";
        const drawingComparison = args.includes("--drawing-churn");
        const modes = libraryComparison ? ["baseline", "candidate"] : initialComparison ? ["native", "visibility"] : drawingComparison ? ["baseline", "instrumented"] : batchingComparison ? ["single", "batch-paced"] : ["owned", "reusable"];
        const report = { workload, seconds, sceneDuration, selection, browser: browser.version(), profile: "temporary", audio: audioEnabled, batchingComparison,
            viewport: { width: 1440, height: 900 }, backgroundLimit: limit, initialComparison, libraryComparison, results };
        for (let round = roundOffset; round < roundOffset + rounds; round++) {
            for (const mode of round % 2 ? [...modes].reverse() : modes) {
                const page = await browser.newPage({ viewport: report.viewport });
                const errors = [];
                page.on("pageerror", error => errors.push(error.message));
                page.on("console", message => { if (message.text().startsWith("IMM_SCENE_20260907:")) console.log(message.text()); });
                try {
                    if (libraryComparison && mode === "candidate") {
                        await page.route("**/imm-library/imm-three-loader.js", route => route.fulfill({
                            path: resolve(option("--candidate-library", "artifacts/navigation-audio-candidate/imm-three-loader.js")),
                            contentType: "text/javascript",
                        }));
                    }
                    if (initialComparison && mode === "native") {
                        await page.route("**/imm-library/imm-three-loader.js", route => route.fulfill({
                            path: resolve(option("--baseline-library", resolve(root, "artifacts/initial-demand-baseline-library.js"))),
                            contentType: "text/javascript",
                        }));
                    }
                    await page.goto(`http://127.0.0.1:${server.address().port}/test/browser-imm.html?evaluation=${batchingComparison ? "reusable" : mode}&delivery=${drawingComparison || initialComparison || libraryComparison ? "batch-paced" : batchingComparison ? mode : "single"}&background-limit=${limit}&time-seconds=${seconds}&resource-selection=${selection}&scene-duration=${sceneDuration}&initial-only=${initialComparison ? 1 : 0}&native-response=${args.includes("--native-response") ? 1 : 0}`);
                    await page.waitForFunction(() => window.__galleryLoading?.done && window.__galleryImm?.ready, null, { timeout: 240_000 });
                    const result = await page.evaluate(() => ({ ...window.__galleryLoading, diagnostics: window.__galleryImmDiagnostics() }));
                    if (result.error || errors.length || result.telemetry.effectiveMode !== "staged"
                        || result.telemetry.completedItems !== result.selectedDeferred || result.playing || result.timeTicks !== result.expectedTicks) {
                        throw new Error(`Gallery loading comparison failed: ${result.error ?? errors.join("; ")}`);
                    }
                    if (initialComparison) {
                        await page.waitForFunction(() => {
                            const overlay = document.getElementById("loadscreen");
                            return !overlay || Number(getComputedStyle(overlay).opacity) === 0;
                        });
                        result.visibleAt = await page.evaluate(() => performance.now());
                    }
                    result.pixels = await captureScenePixels(page, args.includes("--screenshots") ? `${output}.${mode}.${round}.png` : undefined);
                    if (initialComparison) result.initialFrames = await page.evaluate(async () => {
                        const viewer = window.__galleryImm.viewer, asset = viewer.immAsset;
                        const frames = [];
                        for (const seconds of [0, .17, .5, 1, 2.5, 5]) {
                            asset.seek(seconds);
                            await new Promise(done => requestAnimationFrame(() => requestAnimationFrame(done)));
                            frames.push({ seconds, drawCalls: viewer.renderer.info.render.calls, triangles: viewer.renderer.info.render.triangles });
                        }
                        return frames;
                    });
                    if (args.includes("--inspect-scenes")) result.sceneCandidates = await page.evaluate(inspectGalleryScenes);
                    if (args.includes("--measure-playback")) {
                        if (mode === "instrumented") result.drawingInstrumentation = await page.evaluate(activationTimer ? installDrawingActivationTimer : installDrawingChurnProbe);
                        result.playbackSample = await page.evaluate(measureScenePlayback, { harness: "gallery", seconds, segmentSeconds: sceneDuration });
                        if (mode === "instrumented") result.drawingInventory = await page.evaluate(() => {
                            const inventory = window.__immDrawingProbe.inventory();
                            window.__immDrawingProbe.restore();
                            return inventory;
                        });
                    }
                    results.push({ round, ...result, mode });
                    await writeFile(output, JSON.stringify(report, null, 2));
                    console.log(initialComparison
                        ? `IMM_DEMAND_20260907: Gallery ${workload} ${mode} round ${round + 1}: ${result.telemetry.initiallyLoadedItems} initial resources, ${Math.round(result.readyAt - result.startedAt)} ms ready.`
                        : `Gallery ${workload} ${mode} round ${round + 1}: ${result.selectedDeferred}/${result.originalDeferred} deferred resources, ${Math.round(result.completedAt - result.readyAt)} ms background.`);
                } finally { await page.close(); }
            }
        }
        if (libraryComparison) {
            const { createHash } = await import("node:crypto");
            report.hashes = {};
            for (const [label, path] of Object.entries({ baselineLibrary: `${routes[0][1]}/imm-three-loader.js`,
                candidateLibrary: resolve(option("--candidate-library")), worker: `${routes[1][1]}/imm-web-decoder-worker.mjs`,
                wasm: `${routes[1][1]}/imm-web-decoder.wasm`, host: `${gallery}/dist/icosa-viewer.module.js`, input })) {
                const hash = createHash("sha256");
                for await (const chunk of createReadStream(path)) hash.update(chunk);
                report.hashes[label] = hash.digest("hex");
            }
            await writeFile(output, JSON.stringify(report, null, 2));
        }
        if (new Set(results.map(x => JSON.stringify(initialComparison
            ? [x.timeTicks, x.pixels.hash, x.initialFrames]
            : [x.selectedDeferred, x.telemetry.packetBytes, x.telemetry.initiallyLoadedItems, x.resources, x.timeTicks, x.pixels.hash, x.playbackSample?.workHash]))).size !== 1) {
            throw new Error("Gallery loading comparison resource work, authored time, or rendered pixels differ");
        }
        console.log(initialComparison ? `Gallery ${workload} initial loading comparison passed with equal initial render work and pixels.`
            : `Gallery ${workload} loading comparison passed with equal resource work.`);
    } else {
    const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
    const errors = [];
    page.on("pageerror", error => errors.push(error.message));
    await page.goto(`http://127.0.0.1:${server.address().port}/test/browser-imm.html`);
    await page.waitForFunction(() => window.__galleryImm?.ready, null, { timeout: 180_000 });
    await page.evaluate(async () => { window.__galleryImm.viewer.immAsset.pause(); await window.__galleryImm.viewer.immAsset.backgroundComplete; });
    const validation = await page.evaluate(() => {
        const viewer = window.__galleryImm.viewer, asset = viewer.immAsset;
        const values = snapshot => JSON.stringify([snapshot.timeTicks, snapshot.chapterIndex, snapshot.waiting,
            [...snapshot.layers].map(([id, s]) => [id, s.timelineTicks, s.localTimeTicks, s.visible, s.opacity,
                s.transform, s.worldTransform, s.drawInTime, s.drawingIndex])]);
        let previous, comparisons = 0;
        for (const seconds of [0, asset.durationSeconds / 2, ...asset.chapters.map(c => c.startTicks / asset.document.ticksPerSecond), 0]) {
            asset.seek(seconds);
            const frame = asset.update(0, viewer.activeCamera);
            if (previous && previous !== frame.snapshot) throw new Error("IMMAsset.update did not reuse its frame");
            if (values(frame.snapshot) !== values(asset.playback.evaluate())) throw new Error("Asset frame differs from owned evaluation");
            previous = frame.snapshot; comparisons++;
        }
        return { comparisons, reused: true, layers: asset.document.layers.length, ...window.__galleryImmDiagnostics() };
    });
    const results = [];
    for (let round = 0; round < 3; round++) {
        for (const mode of round % 2 ? ["reusable", "owned"] : ["owned", "reusable"]) {
            results.push(await page.evaluate(async ({ mode, round }) => {
                const viewer = window.__galleryImm.viewer, asset = viewer.immAsset;
                const originalAdvance = asset.playback.advanceFrame;
                if (mode === "owned") asset.playback.advanceFrame = function(delta) { return this.advance(delta); };
                asset.restart(); asset.play();
                const originalUpdater = viewer.contentUpdater;
                const costs = [], intervals = [];
                let count = 0, previous;
                try {
                    await new Promise((done, reject) => {
                        const timeout = setTimeout(() => { viewer.contentUpdater = originalUpdater; reject(new Error("Gallery frame sampling timed out")); }, 120_000);
                        viewer.contentUpdater = (_time, camera) => {
                            try {
                                const start = performance.now();
                                originalUpdater(count * 1000 / 60, camera);
                                const cost = performance.now() - start;
                                if (count >= 30) { costs.push(cost); intervals.push(start - previous); }
                                previous = start;
                                if (++count === 150) { clearTimeout(timeout); viewer.contentUpdater = originalUpdater; done(); }
                            } catch (error) { clearTimeout(timeout); viewer.contentUpdater = originalUpdater; reject(error); }
                        };
                    });
                    const stats = items => {
                        const sorted = [...items].sort((a, b) => a - b);
                        return { mean: items.reduce((sum, x) => sum + x, 0) / items.length,
                            median: sorted[Math.floor(sorted.length / 2)], p95: sorted[Math.floor(sorted.length * .95)], max: sorted.at(-1) };
                    };
                    return { mode, round, samples: costs.length, updateMs: stats(costs), frameIntervalMs: stats(intervals),
                        timeTicks: asset.playback.timeTicks, drawCalls: viewer.renderer.info.render.calls, triangles: viewer.renderer.info.render.triangles };
                } finally { viewer.contentUpdater = originalUpdater; asset.playback.advanceFrame = originalAdvance; asset.pause(); }
            }, { mode, round }));
        }
    }
    if (errors.length) throw new Error(`Gallery page errors: ${errors.join("; ")}`);
    if (new Set(results.map(x => JSON.stringify([x.timeTicks, x.drawCalls, x.triangles]))).size !== 1) {
        throw new Error("Comparison transport or rendered work differs");
    }
    await writeFile(output, JSON.stringify({ workload, browser: browser.version(), profile: "temporary", audio: false,
        viewport: { width: 1440, height: 900 }, validation, results }, null, 2));
    console.log(`Gallery ${workload}: ${validation.comparisons} parity checks; reusable frames verified; ${results.length} playback samples saved.`);
    }
} finally { await browser?.close(); await new Promise(done => server.close(done)); }
