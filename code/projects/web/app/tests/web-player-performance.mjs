import { stat, writeFile } from "node:fs/promises";
import { basename, resolve } from "node:path";
import { chromium } from "playwright-core";
import { createServer } from "vite";

const args = process.argv.slice(2);
const outputIndex = args.indexOf("--output");
const outputPath = outputIndex < 0 ? null : resolve(args[outputIndex + 1] ?? "");
const filePaths = args
    .filter((_, index) => index !== outputIndex && index !== outputIndex + 1)
    .map((filePath) => resolve(filePath));
if (filePaths.length === 0) {
    throw new Error("Usage: node tests/web-player-performance.mjs [--output result.json] file.imm [...]");
}

const appRoot = resolve(process.env.IMM_WEB_APP_ROOT ?? resolve(import.meta.dirname, ".."));
const server = await createServer({
    root: appRoot,
    server: { host: "127.0.0.1", port: 4190, strictPort: true },
});
await server.listen();
const browser = await chromium.launch({
    channel: "chrome",
    headless: process.env.IMM_WEB_HEADLESS === "1",
    args: ["--window-size=1280,720", "--disable-background-timer-throttling"],
});

const results = [];
try {
    for (const filePath of filePaths) {
        const page = await browser.newPage({ viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 });
        const errors = [];
        page.on("console", (message) => {
            if (message.type() === "error") errors.push(message.text());
        });
        page.on("pageerror", (error) => errors.push(error.message));
        const loadStartedAt = performance.now();
        await page.goto("http://127.0.0.1:4190/?src=&visual-test=1&benchmark-eager=1");
        await page.setInputFiles("#file-input", filePath);
        await page.waitForFunction(
            () => window.__immDiagnostics?.().ready === true,
            undefined,
            { timeout: 300_000 },
        );
        const readyMs = performance.now() - loadStartedAt;
        const fileBytes = (await stat(filePath)).size;
        const settleMs = fileBytes >= 300 * 1024 * 1024 ? 45_000
            : fileBytes >= 100 * 1024 * 1024 ? 20_000
                : 5_000;
        await page.waitForTimeout(settleMs);
        const heavyPoint = await page.evaluate(async () => {
            window.__immPlayback.pause();
            const initial = window.__immPlayback.snapshot();
            const durationTicks = initial.durationTicks;
            const chapterCount = document.querySelectorAll("#chapter option").length;
            const samples = [];
            if (chapterCount > 0) {
                for (let chapterIndex = 0; chapterIndex < chapterCount; chapterIndex++) {
                    window.__immPlayback.selectChapter(chapterIndex);
                    window.__immPlayback.pause();
                    const chapterStartTicks = window.__immPlayback.snapshot().timeTicks;
                    for (const offsetSeconds of [0, 1, 5]) {
                        const timeTicks = Math.min(
                            durationTicks,
                            chapterStartTicks + offsetSeconds * initial.ticksPerSecond,
                        );
                        window.__immPlayback.seekTicks(timeTicks);
                        await new Promise((resolveFrame) => requestAnimationFrame(() => requestAnimationFrame(resolveFrame)));
                        const diagnostics = window.__immDiagnostics();
                        samples.push({
                            chapterIndex,
                            timeTicks,
                            drawCalls: diagnostics.drawCalls,
                            renderedTriangles: diagnostics.renderedTriangles,
                        });
                    }
                }
            } else {
                for (let index = 0; index < 25; index++) {
                    const timeTicks = Math.round(durationTicks * index / 24);
                    window.__immPlayback.seekTicks(timeTicks);
                    await new Promise((resolveFrame) => requestAnimationFrame(() => requestAnimationFrame(resolveFrame)));
                    const diagnostics = window.__immDiagnostics();
                    samples.push({
                        chapterIndex: -1,
                        timeTicks,
                        drawCalls: diagnostics.drawCalls,
                        renderedTriangles: diagnostics.renderedTriangles,
                    });
                }
            }
            samples.sort((left, right) =>
                right.renderedTriangles - left.renderedTriangles || right.drawCalls - left.drawCalls,
            );
            const selected = samples[0] ?? {
                chapterIndex: -1, timeTicks: 0, drawCalls: 0, renderedTriangles: 0,
            };
            window.__immPlayback.seekTicks(selected.timeTicks);
            await new Promise((resolveFrame) => requestAnimationFrame(() => requestAnimationFrame(resolveFrame)));
            return { durationTicks, chapterCount, selected, samples };
        });
        const frames = await page.evaluate((durationMs) => new Promise((resolveSample) => {
            const intervals = [];
            let previous = performance.now();
            const startedAt = previous;
            function sample(now) {
                intervals.push(now - previous);
                previous = now;
                if (now - startedAt >= durationMs) {
                    intervals.sort((left, right) => left - right);
                    const percentile = (fraction) => intervals[Math.min(
                        intervals.length - 1,
                        Math.floor(intervals.length * fraction),
                    )] ?? 0;
                    resolveSample({
                        durationMs: now - startedAt,
                        frames: intervals.length,
                        meanFrameMs: intervals.reduce((total, value) => total + value, 0) / intervals.length,
                        medianFrameMs: percentile(0.5),
                        p95FrameMs: percentile(0.95),
                        p99FrameMs: percentile(0.99),
                        maximumFrameMs: intervals.at(-1) ?? 0,
                        framesOver25Ms: intervals.filter((value) => value > 25).length,
                        framesOver50Ms: intervals.filter((value) => value > 50).length,
                    });
                    return;
                }
                requestAnimationFrame(sample);
            }
            requestAnimationFrame(sample);
        }), 10_000);
        const diagnostics = await page.evaluate(() => window.__immDiagnostics());
        const result = {
            file: basename(filePath),
            fileBytes,
            readyMs,
            settleMs,
            selectedChapterIndex: heavyPoint.selected.chapterIndex,
            chapterCount: heavyPoint.chapterCount,
            selectedTimeTicks: heavyPoint.selected.timeTicks,
            durationTicks: heavyPoint.durationTicks,
            scanDrawCalls: heavyPoint.selected.drawCalls,
            scanRenderedTriangles: heavyPoint.selected.renderedTriangles,
            ...frames,
            fps: 1_000 / frames.meanFrameMs,
            layers: diagnostics.layers,
            paintLayers: diagnostics.paintLayers,
            meshes: diagnostics.meshes,
            triangles: diagnostics.triangles,
            drawCalls: diagnostics.drawCalls,
            renderedTriangles: diagnostics.renderedTriangles,
            gpuFrameMs: diagnostics.gpuFrameMs,
            jsHeapBytes: diagnostics.jsHeapBytes,
            pixelRatio: diagnostics.pixelRatio,
            canvasWidth: diagnostics.canvasWidth,
            canvasHeight: diagnostics.canvasHeight,
            errors,
        };
        results.push(result);
        process.stdout.write(`${JSON.stringify(result)}\n`);
        await page.close();
    }
} finally {
    await browser.close();
    await server.close();
}

const report = {
    capturedAt: new Date().toISOString(),
    chromeHeadless: process.env.IMM_WEB_HEADLESS === "1",
    viewport: { width: 1280, height: 720, deviceScaleFactor: 1 },
    warmupMs: 5_000,
    sampleMs: 10_000,
    results,
};
if (outputPath !== null) await writeFile(outputPath, `${JSON.stringify(report, null, 2)}\n`);
