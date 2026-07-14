import assert from "node:assert/strict";
import { resolve } from "node:path";
import { chromium } from "playwright-core";
import { createServer } from "vite";

const appRoot = resolve(import.meta.dirname, "..");
const repositoryRoot = resolve(appRoot, "../../../..");
const server = await createServer({ root: appRoot, server: { host: "127.0.0.1", port: 4178 } });
await server.listen();
const browser = await chromium.launch({
    channel: "chrome",
    headless: process.env.IMM_WEB_HEADLESS === "1",
});

try {
    const standalone = await browser.newPage({
        viewport: { width: 960, height: 540 },
        deviceScaleFactor: 2,
    });
    const errors = [];
    standalone.on("console", (message) => {
        if (message.type() === "error") errors.push(message.text());
    });
    await standalone.goto("http://127.0.0.1:4178/?src=/fixtures/sample1.imm&visual-test=1");
    await standalone.waitForFunction(
        () => window.__immDiagnostics?.().ready === true,
        undefined,
        { timeout: 30_000 },
    );
    const diagnostics = await standalone.evaluate(() => window.__immDiagnostics());
    assert.equal(diagnostics.strokes, 1_171);
    assert.equal(diagnostics.meshes, 42);
    assert.ok(diagnostics.renderedTriangles > 0);
    assert.equal(diagnostics.renderQuality, "normal");
    assert.equal(diagnostics.pixelRatio, 1);
    assert.equal(diagnostics.canvasWidth, 960);
    assert.equal(diagnostics.canvasHeight, 540);
    assert.deepEqual(errors, []);

    await standalone.evaluate(() => {
        const quality = document.querySelector("#render-quality");
        quality.value = "high";
        quality.dispatchEvent(new Event("change"));
    });
    await standalone.waitForFunction(() => window.__immDiagnostics().pixelRatio === 2);
    let qualityDiagnostics = await standalone.evaluate(() => window.__immDiagnostics());
    assert.equal(qualityDiagnostics.renderQuality, "high");
    assert.equal(qualityDiagnostics.canvasWidth, 1_920);
    assert.equal(qualityDiagnostics.canvasHeight, 1_080);

    await standalone.reload();
    await standalone.waitForFunction(
        () => window.__immDiagnostics?.().ready === true,
        undefined,
        { timeout: 30_000 },
    );
    qualityDiagnostics = await standalone.evaluate(() => window.__immDiagnostics());
    assert.equal(qualityDiagnostics.renderQuality, "high");
    assert.equal(qualityDiagnostics.pixelRatio, 2);
    assert.equal(await standalone.locator("#render-quality").inputValue(), "high");
    await standalone.evaluate(() => window.__immPlayback.play());
    await standalone.waitForTimeout(100);
    assert.ok((await standalone.evaluate(() => window.__immPlayback.snapshot())).timeTicks > 0);
    await standalone.close();

    const embedded = await browser.newPage({ viewport: { width: 960, height: 540 } });
    await embedded.goto("http://127.0.0.1:4178/embed.html");
    await embedded.setInputFiles("#file-input", resolve(repositoryRoot, "exampleImmFiles/sample1.imm"));
    await embedded.waitForFunction(
        () => document.querySelector("#summary")?.textContent?.includes('"sharedRenderer": true'),
        undefined,
        { timeout: 30_000 },
    );
    const embeddedMetrics = JSON.parse(await embedded.locator("#summary").textContent());
    assert.equal(embeddedMetrics.immAttachedToHostScene, true);
    assert.equal(embeddedMetrics.sharedCanvas, true);
    assert.equal(embeddedMetrics.hostDepthTest, true);
    await embedded.close();
} finally {
    await browser.close();
    await server.close();
}
