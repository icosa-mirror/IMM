import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { desktopSpawnTransform } from "./camera-controls";
import { ImmDecoderClient } from "./decoder-client";
import type { ImmDocument } from "./format/imm-document";
import { ImmThreeView } from "./render-three/imm-three-view";
import "./style.css";

const canvas = required<HTMLCanvasElement>("viewport");
const input = required<HTMLInputElement>("file-input");
const status = required<HTMLParagraphElement>("status");
const summary = required<HTMLPreElement>("summary");
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, stencil: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.NoToneMapping;

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(70, 1, 0.01, 20_000);
camera.position.set(3, 2, 5);
const controls = new OrbitControls(camera, canvas);
const hostCube = new THREE.Mesh(
    new THREE.BoxGeometry(1.25, 1.25, 1.25),
    new THREE.MeshBasicMaterial({ color: 0xff2d73, depthTest: true, depthWrite: true }),
);
hostCube.name = "Host Three.js depth-test cube";
scene.add(hostCube);

const decoder = new ImmDecoderClient();
let view: ImmThreeView | null = null;
let documentTimeSeconds = 0;
let previousAnimationTime = performance.now();

declare global {
    interface Window {
        __immEmbedSetTime: (timeSeconds: number) => void;
    }
}

window.__immEmbedSetTime = (timeSeconds) => {
    documentTimeSeconds = Math.max(0, timeSeconds);
    view?.setTimeSeconds(documentTimeSeconds, camera);
};
input.addEventListener("change", async () => {
    const file = input.files?.[0];
    if (file === undefined) return;
    input.disabled = true;
    status.textContent = `Decoding ${file.name}…`;
    try {
        const document = await decoder.decode(await file.arrayBuffer());
        view?.dispose();
        view = new ImmThreeView(document, { renderer, parent: scene });
        documentTimeSeconds = 0;
        applySpawn(document);
        view.setTimeSeconds(documentTimeSeconds, camera);
        const intersectionTarget = placeCubeOnVisiblePaint(view);
        renderer.setClearColor(new THREE.Color().fromArray(document.backgroundColor), 1);
        summary.textContent = JSON.stringify({
            immAttachedToHostScene: view.object3d.parent === scene,
            sharedRenderer: true,
            sharedCanvas: renderer.domElement === canvas,
            hostDepthTest: hostCube.material.depthTest,
            hostCubeAtPaintVertex: intersectionTarget !== null,
            intersectionTarget,
            immMeshes: view.diagnostics.meshCount,
            immModelLayers: view.diagnostics.modelLayerCount,
            triangles: view.diagnostics.triangleCount,
            hostCompatibilityWarnings: view.diagnostics.hostCompatibilityWarnings,
        }, null, 2);
        summary.hidden = false;
        status.textContent = `${file.name}: IMM and host cube share one depth buffer.`;
    } catch (error) {
        status.textContent = error instanceof Error ? error.message : String(error);
    } finally {
        input.disabled = false;
    }
});

renderer.setAnimationLoop((animationTime) => {
    documentTimeSeconds += Math.max(0, Math.min(0.1, (animationTime - previousAnimationTime) / 1_000));
    previousAnimationTime = animationTime;
    resize();
    controls.update();
    view?.setTimeSeconds(documentTimeSeconds, camera);
    renderer.render(scene, camera);
});
window.addEventListener("beforeunload", () => {
    view?.dispose();
    decoder.dispose();
    hostCube.geometry.dispose();
    hostCube.material.dispose();
    renderer.dispose();
});

function applySpawn(document: ImmDocument): void {
    const spawn = document.layers.find((layer) => layer.type === 8 && layer.defaultSpawn);
    if (spawn === undefined) return;
    const transform = desktopSpawnTransform(spawn.worldTransform, spawn.spawnTracking);
    camera.position.fromArray(transform.translation);
    camera.quaternion.fromArray(transform.rotation);
    controls.target.copy(camera.position).add(new THREE.Vector3(0, 0, -10).applyQuaternion(camera.quaternion));
    controls.update();
}

function placeCubeOnVisiblePaint(view: ImmThreeView): string | null {
    camera.updateMatrixWorld(true);
    view.object3d.updateMatrixWorld(true);
    const worldPoint = new THREE.Vector3();
    const projected = new THREE.Vector3();
    const selectedPoint = new THREE.Vector3();
    let selectedName = "";
    let selectedDistance = Number.POSITIVE_INFINITY;
    view.object3d.traverse((object) => {
        if (!(object instanceof THREE.Mesh) || object.userData.immLayerType !== "paint") return;
        const positions = object.geometry.getAttribute("position");
        const stride = Math.max(1, Math.floor(positions.count / 200));
        for (let index = 0; index < positions.count; index += stride) {
            worldPoint.fromBufferAttribute(positions, index).applyMatrix4(object.matrixWorld);
            projected.copy(worldPoint).project(camera);
            if (Math.abs(projected.x) > 0.75 || Math.abs(projected.y) > 0.75 || projected.z < -1 || projected.z > 1) continue;
            const distance = worldPoint.distanceTo(camera.position);
            if (distance < selectedDistance) {
                selectedPoint.copy(worldPoint);
                selectedName = object.name;
                selectedDistance = distance;
            }
        }
    });
    if (!Number.isFinite(selectedDistance)) {
        hostCube.visible = false;
        return null;
    }
    hostCube.visible = true;
    hostCube.position.copy(selectedPoint);
    hostCube.scale.setScalar(Math.max(0.08, selectedDistance * 0.012));
    return selectedName;
}

function resize(): void {
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    if (canvas.width === Math.round(width * renderer.getPixelRatio()) &&
        canvas.height === Math.round(height * renderer.getPixelRatio())) return;
    renderer.setSize(width, height, false);
    camera.aspect = width / Math.max(height, 1);
    camera.updateProjectionMatrix();
}

function required<T extends HTMLElement>(id: string): T {
    const element = document.getElementById(id);
    if (element === null) throw new Error(`Required element #${id} was not found`);
    return element as T;
}
