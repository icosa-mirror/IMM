import assert from "node:assert/strict";
import { mkdir, writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { chromium } from "playwright-core";
import { createServer } from "vite";

const appRoot = resolve(import.meta.dirname, "..");
const repositoryRoot = resolve(appRoot, "../../../..");
const artifactDirectory = resolve(repositoryRoot, "artifacts/web-native");
await mkdir(artifactDirectory, { recursive: true });

const server = await createServer({ root: appRoot, server: { host: "127.0.0.1", port: 4177 } });
await server.listen();
const browser = await chromium.launch({
    executablePath: "C:/Program Files/Google/Chrome/Application/chrome.exe",
    headless: process.env.IMM_WEB_HEADLESS === "1",
});

try {
    const desktop = await browser.newPage({ viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 });
    const errors = [];
    desktop.on("console", (message) => {
        if (message.type() === "error") errors.push(message.text());
    });
    await desktop.goto("http://127.0.0.1:4177/?src=/fixtures/sample1.imm&visual-test=1");
    await desktop.waitForFunction(() => window.__immDiagnostics?.().ready === true, undefined, { timeout: 30_000 });
    await desktop.waitForTimeout(1_000);
    const desktopMetrics = await desktop.evaluate(() => window.__immDiagnostics());
    assert.equal(desktopMetrics.strokes, 1_171);
    assert.equal(desktopMetrics.meshes, 42);
    assert.equal(desktopMetrics.triangles, 802_890);
    assert.equal(desktopMetrics.canvasWidth, 1280);
    assert.equal(desktopMetrics.canvasHeight, 720);
    assert.deepEqual(errors, []);
    await desktop.screenshot({ path: resolve(artifactDirectory, "sample1-web-1280x720.png") });

    const loadedGeometryCounts = [];
    for (let cycle = 0; cycle < 3; cycle++) {
        await desktop.evaluate(() => window.__immLoadUrl("/fixtures/sample1.imm"));
        await desktop.waitForTimeout(100);
        loadedGeometryCounts.push((await desktop.evaluate(() => window.__immDiagnostics())).gpuGeometries);
    }
    assert.ok(loadedGeometryCounts.every((count) => count === loadedGeometryCounts[0]),
        `WebGL geometry count grew across reloads: ${loadedGeometryCounts}`);

    const embedded = await browser.newPage({ viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 });
    await embedded.goto("http://127.0.0.1:4177/embed.html");
    await embedded.setInputFiles("#file-input", resolve(repositoryRoot, "exampleImmFiles/sample1.imm"));
    await embedded.waitForFunction(() => document.querySelector("#summary")?.textContent?.includes('"sharedRenderer": true'));
    const embedMetrics = JSON.parse(await embedded.locator("#summary").textContent());
    assert.equal(embedMetrics.immAttachedToHostScene, true);
    assert.equal(embedMetrics.sharedCanvas, true);
    assert.equal(embedMetrics.hostDepthTest, true);
    await embedded.screenshot({ path: resolve(artifactDirectory, "sample1-web-embedded.png") });

    const mobileContext = await browser.newContext({
        viewport: { width: 390, height: 844 },
        deviceScaleFactor: 3,
        isMobile: true,
        hasTouch: true,
    });
    const mobile = await mobileContext.newPage();
    const cdp = await mobileContext.newCDPSession(mobile);
    await cdp.send("Emulation.setCPUThrottlingRate", { rate: 4 });
    await mobile.goto("http://127.0.0.1:4177/?src=/fixtures/sample1.imm&visual-test=1");
    await mobile.waitForFunction(() => window.__immDiagnostics?.().ready === true, undefined, { timeout: 60_000 });
    await mobile.waitForTimeout(2_000);
    const mobileMetrics = await mobile.evaluate(() => window.__immDiagnostics());
    assert.equal(mobileMetrics.pixelRatio, 2);
    assert.ok(mobileMetrics.drawCalls > 0 && mobileMetrics.drawCalls <= 42);
    assert.ok(mobileMetrics.renderedTriangles > 0 && mobileMetrics.renderedTriangles <= 802_890);
    await mobile.screenshot({ path: resolve(artifactDirectory, "sample1-web-mobile.png") });
    await mobileContext.close();

    const report = { desktop: desktopMetrics, embedded: embedMetrics, mobile4xCpu: mobileMetrics, loadedGeometryCounts };
    await writeFile(resolve(artifactDirectory, "browser-report.json"), `${JSON.stringify(report, null, 2)}\n`);
    console.log(JSON.stringify(report, null, 2));
} finally {
    await browser.close();
    await server.close();
}
