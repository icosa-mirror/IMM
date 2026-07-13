import assert from "node:assert/strict";
import { mkdir, writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { execFileSync } from "node:child_process";
import { chromium } from "playwright-core";
import { createServer } from "vite";

const appRoot = resolve(import.meta.dirname, "..");
const repositoryRoot = resolve(appRoot, "../../../..");
const artifactDirectory = resolve(repositoryRoot, "artifacts/web-native");
await mkdir(artifactDirectory, { recursive: true });

const server = await createServer({ root: appRoot, server: { host: "127.0.0.1", port: 4177 } });
await server.listen();
const browser = await chromium.launch({
    channel: "chrome",
    headless: process.env.IMM_WEB_HEADLESS === "1",
});

try {
    const controlsPage = await browser.newPage({ viewport: { width: 900, height: 700 }, deviceScaleFactor: 1 });
    const clipboardImmUrl = "https://example.com/scenes/clipboard-test.imm";
    await controlsPage.addInitScript((clipboardText) => {
        Object.defineProperty(navigator, "clipboard", {
            configurable: true,
            value: { readText: async () => clipboardText },
        });
    }, clipboardImmUrl);
    await controlsPage.route("**/fixtures/sample1.imm", async (route) => {
        await new Promise((resolveDelay) => setTimeout(resolveDelay, 300));
        await route.continue();
    });
    await controlsPage.route("**/fixtures/not-an-imm.imm", (route) => route.fulfill({
        status: 200,
        contentType: "application/octet-stream",
        body: "not an IMM document",
    }));
    await controlsPage.goto("http://127.0.0.1:4177/");
    await controlsPage.waitForFunction(() => document.querySelector("#status")?.textContent?.startsWith("Fetching"));
    assert.equal(await controlsPage.locator("#url-input").isDisabled(), false,
        "URL input was disabled while the default IMM loaded");
    assert.equal(await controlsPage.locator("#file-input").isDisabled(), false,
        "File input was disabled while the default IMM loaded");
    await controlsPage.locator("#paste-url").click();
    assert.equal(await controlsPage.locator("#url-input").inputValue(), clipboardImmUrl);
    await controlsPage.waitForFunction(() => window.__immDiagnostics?.().ready === true, undefined, { timeout: 30_000 });
    assert.equal((await controlsPage.locator("#status").textContent())?.includes("sample1.imm"), true,
        "Base URL did not load the bundled sample IMM by default");
    assert.equal(await controlsPage.locator("#camera-mode").inputValue(), "fly",
        "Fly/free-look was not the default camera mode");
    assert.ok(await controlsPage.locator("#viewpoint option").count() > 0,
        "Decoded spawn areas were not exposed as viewpoints");
    const initialNavigation = await controlsPage.evaluate(() => window.__immDiagnostics());
    await controlsPage.mouse.move(700, 350);
    await controlsPage.mouse.down();
    await controlsPage.mouse.move(760, 390, { steps: 4 });
    await controlsPage.mouse.up();
    const lookedNavigation = await controlsPage.evaluate(() => window.__immDiagnostics());
    assert.notDeepEqual(lookedNavigation.cameraQuaternion, initialNavigation.cameraQuaternion,
        "Pointer drag did not rotate the fly camera");
    await controlsPage.keyboard.down("KeyW");
    await controlsPage.waitForTimeout(100);
    await controlsPage.keyboard.up("KeyW");
    const movedNavigation = await controlsPage.evaluate(() => window.__immDiagnostics());
    assert.notDeepEqual(movedNavigation.cameraPosition, lookedNavigation.cameraPosition,
        "WASD input did not move the fly camera");
    await controlsPage.locator("#camera-mode").selectOption("orbit");
    assert.equal((await controlsPage.evaluate(() => window.__immDiagnostics())).cameraMode, "orbit",
        "Orbit mode could not be selected");
    await controlsPage.locator("#camera-mode").selectOption("fly");
    await controlsPage.locator("#viewpoint").selectOption(initialNavigation.viewpoint);
    await controlsPage.evaluate(() => window.__immLoadUrl("/fixtures/not-an-imm.imm").catch(() => undefined));
    const failedLoadState = await controlsPage.evaluate(() => ({
        diagnostics: window.__immDiagnostics(),
        playback: window.__immPlayback.snapshot(),
    }));
    assert.equal(failedLoadState.diagnostics.ready, false,
        "Failed load retained the previous IMM view");
    assert.equal(failedLoadState.diagnostics.gridVisible, true,
        "Failed load did not restore the idle grid");
    assertVectorNear(failedLoadState.diagnostics.cameraPosition, [3, 2, 5],
        "Failed load retained the previous IMM camera position");
    assertVectorNear(failedLoadState.diagnostics.controlsTarget, [0, 0.75, 0],
        "Failed load retained the previous OrbitControls target");
    assert.equal(failedLoadState.playback.durationTicks, 0,
        "Failed load retained the previous playback document");
    await controlsPage.evaluate(() => window.__immLoadUrl("/fixtures/sample1.imm"));
    await controlsPage.waitForFunction(() => window.__immDiagnostics?.().ready === true, undefined, { timeout: 30_000 });
    assert.equal((await controlsPage.evaluate(() => window.__immDiagnostics())).strokes, 1_171,
        "Valid IMM did not recover after a failed load");
    await controlsPage.close();

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
    execFileSync("python", [
        resolve(repositoryRoot, "tests/tools/compare_render_metrics.py"),
        resolve(artifactDirectory, "sample1-web-1280x720.png"),
        "--reference", resolve(repositoryRoot, "tests/baselines/render/windows-directx-sample1.ppm"),
        "--contract", resolve(repositoryRoot, "tests/baselines/render/web-three-sample1.json"),
        "--json-output", resolve(artifactDirectory, "sample1-web-render-metrics.json"),
    ], { stdio: "inherit" });

    await desktop.evaluate(() => window.__immPlayback.play());
    await desktop.waitForTimeout(100);
    const playedState = await desktop.evaluate(() => window.__immPlayback.snapshot());
    assert.ok(playedState.timeTicks > 0, "Standalone document clock did not advance while playing");
    const playbackInfo = await desktop.evaluate(() => {
        window.__immPlayback.pause();
        window.__immPlayback.seekTicks(0);
        return window.__immPlayback.snapshot();
    });
    assert.ok(playbackInfo.durationTicks > 0);
    const sampleTimestamps = [...new Set([
        0,
        Math.round(playbackInfo.durationTicks / 2),
        playbackInfo.durationTicks,
    ])];
    for (const ticks of sampleTimestamps) {
        await desktop.evaluate((value) => window.__immPlayback.seekTicks(value), ticks);
        await desktop.waitForTimeout(50);
        await desktop.screenshot({ path: resolve(artifactDirectory, `sample1-time-${ticks}.png`) });
    }

    const loadedGeometryCounts = [];
    let decodeResponsiveness = null;
    for (let cycle = 0; cycle < 3; cycle++) {
        const responsiveness = await desktop.evaluate(async () => {
            let eventLoopTicks = 0;
            const interval = window.setInterval(() => eventLoopTicks++, 0);
            const startedAt = performance.now();
            await window.__immLoadUrl("/fixtures/sample1.imm");
            window.clearInterval(interval);
            return { eventLoopTicks, loadMs: performance.now() - startedAt };
        });
        if (cycle === 0) decodeResponsiveness = responsiveness;
        await desktop.waitForTimeout(100);
        loadedGeometryCounts.push((await desktop.evaluate(() => window.__immDiagnostics())).gpuGeometries);
    }
    assert.ok(decodeResponsiveness.eventLoopTicks > 0,
        `Main thread did not service events during ${decodeResponsiveness.loadMs} ms reload`);
    assert.ok(loadedGeometryCounts.every((count) => count === loadedGeometryCounts[0]),
        `WebGL geometry count grew across reloads: ${loadedGeometryCounts}`);

    const phase3 = await browser.newPage({ viewport: { width: 640, height: 360 }, deviceScaleFactor: 1 });
    const phase3Errors = [];
    phase3.on("console", (message) => {
        if (message.type() === "error") phase3Errors.push(message.text());
    });
    await phase3.goto("http://127.0.0.1:4177/phase3-fixture.html");
    await phase3.waitForFunction(() => window.__phase3Fixture?.state().ready === true);
    const expectedStates = [
        { ticks: 0, x: -1, opacity: 0.25, drawIn: 0, drawing: 0 },
        { ticks: 25, x: -0.5, opacity: 0.4375, drawIn: 0.125, drawing: 0 },
        { ticks: 50, x: 0, opacity: 0.625, drawIn: 0.25, drawing: 1 },
        { ticks: 75, x: 0.5, opacity: 0.8125, drawIn: 0.375, drawing: 1 },
        { ticks: 100, x: -1, opacity: 0.25, drawIn: 0, drawing: 0 },
        { ticks: 150, x: 0, opacity: 0.625, drawIn: 0.25, drawing: 1 },
        { ticks: 200, x: -1, opacity: 0.25, drawIn: 0, drawing: 0 },
        { ticks: 399, x: 0.98, opacity: 0.9925, drawIn: 0.495, drawing: 1 },
        { ticks: 400, x: -1, opacity: 0.25, drawIn: 0, drawing: 0 },
    ];
    const phase3States = [];
    for (const expected of expectedStates) {
        const actual = await phase3.evaluate((ticks) => {
            window.__phase3Fixture.setTimeTicks(ticks);
            return window.__phase3Fixture.state();
        }, expected.ticks);
        phase3States.push(actual);
        assert.equal(actual.timeTicks, expected.ticks);
        assert.equal(actual.paintVisible, true);
        assert.ok(Math.abs(actual.paintX - expected.x) < 1e-5, `Transform mismatch at ${expected.ticks}`);
        assert.ok(Math.abs(actual.opacity - expected.opacity) < 1e-5, `Opacity mismatch at ${expected.ticks}`);
        assert.ok(Math.abs(actual.drawIn - expected.drawIn) < 1e-5, `Draw-in mismatch at ${expected.ticks}`);
        assert.equal(actual.drawingIndex, expected.drawing);
        assert.deepEqual(actual.pictureTypes, [0, 1, 2, 3, 4]);
        assert.deepEqual(actual.keepAliveTypes, [1, 2]);
        await phase3.screenshot({ path: resolve(artifactDirectory, `phase3-time-${expected.ticks}.png`) });
    }
    await phase3.evaluate(() => window.__phase3Fixture.setTimeTicks(50));
    const targetBefore = await phase3.screenshot();
    await phase3.evaluate(() => window.__phase3Fixture.setTimeTicks(17));
    await phase3.evaluate(() => window.__phase3Fixture.setTimeTicks(311));
    await phase3.evaluate(() => window.__phase3Fixture.setTimeTicks(400));
    await phase3.evaluate(() => window.__phase3Fixture.setTimeTicks(50));
    const targetAfterPriorPlayback = await phase3.screenshot();
    assert.deepEqual(targetAfterPriorPlayback, targetBefore,
        "Explicit seek rendered differently after prior playback");
    assert.deepEqual(phase3Errors, []);
    const phase3GeometryCounts = phase3States.map((state) => state.gpuGeometries);
    assert.ok(phase3GeometryCounts.every((count) => count === phase3GeometryCounts[0]),
        `Drawing swaps leaked GPU geometries: ${phase3GeometryCounts}`);
    const lockedPositions = await phase3.evaluate(() => {
        window.__phase3Fixture.moveCameraX(0);
        const initial = window.__phase3Fixture.state().lockedPictureX;
        window.__phase3Fixture.moveCameraX(5);
        const moved = window.__phase3Fixture.state().lockedPictureX;
        return { initial, moved };
    });
    assert.ok(Math.abs(lockedPositions.moved - lockedPositions.initial - 5) < 1e-5,
        `Viewer-locked picture did not follow the camera: ${JSON.stringify(lockedPositions)}`);
    const pictureSamples = await phase3.evaluate(() => {
        const directions = [[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]];
        return {
            stereo: [
                window.__phase3Fixture.samplePicture(2, [0, 0, -1], 0),
                window.__phase3Fixture.samplePicture(2, [0, 0, -1], 1),
            ],
            cross: directions.map((direction) => window.__phase3Fixture.samplePicture(3, direction)),
            vertical: directions.map((direction) => window.__phase3Fixture.samplePicture(4, direction)),
        };
    });
    const faceColors = [[255, 0, 0], [0, 255, 0], [0, 0, 255], [255, 255, 0], [255, 0, 255], [0, 255, 255]];
    assertPixelNear(pictureSamples.stereo[0], [255, 0, 0], "stereo left eye");
    assertPixelNear(pictureSamples.stereo[1], [0, 0, 255], "stereo right eye");
    pictureSamples.cross.forEach((pixel, index) => assertPixelNear(pixel, faceColors[index], `cross face ${index}`));
    pictureSamples.vertical.forEach((pixel, index) => assertPixelNear(pixel, faceColors[index], `vertical face ${index}`));
    const paintEffectSamples = await phase3.evaluate(() => ({
        drawHidden: window.__phase3Fixture.samplePaintEffect(0, 0, 0),
        drawVisible: window.__phase3Fixture.samplePaintEffect(1, 0, 0),
        blinkLow: window.__phase3Fixture.samplePaintEffect(1, 0, 2),
        blinkHigh: window.__phase3Fixture.samplePaintEffect(1, 0.5, 2),
    }));
    assert.ok(pixelDistance(paintEffectSamples.drawHidden, paintEffectSamples.drawVisible) > 100,
        `Draw-in did not change rendered pixels: ${JSON.stringify(paintEffectSamples)}`);
    assert.ok(pixelDistance(paintEffectSamples.blinkLow, paintEffectSamples.blinkHigh) > 50,
        `Blink did not modulate rendered pixels: ${JSON.stringify(paintEffectSamples)}`);
    assert.ok(pixelDistance(paintEffectSamples.drawVisible, paintEffectSamples.blinkHigh) <= 8,
        "Blink maximum did not restore authored paint output");
    await phase3.close();

    const embedded = await browser.newPage({ viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 });
    await embedded.goto("http://127.0.0.1:4177/embed.html");
    await embedded.setInputFiles("#file-input", resolve(repositoryRoot, "exampleImmFiles/sample1.imm"));
    await embedded.waitForFunction(() => document.querySelector("#summary")?.textContent?.includes('"sharedRenderer": true'));
    await embedded.waitForTimeout(1_000);
    const embedMetrics = JSON.parse(await embedded.locator("#summary").textContent());
    assert.equal(embedMetrics.immAttachedToHostScene, true);
    assert.equal(embedMetrics.sharedCanvas, true);
    assert.equal(embedMetrics.hostDepthTest, true);
    assert.equal(embedMetrics.hostCubeAtPaintVertex, true);
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

    const report = {
        desktop: desktopMetrics,
        embedded: embedMetrics,
        mobile4xCpu: mobileMetrics,
        loadedGeometryCounts,
        decodeResponsiveness,
        playbackInfo,
        playedState,
        sampleTimestamps,
        phase3States,
        lockedPositions,
        pictureSamples,
        paintEffectSamples,
    };
    await writeFile(resolve(artifactDirectory, "browser-report.json"), `${JSON.stringify(report, null, 2)}\n`);
    console.log(JSON.stringify(report, null, 2));
} finally {
    await browser.close();
    await server.close();
}

function assertPixelNear(actual, expected, label) {
    assert.ok(actual !== undefined && expected !== undefined && actual.length >= 3);
    const difference = Math.max(...expected.map((value, index) => Math.abs(value - actual[index])));
    assert.ok(difference <= 8, `${label} sampled ${actual}, expected ${expected}`);
}

function pixelDistance(a, b) {
    return Math.abs(a[0] - b[0]) + Math.abs(a[1] - b[1]) + Math.abs(a[2] - b[2]);
}

function assertVectorNear(actual, expected, message) {
    assert.equal(actual.length, expected.length, message);
    const difference = Math.max(...expected.map((value, index) => Math.abs(value - actual[index])));
    assert.ok(difference <= 1e-9, `${message}: ${actual}, expected ${expected}`);
}
