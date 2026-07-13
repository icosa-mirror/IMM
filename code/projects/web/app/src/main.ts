import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { ImmDecoderClient, type ImmDocumentSummary } from "./decoder-client";
import "./style.css";


const canvas = requiredElement<HTMLCanvasElement>("viewport");
const fileInput = requiredElement<HTMLInputElement>("file-input");
const status = requiredElement<HTMLParagraphElement>("status");
const summary = requiredElement<HTMLPreElement>("summary");

const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setClearColor(0x10151d, 1);
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.NoToneMapping;

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(55, 1, 0.01, 1_000);
camera.position.set(3, 2, 5);

const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;
controls.target.set(0, 0.75, 0);

const grid = new THREE.GridHelper(10, 20, 0x506070, 0x28333f);
scene.add(grid);

const decoder = new ImmDecoderClient();

fileInput.addEventListener("change", async () => {
    const file = fileInput.files?.[0];
    if (file === undefined) {
        return;
    }

    fileInput.disabled = true;
    summary.hidden = true;
    status.textContent = `Reading ${file.name}…`;
    try {
        const source = await file.arrayBuffer();
        status.textContent = `Inspecting ${formatBytes(source.byteLength)} in the decoder worker…`;
        const documentSummary = await decoder.inspect(source);
        showSummary(file.name, documentSummary);
    } catch (error) {
        status.textContent = error instanceof Error ? error.message : String(error);
    } finally {
        fileInput.disabled = false;
    }
});

window.addEventListener("resize", resize);
window.addEventListener("beforeunload", () => decoder.dispose());
renderer.setAnimationLoop(() => {
    resize();
    controls.update();
    renderer.render(scene, camera);
});


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


function showSummary(fileName: string, value: ImmDocumentSummary): void {
    status.textContent = `${fileName}: ${value.assetCount} assets in ${value.chunkCount} top-level chunks.`;
    summary.textContent = JSON.stringify(value, (_key, item: unknown) => {
        return typeof item === "bigint" ? item.toString() : item;
    }, 2);
    summary.hidden = false;
}


function formatBytes(byteCount: number): string {
    const mebibytes = byteCount / (1024 * 1024);
    return `${mebibytes.toFixed(1)} MiB`;
}
