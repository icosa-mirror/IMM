import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { ImmDecoderClient } from "./decoder-client";
import { ImmThreeView } from "./render-three/imm-three-view";
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
const camera = new THREE.PerspectiveCamera(70, 1, 0.01, 20_000);
camera.position.set(3, 2, 5);

const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;
controls.target.set(0, 0.75, 0);

const grid = new THREE.GridHelper(10, 20, 0x506070, 0x28333f);
scene.add(grid);

const decoder = new ImmDecoderClient();
let immView: ImmThreeView | null = null;

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
        status.textContent = `Decoding ${formatBytes(source.byteLength)} in the decoder worker…`;
        const document = await decoder.decode(source);
        status.textContent = "Building paint geometry…";
        immView?.dispose();
        immView = new ImmThreeView(document);
        scene.add(immView.object3d);
        applyDefaultSpawn(document);
        grid.visible = false;
        renderer.setClearColor(new THREE.Color().fromArray(document.backgroundColor), 1);
        showSummary(file.name, document, immView);
    } catch (error) {
        status.textContent = error instanceof Error ? error.message : String(error);
    } finally {
        fileInput.disabled = false;
    }
});

window.addEventListener("resize", resize);
window.addEventListener("beforeunload", () => {
    immView?.dispose();
    decoder.dispose();
    renderer.dispose();
});
renderer.setAnimationLoop(() => {
    resize();
    controls.update();
    immView?.update(performance.now() / 1_000, camera);
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
        geometryBuildMs: round(view.diagnostics.geometryBuildMs),
    };
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
