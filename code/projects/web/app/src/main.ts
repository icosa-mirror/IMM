import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { ImmDecoderClient } from "./decoder-client";
import type { ImmDocument } from "./format/imm-document";
import { ImmThreeView } from "./render-three/imm-three-view";
import "./style.css";


const canvas = requiredElement<HTMLCanvasElement>("viewport");
const fileInput = requiredElement<HTMLInputElement>("file-input");
const urlForm = requiredElement<HTMLFormElement>("url-form");
const urlInput = requiredElement<HTMLInputElement>("url-input");
const status = requiredElement<HTMLParagraphElement>("status");
const summary = requiredElement<HTMLPreElement>("summary");

const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setClearColor(0x10151d, 1);
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.NoToneMapping;

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(70, 1, 0.01, 20_000);
camera.position.set(3, 2, 5);

const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;
controls.target.set(0, 0.75, 0);

const grid = new THREE.GridHelper(10, 20, 0x506070, 0x28333f);
scene.add(grid);

const decoder = new ImmDecoderClient();
let immView: ImmThreeView | null = null;
let lastMetrics: Record<string, unknown> | null = null;
let frameStart = performance.now();
let frameCount = 0;
let meanFrameMs = 0;

declare global {
    interface Window {
        __immLoadUrl: (url: string) => Promise<void>;
        __immDisposeView: () => void;
        __immDiagnostics: () => Record<string, unknown>;
    }
}

fileInput.addEventListener("change", async () => {
    const file = fileInput.files?.[0];
    if (file === undefined) {
        return;
    }

    try {
        const source = await file.arrayBuffer();
        await loadDocument(file.name, source);
    } catch (error) {
        status.textContent = error instanceof Error ? error.message : String(error);
    } finally {
        fileInput.disabled = false;
    }
});

urlForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    if (urlInput.value === "") return;
    await loadUrl(urlInput.value);
});

window.__immLoadUrl = loadUrl;
window.__immDisposeView = disposeView;
window.__immDiagnostics = () => ({
    ready: immView !== null,
    ...lastMetrics,
    frameMs: round(meanFrameMs),
    fps: meanFrameMs > 0 ? round(1_000 / meanFrameMs) : 0,
    pixelRatio: renderer.getPixelRatio(),
    canvasWidth: canvas.width,
    canvasHeight: canvas.height,
    drawCalls: renderer.info.render.calls,
    renderedTriangles: renderer.info.render.triangles,
    gpuGeometries: renderer.info.memory.geometries,
    gpuTextures: renderer.info.memory.textures,
    jsHeapBytes: "memory" in performance
        ? (performance as Performance & { memory: { usedJSHeapSize: number } }).memory.usedJSHeapSize
        : null,
});

const parameters = new URLSearchParams(location.search);
if (parameters.get("visual-test") === "1") document.body.classList.add("visual-test");
const initialSource = parameters.get("src");
if (initialSource !== null) void loadUrl(initialSource);

window.addEventListener("resize", resize);
window.addEventListener("beforeunload", () => {
    immView?.dispose();
    decoder.dispose();
    renderer.dispose();
});
renderer.setAnimationLoop(() => {
    const now = performance.now();
    frameCount++;
    if (now - frameStart >= 500) {
        meanFrameMs = (now - frameStart) / frameCount;
        frameStart = now;
        frameCount = 0;
    }
    resize();
    controls.update();
    immView?.update(performance.now() / 1_000, camera);
    renderer.render(scene, camera);
});

async function loadUrl(url: string): Promise<void> {
    fileInput.disabled = true;
    urlInput.disabled = true;
    summary.hidden = true;
    status.textContent = `Fetching ${url}…`;
    try {
        const response = await fetch(url);
        if (!response.ok) throw new Error(`IMM fetch failed: HTTP ${response.status}`);
        await loadDocument(url.split("/").pop() || url, await response.arrayBuffer());
    } catch (error) {
        status.textContent = error instanceof Error ? error.message : String(error);
        throw error;
    } finally {
        fileInput.disabled = false;
        urlInput.disabled = false;
    }
}

async function loadDocument(name: string, source: ArrayBuffer): Promise<void> {
    fileInput.disabled = true;
    summary.hidden = true;
    status.textContent = `Decoding ${formatBytes(source.byteLength)} in the decoder worker…`;
    const document = await decoder.decode(source);
    disposeView();
    immView = new ImmThreeView(document, { renderer, parent: scene });
    applyDefaultSpawn(document);
    grid.visible = false;
    renderer.setClearColor(new THREE.Color().fromArray(document.backgroundColor), 1);
    showSummary(name, document, immView);
    fileInput.disabled = false;
}

function disposeView(): void {
    immView?.dispose();
    immView = null;
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
        (drawingTotal, drawing) => drawingTotal + drawing.descriptors.length / 4, 0), 0);
    const pointCount = paintLayers.reduce((total, layer) => total + layer.drawings.reduce(
        (drawingTotal, drawing) => drawingTotal + drawing.points.length / 14, 0), 0);
    const metrics = {
        layers: document.layers.length,
        paintLayers: view.diagnostics.paintLayerCount,
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
        maxTextureSize: view.diagnostics.maxTextureSize,
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

function applyDefaultSpawn(document: Awaited<ReturnType<ImmDecoderClient["decode"]>>): void {
    const spawn = document.layers.find((layer) => layer.type === 8 && layer.defaultSpawn);
    if (spawn === undefined) return;
    camera.position.fromArray(spawn.worldTransform.translation);
    camera.quaternion.fromArray(spawn.worldTransform.rotation);
    const forward = new THREE.Vector3(0, 0, -1).applyQuaternion(camera.quaternion);
    controls.target.copy(camera.position).add(forward.multiplyScalar(10));
    controls.update();
}
