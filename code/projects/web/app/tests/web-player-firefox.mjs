import assert from "node:assert/strict";
import { mkdir, writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { firefox } from "playwright-core";
import { createServer } from "vite";

const appRoot = resolve(import.meta.dirname, "..");
const repositoryRoot = resolve(appRoot, "../../../..");
const artifactDirectory = resolve(repositoryRoot, "artifacts/web-native");
const localCodecFixtures = [
    ["ogg-vorbis", process.env.IMM_WEB_LOCAL_OGG_FIXTURE],
    ["wav", process.env.IMM_WEB_LOCAL_WAV_FIXTURE],
].filter((entry) => entry[1] !== undefined);

await mkdir(artifactDirectory, { recursive: true });
const server = await createServer({
    root: appRoot,
    server: { host: "127.0.0.1", port: 4181, strictPort: true },
});
await server.listen();
const firefoxUserPrefs = {
    // Hosted Linux runners can block WebGL2 after a Firefox/runner-image update
    // even though Mesa software rendering is available under Xvfb.
    "webgl.disabled": false,
    "webgl.force-enabled": true,
};
if (process.env.IMM_WEB_REQUIRE_AUDIO_GESTURE === "1") {
    firefoxUserPrefs["media.autoplay.default"] = 5;
}
const browser = await firefox.launch({
    headless: process.env.IMM_WEB_HEADLESS === "1",
    firefoxUserPrefs,
});

try {
    const page = await browser.newPage({ viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 });
    const errors = [];
    capturePageErrors(page, errors);
    const webgl2Available = await page.evaluate(() => {
        const canvas = document.createElement("canvas");
        return canvas.getContext("webgl2") !== null;
    });
    assert.equal(webgl2Available, true,
        `Firefox did not expose WebGL2 before loading IMM: ${JSON.stringify(errors)}`);
    await page.goto("http://127.0.0.1:4181/?src=/fixtures/sample1.imm");
    try {
        await page.waitForFunction(() => window.__immDiagnostics?.().ready === true,
            undefined, { timeout: 30_000 });
    } catch (error) {
        const state = await page.evaluate(() => ({
            status: document.querySelector("#status")?.textContent ?? "",
            diagnostics: window.__immDiagnostics?.() ?? null,
        }));
        throw new Error(`Firefox IMM load failed: ${JSON.stringify({ state, errors })}`, { cause: error });
    }
    await page.waitForFunction(() => window.__immDiagnostics().audio.decodedSounds === 3,
        undefined, { timeout: 120_000 });

    let diagnostics = await page.evaluate(() => window.__immDiagnostics());
    assert.equal(diagnostics.strokes, 1_171);
    assert.equal(diagnostics.meshes, 42);
    assert.equal(diagnostics.triangles, 802_890);
    assert.ok(diagnostics.drawCalls > 0);
    assert.ok(diagnostics.renderedTriangles > 0);
    assert.ok(["sample-mask", "alpha-to-coverage", "alpha-hash"].includes(diagnostics.alphaMode),
        `Firefox selected unknown coverage mode ${diagnostics.alphaMode}`);
    assert.equal(diagnostics.audio.codecs.webAudio, true);
    assert.notEqual(diagnostics.audio.codecs.wav, "");
    assert.notEqual(diagnostics.audio.codecs.oggVorbis, "");
    assert.notEqual(diagnostics.audio.codecs.oggOpus, "");
    assert.equal(diagnostics.audio.decodedSounds, 3);
    assert.deepEqual(diagnostics.audio.decodeFailures, []);
    assert.equal(diagnostics.audio.ambisonicSupported, false);

    if (diagnostics.audio.contextState !== "running") await page.locator("#audio-toggle").click();
    let audioClockVerified = false;
    try {
        await page.waitForFunction(() => window.__immDiagnostics().audio.contextState === "running",
            undefined, { timeout: 5_000 });
        audioClockVerified = true;
    } catch (error) {
        if (process.env.IMM_WEB_ALLOW_SUSPENDED_AUDIO !== "1") throw error;
    }
    if (audioClockVerified) {
        const driftDeadline = Date.now() + 5_000;
        while (Date.now() < driftDeadline) {
            const playback = await page.evaluate(() => window.__immPlayback.snapshot());
            if (playback.waiting) await page.locator("#continue").click();
            const driftSamples = await page.evaluate(() => window.__immDiagnostics().audio.driftSampleCount);
            if (driftSamples >= 3) break;
            await page.waitForTimeout(100);
        }
        diagnostics = await page.evaluate(() => window.__immDiagnostics());
        assert.equal(diagnostics.audio.timelineClock, "audio-context");
        assert.ok(diagnostics.audio.driftSampleCount >= 3,
            `Firefox produced only ${diagnostics.audio.driftSampleCount} audio drift samples`);
        assert.ok(diagnostics.audio.maximumAbsoluteDriftSeconds <= 0.05,
            `Firefox A/V drift exceeded 50 ms: ${JSON.stringify(diagnostics.audio)}`);
    }

    const generatedWav = await page.evaluate(async () => {
        const sampleRate = 8_000;
        const sampleCount = 800;
        const bytes = new Uint8Array(44 + sampleCount * 2);
        const view = new DataView(bytes.buffer);
        const text = (offset, value) => [...value].forEach((character, index) =>
            view.setUint8(offset + index, character.charCodeAt(0)));
        text(0, "RIFF");
        view.setUint32(4, bytes.length - 8, true);
        text(8, "WAVE");
        text(12, "fmt ");
        view.setUint32(16, 16, true);
        view.setUint16(20, 1, true);
        view.setUint16(22, 1, true);
        view.setUint32(24, sampleRate, true);
        view.setUint32(28, sampleRate * 2, true);
        view.setUint16(32, 2, true);
        view.setUint16(34, 16, true);
        text(36, "data");
        view.setUint32(40, sampleCount * 2, true);
        for (let index = 0; index < sampleCount; index++) {
            view.setInt16(44 + index * 2, Math.round(Math.sin(index * Math.PI / 10) * 8_000), true);
        }
        const context = new AudioContext();
        try {
            const decoded = await context.decodeAudioData(bytes.buffer);
            return { channels: decoded.numberOfChannels, sampleRate: decoded.sampleRate, duration: decoded.duration };
        } finally {
            await context.close();
        }
    });
    assert.equal(generatedWav.channels, 1);
    assert.ok(generatedWav.sampleRate > 0);
    assert.ok(Math.abs(generatedWav.duration - 0.1) < 0.001,
        `Firefox decoded the generated WAV at an unexpected duration: ${JSON.stringify(generatedWav)}`);

    const localCodecResults = [];
    for (const [codec, fixturePath] of localCodecFixtures) {
        await page.setInputFiles("#file-input", fixturePath);
        await page.waitForFunction(() => window.__immDiagnostics?.().ready === true,
            undefined, { timeout: 120_000 });
        await page.waitForFunction(() => {
            const audio = window.__immDiagnostics().audio;
            return audio.soundLayers > 0 && audio.decodedSounds + audio.decodeFailures.length >= audio.soundLayers;
        }, undefined, { timeout: 120_000 });
        const audio = await page.evaluate(() => window.__immDiagnostics().audio);
        assert.equal(audio.decodeFailures.length, 0,
            `Firefox failed ${codec} IMM decode: ${JSON.stringify(audio.decodeFailures)}`);
        assert.equal(audio.decodedSounds, audio.soundLayers);
        localCodecResults.push({ codec, soundLayers: audio.soundLayers, decodedSounds: audio.decodedSounds });
    }

    const phase3 = await browser.newPage({ viewport: { width: 640, height: 360 }, deviceScaleFactor: 1 });
    capturePageErrors(phase3, errors);
    await phase3.goto("http://127.0.0.1:4181/phase3-fixture.html");
    await phase3.waitForFunction(() => window.__phase3Fixture?.state().ready === true);
    const coverageState = await phase3.evaluate(() => window.__phase3Fixture.coverageState());
    assert.ok(coverageState.paint.length > 0);
    assert.ok(coverageState.paint.every((state) => state.noBlending && !state.transparent &&
        state.depthTest && state.depthWrite));
    assert.ok(coverageState.models.every((state) => state.noBlending && !state.transparent &&
        state.depthTest && state.depthWrite && state.doubleSided));
    const overlapSamples = await phase3.evaluate(() => ({
        forward: window.__phase3Fixture.sampleOverlap(false),
        reverse: window.__phase3Fixture.sampleOverlap(true),
    }));
    const overlapDifference = pixelArrayDifference(overlapSamples.forward, overlapSamples.reverse);
    assert.equal(overlapDifference.changedChannels, 0,
        `Firefox coverage changed with paint submission order: ${JSON.stringify(overlapDifference)}`);
    const modelOverlapSamples = await phase3.evaluate(() => ({
        forward: window.__phase3Fixture.sampleModelOverlap(false),
        reverse: window.__phase3Fixture.sampleModelOverlap(true),
    }));
    const modelOverlapDifference = pixelArrayDifference(modelOverlapSamples.forward, modelOverlapSamples.reverse);
    assert.equal(modelOverlapDifference.changedChannels, 0,
        `Firefox coverage changed with model submission order: ${JSON.stringify(modelOverlapDifference)}`);
    const opaqueIntersection = await phase3.evaluate(() => ({
        forward: window.__phase3Fixture.sampleOpaqueIntersection(false),
        reverse: window.__phase3Fixture.sampleOpaqueIntersection(true),
    }));
    assert.deepEqual(opaqueIntersection.forward, opaqueIntersection.reverse);
    assert.ok(pixelDistance(opaqueIntersection.forward.left, opaqueIntersection.forward.right) > 40);
    const flippedPaint = await phase3.evaluate(() => window.__phase3Fixture.sampleFlippedPaint());
    assert.ok(pixelDistance(flippedPaint.normal, [102, 187, 106, 255]) <= 12);
    assert.ok(pixelDistance(flippedPaint.flipped, flippedPaint.normal) <= 12);
    await phase3.close();

    const alphaHash = await browser.newPage({ viewport: { width: 640, height: 360 }, deviceScaleFactor: 1 });
    capturePageErrors(alphaHash, errors);
    await alphaHash.goto("http://127.0.0.1:4181/phase3-fixture.html?antialias=0");
    await alphaHash.waitForFunction(() => window.__phase3Fixture?.state().ready === true);
    const alphaHashState = await alphaHash.evaluate(() => window.__phase3Fixture.state());
    assert.equal(alphaHashState.alphaMode, "alpha-hash");
    assert.equal(alphaHashState.sampleCount, 0);
    const alphaHashSamples = await alphaHash.evaluate(() => ({
        forward: window.__phase3Fixture.sampleOverlap(false),
        reverse: window.__phase3Fixture.sampleOverlap(true),
    }));
    const alphaHashDifference = pixelArrayDifference(alphaHashSamples.forward, alphaHashSamples.reverse);
    assert.equal(alphaHashDifference.changedChannels, 0,
        `Firefox alpha-hash changed with submission order: ${JSON.stringify(alphaHashDifference)}`);
    await alphaHash.close();

    await page.screenshot({ path: resolve(artifactDirectory, "sample1-web-firefox-1280x720.png") });
    const report = {
        browser: await browser.version(),
        coverageMode: diagnostics.alphaMode,
        strokes: diagnostics.strokes,
        meshes: diagnostics.meshes,
        triangles: diagnostics.triangles,
        audio: diagnostics.audio,
        generatedWav,
        audioClockVerified,
        localCodecResults,
        coverage: {
            state: coverageState,
            overlapDifference,
            modelOverlapDifference,
            opaqueIntersection,
            flippedPaint,
            alphaHash: { state: alphaHashState, overlapDifference: alphaHashDifference },
        },
        errors,
    };
    assert.deepEqual(errors, []);
    await writeFile(resolve(artifactDirectory, "browser-report-firefox.json"), `${JSON.stringify(report, null, 2)}\n`);
    console.log(JSON.stringify(report, null, 2));
} finally {
    await browser.close();
    await server.close();
}

function capturePageErrors(page, errors) {
    page.on("console", (message) => {
        if (message.type() === "error") errors.push(message.text());
    });
    page.on("pageerror", (error) => errors.push(error.message));
}

function pixelDistance(a, b) {
    return Math.abs(a[0] - b[0]) + Math.abs(a[1] - b[1]) + Math.abs(a[2] - b[2]);
}

function pixelArrayDifference(a, b) {
    assert.equal(a.length, b.length);
    let changedChannels = 0;
    let maximumDifference = 0;
    let absoluteDifference = 0;
    for (let index = 0; index < a.length; index++) {
        const difference = Math.abs(a[index] - b[index]);
        if (difference > 0) changedChannels++;
        maximumDifference = Math.max(maximumDifference, difference);
        absoluteDifference += difference;
    }
    return { changedChannels, maximumDifference, absoluteDifference };
}
