import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { ImmDecoderClient } from "./decoder-client";
import type { ImmDocument } from "./format/imm-document";
import { ImmThreeView } from "./render-three/imm-three-view";
import "./style.css";

const canvas = required<HTMLCanvasElement>("viewport");
const input = required<HTMLInputElement>("file-input");
const status = required<HTMLParagraphElement>("status");
const summary = required<HTMLPreElement>("summary");
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
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
input.addEventListener("change", async () => {
    const file = input.files?.[0];
    if (file === undefined) return;
    input.disabled = true;
    status.textContent = `Decoding ${file.name}…`;
    try {
        const document = await decoder.decode(await file.arrayBuffer());
        view?.dispose();
        view = new ImmThreeView(document, { renderer, parent: scene });
        applySpawn(document);
        const forward = new THREE.Vector3(0, 0, -1).applyQuaternion(camera.quaternion);
        hostCube.position.copy(camera.position).add(forward.multiplyScalar(3));
        renderer.setClearColor(new THREE.Color().fromArray(document.backgroundColor), 1);
        summary.textContent = JSON.stringify({
            immAttachedToHostScene: view.object3d.parent === scene,
            sharedRenderer: true,
            sharedCanvas: renderer.domElement === canvas,
            hostDepthTest: hostCube.material.depthTest,
            immMeshes: view.diagnostics.meshCount,
            triangles: view.diagnostics.triangleCount,
        }, null, 2);
        summary.hidden = false;
        status.textContent = `${file.name}: IMM and host cube share one depth buffer.`;
    } catch (error) {
        status.textContent = error instanceof Error ? error.message : String(error);
    } finally {
        input.disabled = false;
    }
});

renderer.setAnimationLoop(() => {
    resize();
    controls.update();
    view?.update(performance.now() / 1_000, camera);
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
    camera.position.fromArray(spawn.worldTransform.translation);
    camera.quaternion.fromArray(spawn.worldTransform.rotation);
    controls.target.copy(camera.position).add(new THREE.Vector3(0, 0, -10).applyQuaternion(camera.quaternion));
    controls.update();
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
