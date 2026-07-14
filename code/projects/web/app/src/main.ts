import * as THREE from "three";
import { VRButton } from "three/addons/webxr/VRButton.js";
import { cameraAudioTransform, ImmWebAudio } from "./audio/imm-web-audio";
import { desktopSpawnTransform, ImmCameraControls, type CameraMode } from "./camera-controls";
import { ImmDecoderClient, type ImmStagedDelta } from "./decoder-client";
import { createNativeLoadOrder, type StagedLoadWork } from "./staged-loading";
import { releaseAssetUrl } from "./release-assets";
import { FileRandomAccessSource } from "./random-access-source";
import type { ImmDocument } from "./format/imm-document";
import { ImmThreeView } from "./render-three/imm-three-view";
import {
    ImmPlaybackController,
    resolveActiveSpawnArea,
    type ImmActiveSpawnArea,
    type ImmPlaybackSnapshot,
} from "./runtime/imm-playback";
import "./style.css";


const canvas = requiredElement<HTMLCanvasElement>("viewport");
const fileInput = requiredElement<HTMLInputElement>("file-input");
const urlForm = requiredElement<HTMLFormElement>("url-form");
const urlInput = requiredElement<HTMLInputElement>("url-input");
const pasteUrl = requiredElement<HTMLButtonElement>("paste-url");
const status = requiredElement<HTMLParagraphElement>("status");
const summary = requiredElement<HTMLPreElement>("summary");
const playbackControls = requiredElement<HTMLElement>("playback-controls");
const playPause = requiredElement<HTMLButtonElement>("play-pause");
const continueButton = requiredElement<HTMLButtonElement>("continue");
const restartButton = requiredElement<HTMLButtonElement>("restart");
const skipBack = requiredElement<HTMLButtonElement>("skip-back");
const skipForward = requiredElement<HTMLButtonElement>("skip-forward");
const audioToggle = requiredElement<HTMLButtonElement>("audio-toggle");
const timeline = requiredElement<HTMLInputElement>("timeline");
const playbackTime = requiredElement<HTMLOutputElement>("playback-time");
const chapter = requiredElement<HTMLSelectElement>("chapter");
const viewpoint = requiredElement<HTMLSelectElement>("viewpoint");
const cameraMode = requiredElement<HTMLSelectElement>("camera-mode");
const renderQuality = requiredElement<HTMLSelectElement>("render-quality");
const benchmarkEagerDecode = new URLSearchParams(location.search).get("benchmark-eager") === "1";
const renderQualityStorageKey = "imm-render-quality";
type RenderQuality = "normal" | "high";
let selectedRenderQuality = loadRenderQuality();
renderQuality.value = selectedRenderQuality;

const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, stencil: true });
renderer.setPixelRatio(renderPixelRatio(selectedRenderQuality));
const idleClearColor = new THREE.Color(0x10151d);
renderer.setClearColor(idleClearColor, 1);
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.NoToneMapping;
renderer.xr.enabled = true;
let appliedXrFramebufferScale = renderXrFramebufferScale(selectedRenderQuality);
renderer.xr.setFramebufferScaleFactor(appliedXrFramebufferScale);
renderer.xr.addEventListener("sessionend", applyXrFramebufferScale);

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(70, 1, 0.01, 20_000);
const idleCameraPosition = new THREE.Vector3(3, 2, 5);
const idleControlsTarget = new THREE.Vector3(0, 0.75, 0);
camera.position.copy(idleCameraPosition);
scene.add(camera);

const controls = new ImmCameraControls(camera, renderer.domElement);
controls.reset(idleCameraPosition, idleControlsTarget);
const xrRig = new THREE.Group();
scene.add(xrRig);
if ("xr" in navigator) document.body.append(VRButton.createButton(renderer));

const grid = new THREE.GridHelper(10, 20, 0x506070, 0x28333f);
scene.add(grid);

const decoder = new ImmDecoderClient();
let immView: ImmThreeView | null = null;
let playback: ImmPlaybackController | null = null;
let immAudio: ImmWebAudio | null = null;
let lastMetrics: Record<string, unknown> | null = null;
let frameStart = performance.now();
let frameCount = 0;
let meanFrameMs = 0;
let firstUploadRenderMs: number | null = null;
let measureNextRender = false;
const timerContext = renderer.getContext() as WebGL2RenderingContext;
const timerExtension = timerContext.getExtension("EXT_disjoint_timer_query_webgl2") as
    | { TIME_ELAPSED_EXT: number; GPU_DISJOINT_EXT: number }
    | null;
let timerQuery: WebGLQuery | null = null;
let timerQueryActive = false;
let gpuFrameMs: number | null = null;
let previousAnimationTime = performance.now();
let loadRequestId = 0;
let loadAbortController = new AbortController();
let appliedAuthoredSpawn = "";

declare global {
    interface Window {
        __immLoadUrl: (url: string) => Promise<void>;
        __immDisposeView: () => void;
        __immDiagnostics: () => Record<string, unknown>;
        __immPlayback: {
            play(): void;
            pause(): void;
            seekTicks(value: number): void;
            selectChapter(index: number): void;
        snapshot(): Record<string, unknown>;
        };
    }
}

fileInput.addEventListener("change", async () => {
    const file = fileInput.files?.[0];
    if (file === undefined) {
        return;
    }

    const requestId = beginLoad(`Reading ${file.name}…`);
    fileInput.value = "";
    try {
        const fileSource = new FileRandomAccessSource(file);
        const source = await fileSource.read(0n, file.size, loadAbortController.signal);
        await loadDocument(file.name, source, requestId);
    } catch (error) {
        if (requestId === loadRequestId) {
            status.textContent = error instanceof Error ? error.message : String(error);
        }
    }
});

urlForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    if (urlInput.value === "") return;
    await loadUrl(urlInput.value).catch(() => undefined);
});

pasteUrl.addEventListener("click", async () => {
    try {
        if (navigator.clipboard?.readText === undefined) {
            throw new Error("Clipboard access is not available in this browser.");
        }
        const clipboardText = (await navigator.clipboard.readText()).trim();
        const clipboardUrl = new URL(clipboardText);
        if (clipboardUrl.protocol !== "http:" && clipboardUrl.protocol !== "https:") {
            throw new Error("Clipboard text is not an HTTP or HTTPS URL.");
        }
        urlInput.value = clipboardText;
        urlInput.setCustomValidity("");
        urlInput.focus();
        status.textContent = "IMM URL pasted. Select Load to open it.";
    } catch (error) {
        status.textContent = error instanceof Error ? error.message : String(error);
        urlInput.focus();
    }
});

window.__immLoadUrl = loadUrl;
window.__immDisposeView = resetDocumentState;
window.__immDiagnostics = () => {
    const activeViewpoint = playback?.evaluate().layers.get(Number(viewpoint.value));
    return {
    ready: immView !== null,
    ...lastMetrics,
    frameMs: round(meanFrameMs),
    fps: meanFrameMs > 0 ? round(1_000 / meanFrameMs) : 0,
    pixelRatio: renderer.getPixelRatio(),
    renderQuality: selectedRenderQuality,
    canvasWidth: canvas.width,
    canvasHeight: canvas.height,
    drawCalls: renderer.info.render.calls,
    renderedTriangles: renderer.info.render.triangles,
    gpuGeometries: renderer.info.memory.geometries,
    gpuTextures: renderer.info.memory.textures,
    jsHeapBytes: "memory" in performance
        ? (performance as Performance & { memory: { usedJSHeapSize: number } }).memory.usedJSHeapSize
        : null,
    firstUploadRenderMs: firstUploadRenderMs === null ? null : round(firstUploadRenderMs),
    gpuFrameMs: gpuFrameMs === null ? null : round(gpuFrameMs),
    gpuTimerAvailable: timerExtension !== null,
    cameraPosition: camera.position.toArray(),
    cameraQuaternion: camera.quaternion.toArray(),
    cameraScale: camera.scale.toArray(),
    controlsTarget: controls.orbit.target.toArray(),
    cameraMode: controls.mode,
    viewpoint: viewpoint.value,
    viewpointTracking: activeViewpoint?.layer.spawnTracking ?? null,
    viewpointAuthoredTranslation: activeViewpoint?.worldTransform.translation ?? null,
    xrPresenting: renderer.xr.isPresenting,
    xrFramebufferScale: appliedXrFramebufferScale,
    xrFramebufferScalePending: appliedXrFramebufferScale !== renderXrFramebufferScale(selectedRenderQuality),
    gridVisible: grid.visible,
    audio: immAudio?.diagnostics ?? null,
    };
};
window.__immPlayback = {
    play: () => {
        playback?.play();
        syncAudio(false);
    },
    pause: () => {
        playback?.pause();
        syncAudio(false);
    },
    seekTicks: (value) => {
        playback?.seekTicks(value);
        applyAuthoredSpawn(true);
        syncAudio(true);
    },
    selectChapter: (index) => {
        playback?.selectChapter(index);
        applyAuthoredSpawn(true);
        syncAudio(true);
    },
    snapshot: () => ({
        timeTicks: playback?.timeTicks ?? 0,
        chapterIndex: playback?.chapterIndex ?? 0,
        playing: playback?.playing ?? false,
        waiting: playback?.waiting ?? false,
        durationTicks: playback?.durationTicks ?? 0,
        ticksPerSecond: playback?.document.ticksPerSecond ?? 0,
    }),
};

playPause.addEventListener("click", () => {
    if (playback === null) return;
    if (playback.playing) playback.pause(); else playback.play();
    syncAudio(false);
    updatePlaybackControls();
});
continueButton.addEventListener("click", () => {
    playback?.continue();
    syncAudio(false);
    updatePlaybackControls();
});
restartButton.addEventListener("click", () => {
    playback?.restart();
    applyAuthoredSpawn(true);
    syncAudio(true);
    updatePlaybackControls();
});
skipBack.addEventListener("click", () => {
    playback?.skipBack();
    applyAuthoredSpawn(true);
    syncAudio(true);
    updatePlaybackControls();
});
skipForward.addEventListener("click", () => {
    playback?.skipForward();
    applyAuthoredSpawn(true);
    syncAudio(true);
    updatePlaybackControls();
});
timeline.addEventListener("input", () => {
    playback?.seekTicks(Number(timeline.value));
    applyAuthoredSpawn(true);
    syncAudio(true);
    updatePlaybackControls();
});
chapter.addEventListener("change", () => {
    playback?.selectChapter(Number(chapter.value));
    applyAuthoredSpawn(true);
    syncAudio(true);
    updatePlaybackControls();
});
viewpoint.addEventListener("change", () => {
    if (playback === null) return;
    const state = playback.evaluate().layers.get(Number(viewpoint.value));
    if (state !== undefined) applySpawnPose(state.worldTransform, state.layer.spawnTracking);
});
cameraMode.addEventListener("change", () => {
    controls.setMode(cameraMode.value as CameraMode);
});
renderQuality.addEventListener("change", () => {
    selectedRenderQuality = renderQuality.value as RenderQuality;
    saveRenderQuality(selectedRenderQuality);
    renderer.setPixelRatio(renderPixelRatio(selectedRenderQuality));
    applyXrFramebufferScale();
    resize();
});

audioToggle.addEventListener("click", async () => {
    if (immAudio === null) return;
    if (immAudio.diagnostics.contextState !== "running" && playback?.playing) await immAudio.enable();
    else immAudio.setMuted(!immAudio.diagnostics.muted);
    updateAudioControl();
});

document.addEventListener("visibilitychange", () => {
    void immAudio?.setPageVisible(document.visibilityState !== "hidden");
});

renderer.xr.addEventListener("sessionstart", () => {
    xrRig.position.copy(camera.position);
    xrRig.quaternion.copy(camera.quaternion);
    xrRig.scale.copy(camera.scale);
    camera.position.set(0, 0, 0);
    camera.quaternion.identity();
    camera.scale.set(1, 1, 1);
    xrRig.add(camera);
});
renderer.xr.addEventListener("sessionend", () => {
    camera.position.copy(xrRig.position);
    camera.quaternion.copy(xrRig.quaternion);
    camera.scale.copy(xrRig.scale);
    xrRig.scale.set(1, 1, 1);
    scene.add(camera);
    controls.setMode(cameraMode.value as CameraMode);
});

const parameters = new URLSearchParams(location.search);
if (parameters.get("visual-test") === "1") document.body.classList.add("visual-test");
const initialSource = parameters.get("src") ?? import.meta.env.VITE_IMM_DEFAULT_SOURCE ??
    releaseAssetUrl("fixtures", "sample1.imm");
if (initialSource !== "") void loadUrl(initialSource).catch(() => undefined);

window.addEventListener("resize", resize);
window.addEventListener("beforeunload", () => {
    immView?.dispose();
    void immAudio?.dispose();
    controls.dispose();
    decoder.dispose();
    renderer.dispose();
});
renderer.setAnimationLoop((animationTime) => {
    const now = performance.now();
    const deltaSeconds = Math.max(0, Math.min(0.1, (animationTime - previousAnimationTime) / 1_000));
    previousAnimationTime = animationTime;
    frameCount++;
    if (now - frameStart >= 500) {
        meanFrameMs = (now - frameStart) / frameCount;
        frameStart = now;
        frameCount = 0;
    }
    resize();
    if (!renderer.xr.isPresenting) controls.update(deltaSeconds);
    if (playback !== null && immView !== null) {
        const previousTicks = playback.timeTicks;
        const snapshot = playback.advance(immAudio?.timelineDeltaSeconds(deltaSeconds) ?? deltaSeconds);
        applyAuthoredSpawn(false, snapshot);
        immView.applySnapshot(snapshot, camera);
        syncAudio(playback.timeTicks < previousTicks, snapshot);
        updatePlaybackControls();
    }
    pollGpuTimer();
    const renderStartedAt = measureNextRender ? performance.now() : 0;
    if (timerExtension !== null && timerQuery === null) {
        timerQuery = timerContext.createQuery();
        if (timerQuery !== null) {
            timerContext.beginQuery(timerExtension.TIME_ELAPSED_EXT, timerQuery);
            timerQueryActive = true;
        }
    }
    renderer.render(scene, camera);
    if (timerExtension !== null && timerQueryActive) {
        timerContext.endQuery(timerExtension.TIME_ELAPSED_EXT);
        timerQueryActive = false;
    }
    if (measureNextRender) {
        firstUploadRenderMs = performance.now() - renderStartedAt;
        measureNextRender = false;
    }
});

function pollGpuTimer(): void {
    if (timerExtension === null || timerQuery === null) return;
    if (!timerContext.getQueryParameter(timerQuery, timerContext.QUERY_RESULT_AVAILABLE)) return;
    const disjoint = timerContext.getParameter(timerExtension.GPU_DISJOINT_EXT) as boolean;
    if (!disjoint) {
        gpuFrameMs = Number(timerContext.getQueryParameter(timerQuery, timerContext.QUERY_RESULT)) / 1_000_000;
    }
    timerContext.deleteQuery(timerQuery);
    timerQuery = null;
}

async function loadUrl(url: string): Promise<void> {
    const requestId = beginLoad(`Fetching ${url}…`);
    try {
        const response = await fetch(url, { signal: loadAbortController.signal });
        if (!response.ok) throw new Error(`IMM fetch failed: HTTP ${response.status}`);
        if (requestId !== loadRequestId) return;
        await loadDocument(url.split("/").pop() || url, await response.arrayBuffer(), requestId);
    } catch (error) {
        if (requestId === loadRequestId) {
            status.textContent = error instanceof Error ? error.message : String(error);
        }
        throw error;
    }
}

async function loadDocument(name: string, source: ArrayBuffer, requestId: number): Promise<void> {
    if (requestId !== loadRequestId) return;
    summary.hidden = true;
    status.textContent = `Reading ${formatBytes(source.byteLength)} of IMM metadata…`;
    let document: ImmDocument;
    let remainingWork: StagedLoadWork[] = [];
    if (benchmarkEagerDecode) {
        status.textContent = "Benchmark mode: decoding the complete IMM before playback…";
        document = await decoder.decode(source);
    } else {
        try {
            document = await decoder.openMetadata(source);
            const work = createNativeLoadOrder(document);
            const initialWork = work.filter((item) => item.initial);
            remainingWork = work.filter((item) => !item.initial);
            for (let index = 0; index < initialWork.length; index++) {
                if (requestId !== loadRequestId) return;
                status.textContent = `Buffering native five-second window ${index + 1}/${initialWork.length}…`;
                applyStagedDelta(document, await decodeStagedWork(initialWork[index]!));
            }
        } catch (stagedError) {
            if (requestId !== loadRequestId) return;
            status.textContent = "Staged loading failed; using the eager decoder…";
            document = await decoder.fallbackEager().catch(() => { throw stagedError; });
            remainingWork = [];
        }
    }
    if (requestId !== loadRequestId) return;
    const nextView = new ImmThreeView(document, { renderer, parent: scene });
    const nextAudio = new ImmWebAudio(document);
    try {
        const nextPlayback = new ImmPlaybackController(document);
        nextPlayback.play();
        immView = nextView;
        playback = nextPlayback;
        immAudio = nextAudio;
        configurePlaybackControls(document);
        measureNextRender = true;
        configureViewpoints(document);
        applyAuthoredSpawn(true);
        grid.visible = false;
        renderer.setClearColor(new THREE.Color().fromArray(document.backgroundColor), 1);
        showSummary(name, document, nextView);
        syncAudio(true);
        void nextAudio.prepare().then(() => {
            if (immAudio === nextAudio) updateAudioControl();
        });
        if (remainingWork.length > 0) {
            void continueStagedLoad(name, remainingWork, requestId).catch((error) => {
                if (requestId === loadRequestId) {
                    status.textContent = `Background IMM loading stopped: ${error instanceof Error ? error.message : String(error)}`;
                }
            });
        } else {
            void decoder.release().catch(() => undefined);
        }
    } catch (error) {
        if (immView === nextView) immView = null;
        playback = null;
        if (immAudio === nextAudio) immAudio = null;
        void nextAudio.dispose();
        nextView.dispose();
        resetDocumentState();
        throw error;
    }
}

function decodeStagedWork(work: StagedLoadWork): Promise<ImmStagedDelta> {
    return work.type === "drawing"
        ? decoder.decodeDrawing(work.layerId, work.drawingId)
        : decoder.decodeLayerAsset(work.layerId);
}

function applyStagedDelta(document: ImmDocument, delta: ImmStagedDelta): void {
    const layer = document.layers.find((candidate) => candidate.id === delta.layerId);
    if (layer === undefined) throw new Error(`Staged decoder returned unknown layer ${delta.layerId}`);
    if (delta.type === "drawing") {
        if (delta.drawingId < 0 || delta.drawingId >= layer.drawings.length) {
            throw new Error(`Staged decoder returned unknown drawing ${delta.layerId}/${delta.drawingId}`);
        }
        layer.drawings[delta.drawingId] = delta.drawing;
    } else {
        if (delta.picture !== undefined) layer.picture = delta.picture;
        if (delta.sound !== undefined) layer.sound = delta.sound;
    }
}

async function continueStagedLoad(name: string, work: StagedLoadWork[], requestId: number): Promise<void> {
    for (let index = 0; index < work.length; index++) {
        if (requestId !== loadRequestId) return;
        const delta = await decodeStagedWork(work[index]!);
        if (requestId !== loadRequestId) return;
        const document = playback?.document;
        if (document === undefined) return;
        applyStagedDelta(document, delta);

        const loadedLayer = document.layers.find((layer) => layer.id === work[index]!.layerId);
        if (work[index]!.type === "drawing" || loadedLayer?.type === 3 || loadedLayer?.type === 4) {
            immView?.refreshLayer(
                delta.layerId,
                delta.type === "drawing" ? delta.drawingId : undefined,
                camera,
            );
            if (immView !== null) showSummary(name, document, immView);
        }

        if (loadedLayer?.type === 5) {
            const nextAudio = new ImmWebAudio(document);
            const previousAudio = immAudio;
            immAudio = nextAudio;
            void previousAudio?.dispose();
            void nextAudio.prepare().then(() => {
                if (immAudio === nextAudio) {
                    syncAudio(true);
                    updateAudioControl();
                }
            });
        }
        status.textContent = `${name}: loaded ${index + 1}/${work.length} remaining native-order assets.`;
    }
    if (requestId === loadRequestId) await decoder.release();
}

function beginLoad(message: string): number {
    loadAbortController.abort();
    loadAbortController = new AbortController();
    const requestId = ++loadRequestId;
    resetDocumentState();
    void decoder.release().catch(() => undefined);
    urlInput.setCustomValidity("");
    status.textContent = message;
    return requestId;
}

function resetDocumentState(): void {
    disposeView();
    renderer.renderLists.dispose();
    renderer.setClearColor(idleClearColor, 1);
    grid.visible = true;
    lastMetrics = null;
    firstUploadRenderMs = null;
    measureNextRender = false;
    gpuFrameMs = null;
    summary.textContent = "";
    summary.hidden = true;
    timeline.value = "0";
    timeline.max = "1";
    chapter.replaceChildren();
    viewpoint.replaceChildren();
    playbackTime.value = "0:00 / 0:00";
    playPause.textContent = "Play";
    audioToggle.hidden = true;
    audioToggle.disabled = true;
    audioToggle.textContent = "Enable audio";
    continueButton.disabled = true;
    camera.position.copy(idleCameraPosition);
    camera.quaternion.identity();
    camera.scale.set(1, 1, 1);
    camera.up.set(0, 1, 0);
    controls.reset(idleCameraPosition, idleControlsTarget);
    appliedAuthoredSpawn = "";
}

function configureViewpoints(document: ImmDocument): void {
    const spawnAreas = document.layers.filter((layer) => layer.type === 8);
    viewpoint.replaceChildren(...spawnAreas.map((layer, index) =>
        new Option(layer.name || `Viewpoint ${index + 1}`, String(layer.id))));
    viewpoint.disabled = spawnAreas.length === 0;
}

function disposeView(): void {
    immView?.dispose();
    immView = null;
    void immAudio?.dispose();
    immAudio = null;
    playback = null;
    playbackControls.hidden = true;
}

function configurePlaybackControls(document: ImmDocument): void {
    playbackControls.hidden = false;
    timeline.max = String(document.durationTicks);
    chapter.replaceChildren(...document.chapters.map((_, index) => {
        const option = new Option(`Chapter ${index + 1}`, String(index));
        return option;
    }));
    updatePlaybackControls();
    updateAudioControl();
}

function updateAudioControl(): void {
    const diagnostics = immAudio?.diagnostics;
    const hasAudio = (diagnostics?.soundLayers ?? 0) > 0;
    audioToggle.hidden = !hasAudio;
    audioToggle.disabled = !hasAudio || diagnostics?.available !== true || diagnostics.decodedSounds === 0;
    if (diagnostics?.available !== true) audioToggle.textContent = "Audio unavailable";
    else if (diagnostics.decodedSounds === 0) audioToggle.textContent = "Preparing audio…";
    else if (diagnostics.contextState !== "running" && playback?.playing) audioToggle.textContent = "Enable audio";
    else audioToggle.textContent = diagnostics.muted ? "Unmute" : "Mute";
}

function syncAudio(restart: boolean, snapshot?: ImmPlaybackSnapshot): void {
    if (immAudio === null || playback === null) return;
    const xrCamera = renderer.xr.isPresenting ? renderer.xr.getCamera() : null;
    const listener = xrCamera?.cameras[0] ?? xrCamera ?? camera;
    immAudio.update(snapshot ?? playback.evaluate(), cameraAudioTransform(listener), restart);
    void immAudio.setTransportPlaying(playback.playing);
}

function updatePlaybackControls(): void {
    if (playback === null) return;
    timeline.value = String(playback.timeTicks);
    chapter.value = String(playback.chapterIndex);
    playPause.textContent = playback.playing ? "Pause" : "Play";
    continueButton.disabled = !playback.waiting;
    playbackTime.value = `${formatTime(playback.timeTicks)} / ${formatTime(playback.durationTicks)}`;
}

function formatTime(ticks: number): string {
    const seconds = playback === null ? 0 : Math.floor(ticks / playback.document.ticksPerSecond);
    return `${Math.floor(seconds / 60)}:${String(seconds % 60).padStart(2, "0")}`;
}


function requiredElement<T extends HTMLElement>(id: string): T {
    const element = document.getElementById(id);
    if (element === null) {
        throw new Error(`Required element #${id} was not found`);
    }
    return element as T;
}


function resize(): void {
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    if (canvas.width === Math.round(width * renderer.getPixelRatio()) &&
        canvas.height === Math.round(height * renderer.getPixelRatio())) {
        return;
    }
    renderer.setSize(width, height, false);
    camera.aspect = width / Math.max(height, 1);
    camera.updateProjectionMatrix();
}


function showSummary(
    fileName: string,
    document: Awaited<ReturnType<ImmDecoderClient["decode"]>>,
    view: ImmThreeView,
): void {
    const paintLayers = document.layers.filter((layer) => layer.type === 1);
    const strokeCount = paintLayers.reduce((total, layer) => total + layer.drawings.reduce(
        (drawingTotal, drawing) => drawingTotal + drawing.strokeCount, 0), 0);
    const pointCount = paintLayers.reduce((total, layer) => total + layer.drawings.reduce(
        (drawingTotal, drawing) => drawingTotal + drawing.pointCount, 0), 0);
    const metrics = {
        layers: document.layers.length,
        paintLayers: view.diagnostics.paintLayerCount,
        modelLayers: view.diagnostics.modelLayerCount,
        pictureLayers: view.diagnostics.pictureLayerCount,
        strokes: strokeCount,
        points: pointCount,
        meshes: view.diagnostics.meshCount,
        triangles: view.diagnostics.triangleCount,
        decodeMs: round(document.metrics.decodeMs),
        workerMarshalMs: round(document.metrics.marshalMs),
        workerPackMs: round(document.metrics.packMs),
        geometryUploadMs: round(view.diagnostics.geometryBuildMs),
        alphaMode: view.diagnostics.alphaMode,
        depthBits: view.diagnostics.depthBits,
        stencilBits: view.diagnostics.stencilBits,
        sampleCount: view.diagnostics.sampleCount,
        maxSamples: view.diagnostics.maxSamples,
        programmableSampleMask: view.diagnostics.programmableSampleMask,
        hostCompatibilityWarnings: view.diagnostics.hostCompatibilityWarnings,
        maxTextureSize: view.diagnostics.maxTextureSize,
        colorMode: view.diagnostics.colorMode,
    };
    lastMetrics = metrics;
    status.textContent = `${fileName}: rendered ${metrics.strokes.toLocaleString()} strokes in ${metrics.meshes} meshes.`;
    summary.textContent = JSON.stringify(metrics, null, 2);
    summary.hidden = false;
}


function formatBytes(byteCount: number): string {
    const mebibytes = byteCount / (1024 * 1024);
    return `${mebibytes.toFixed(1)} MiB`;
}

function round(value: number): number {
    return Math.round(value * 10) / 10;
}

function loadRenderQuality(): RenderQuality {
    try {
        return localStorage.getItem(renderQualityStorageKey) === "high" ? "high" : "normal";
    } catch {
        return "normal";
    }
}

function saveRenderQuality(quality: RenderQuality): void {
    try {
        localStorage.setItem(renderQualityStorageKey, quality);
    } catch {
        // Storage can be disabled without affecting the active quality selection.
    }
}

function renderPixelRatio(quality: RenderQuality): number {
    return quality === "high" ? Math.min(window.devicePixelRatio, 2) : 1;
}

function renderXrFramebufferScale(quality: RenderQuality): number {
    return quality === "high" ? 1.5 : 1;
}

function applyXrFramebufferScale(): void {
    if (renderer.xr.isPresenting) return;
    appliedXrFramebufferScale = renderXrFramebufferScale(selectedRenderQuality);
    renderer.xr.setFramebufferScaleFactor(appliedXrFramebufferScale);
}

function applyAuthoredSpawn(force: boolean, snapshot?: ImmPlaybackSnapshot): void {
    if (playback === null) return;
    const active = resolveActiveSpawnArea(
        playback.document,
        playback.timeTicks,
        snapshot ?? playback.evaluate(),
    );
    if (active === undefined) return;
    const key = spawnActivationKey(active);
    if (!force && key === appliedAuthoredSpawn) return;
    appliedAuthoredSpawn = key;
    viewpoint.value = String(active.state.layer.id);
    applySpawnPose(active.state.worldTransform, active.state.layer.spawnTracking);
}

function spawnActivationKey(active: ImmActiveSpawnArea): string {
    return `${active.state.layer.id}:${active.actionTimeTicks ?? "initial"}:${playback?.chapterIndex ?? 0}`;
}

function applySpawnPose(
    transform: ImmActiveSpawnArea["state"]["worldTransform"],
    tracking: "eye" | "floor" | null | undefined,
): void {
    if (renderer.xr.isPresenting) {
        xrRig.position.fromArray(transform.translation);
        xrRig.quaternion.fromArray(transform.rotation).normalize();
        xrRig.scale.setScalar(transform.scale);
        if (transform.flip === 1) xrRig.scale.x *= -1;
        if (transform.flip === 2) xrRig.scale.y *= -1;
        if (transform.flip === 3) xrRig.scale.z *= -1;
    } else {
        controls.setPose(desktopSpawnTransform(transform, tracking));
    }
}
