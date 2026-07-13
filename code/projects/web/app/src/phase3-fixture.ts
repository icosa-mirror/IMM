import * as THREE from "three";
import {
    IMM_ACTION_PLAY,
    IMM_ANIM_ACTION,
    IMM_ANIM_DRAW_IN_TIME,
    IMM_ANIM_OPACITY,
    IMM_ANIM_TRANSFORM,
    IMM_ANIM_VISIBILITY,
    IMM_INTERPOLATION_LINEAR,
    type ImmAnimationKey,
    type ImmDocument,
    type ImmDrawing,
    type ImmLayer,
    type ImmTransform,
} from "./format/imm-document";
import { ImmThreeView } from "./render-three/imm-three-view";
import "./style.css";

const canvas = document.querySelector<HTMLCanvasElement>("#viewport");
if (canvas === null) throw new Error("Phase 3 fixture canvas is missing");
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, preserveDrawingBuffer: true });
renderer.setPixelRatio(1);
renderer.setSize(640, 360, false);
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.NoToneMapping;
const scene = new THREE.Scene();
scene.background = new THREE.Color(0x16202b);
const camera = new THREE.PerspectiveCamera(70, 640 / 360, 0.01, 1_000);
camera.position.set(0, 0, 3);
camera.lookAt(0, 0, 0);
const fixture = createFixture();
const view = new ImmThreeView(fixture, { renderer, parent: scene });

declare global {
    interface Window {
        __phase3Fixture: {
            setTimeTicks(value: number): void;
            state(): Record<string, unknown>;
            moveCameraX(value: number): void;
            samplePicture(contentType: number, direction: [number, number, number], eye?: number): number[];
        };
    }
}

window.__phase3Fixture = {
    setTimeTicks(value) {
        view.setTimeTicks(value, camera);
        renderer.render(scene, camera);
    },
    state() {
        const paintNode = view.object3d.getObjectByProperty("userData.immLayerId", 2) ?? findLayer(view.object3d, 2);
        const paintMesh = findPaintMesh(view.object3d);
        const material = paintMesh?.material as THREE.ShaderMaterial | undefined;
        const pictureTypes: number[] = [];
        const keepAliveTypes: number[] = [];
        view.object3d.traverse((object) => {
            if (object.userData.immLayerType === "picture") pictureTypes.push(Number(object.userData.immPictureType));
            if (object.userData.immLayerType === "paint") {
                const paintMaterial = (object as THREE.Mesh).material as THREE.ShaderMaterial;
                keepAliveTypes.push(Number(paintMaterial.uniforms.immKeepAliveType?.value ?? 0));
            }
        });
        const lockedPicture = findLayer(view.object3d, 3);
        const lockedWorldPosition = lockedPicture?.getWorldPosition(new THREE.Vector3());
        return {
            ready: true,
            timeTicks: view.timeTicks,
            paintVisible: paintNode?.visible ?? false,
            paintX: paintNode?.position.x ?? null,
            drawingIndex: paintMesh?.userData.immDrawingIndex ?? null,
            opacity: material?.uniforms.immOpacity?.value ?? null,
            drawIn: material?.uniforms.immDrawIn?.value ?? null,
            pictureTypes: pictureTypes.sort(),
            keepAliveTypes: keepAliveTypes.sort(),
            lockedPictureX: lockedWorldPosition?.x ?? null,
            gpuGeometries: renderer.info.memory.geometries,
            gpuTextures: renderer.info.memory.textures,
        };
    },
    moveCameraX(value) {
        camera.position.set(value, 0, 3);
        camera.quaternion.identity();
        view.setTimeTicks(view.timeTicks, camera);
        renderer.render(scene, camera);
    },
    samplePicture(contentType, direction, eye = 0) {
        camera.position.set(0, 0, 0);
        camera.lookAt(...direction);
        (camera as THREE.PerspectiveCamera & { viewport?: THREE.Vector4 }).viewport = new THREE.Vector4(eye, 0, 1, 1);
        view.setTimeTicks(0, camera);
        view.object3d.traverse((object) => {
            if (object.userData.immLayerType === "paint") object.visible = false;
            if (object.userData.immLayerType === "picture") object.visible = object.userData.immPictureType === contentType;
        });
        renderer.render(scene, camera);
        const pixel = new Uint8Array(4);
        renderer.getContext().readPixels(320, 180, 1, 1, renderer.getContext().RGBA, renderer.getContext().UNSIGNED_BYTE, pixel);
        return Array.from(pixel);
    },
};
window.__phase3Fixture.setTimeTicks(0);

function createFixture(): ImmDocument {
    const root = makeLayer({ id: 0, parentId: -1, type: 0, isTimeline: true, durationTicks: 400 });
    root.keys = [animationKey(IMM_ANIM_ACTION, 200, { uintValue: IMM_ACTION_PLAY })];
    const timeline = makeLayer({ id: 1, parentId: 0, type: 0, isTimeline: true, durationTicks: 100, maxRepeatCount: 0 });
    timeline.keys = [animationKey(IMM_ANIM_VISIBILITY, 0, { boolValue: true })];
    const paint = makeLayer({
        id: 2,
        parentId: 1,
        type: 1,
        frameRate: 2,
        frameCount: 2,
        maxRepeatCount: 0,
        frameBuffer: new Uint32Array([0, 1]),
        drawings: [drawing(-0.4, 0xff7043), drawing(0.4, 0x42a5f5)],
    });
    paint.keepAlive = { type: 1, waveform: 0, parameters: [3, 2, 0.03, 0, 0, 0] };
    paint.keys = [
        animationKey(IMM_ANIM_VISIBILITY, 0, { boolValue: true }),
        animationKey(IMM_ANIM_OPACITY, 0, { interpolation: IMM_INTERPOLATION_LINEAR, floatValue: 0.25 }),
        animationKey(IMM_ANIM_OPACITY, 100, { floatValue: 1 }),
        animationKey(IMM_ANIM_DRAW_IN_TIME, 0, { interpolation: IMM_INTERPOLATION_LINEAR, doubleValue: 0 }),
        animationKey(IMM_ANIM_DRAW_IN_TIME, 100, { doubleValue: 0.5 }),
        animationKey(IMM_ANIM_TRANSFORM, 0, {
            interpolation: IMM_INTERPOLATION_LINEAR,
            transformValue: { ...identity(), translation: [-1, 0, 0] },
        }),
        animationKey(IMM_ANIM_TRANSFORM, 100, { transformValue: { ...identity(), translation: [1, 0, 0] } }),
    ];
    const pictureLayers = [0, 1, 2, 3, 4].map((contentType, index) => makeLayer({
        id: 3 + index,
        parentId: 0,
        type: 4,
        visible: true,
        localTransform: { ...identity(), translation: contentType === 0 ? [0, -1, -2] : [0, 0, 0] },
        picture: picture(contentType, contentType === 0),
    }));
    const blink = makeLayer({
        id: 8,
        parentId: 1,
        type: 1,
        drawings: [drawing(0, 0xab47bc)],
        localTransform: { ...identity(), translation: [0, 0.9, 0] },
        keepAlive: { type: 2, waveform: 3, parameters: [1, 0.2, 1, 0, 1, 0] },
    });
    blink.keys = [animationKey(IMM_ANIM_VISIBILITY, 0, { boolValue: true })];
    return {
        schemaVersion: 2,
        backgroundColor: [0.08, 0.12, 0.18],
        ticksPerSecond: 100,
        animateOnStart: false,
        durationTicks: 400,
        chapters: [
            { startTicks: 0, endTicks: 200, markerAction: IMM_ACTION_PLAY },
            { startTicks: 200, endTicks: 400, markerAction: IMM_ACTION_PLAY },
        ],
        layers: [root, timeline, paint, blink, ...pictureLayers],
        metrics: { decodeMs: 0, marshalMs: 0, packMs: 0 },
    };
}

function makeLayer(values: Partial<ImmLayer> & Pick<ImmLayer, "id" | "parentId" | "type">): ImmLayer {
    const { id, parentId, type, ...overrides } = values;
    return {
        id,
        parentId,
        type,
        name: `fixture-${id}`,
        visible: true,
        isTimeline: false,
        opacity: 1,
        defaultSpawn: false,
        localTransform: identity(),
        worldTransform: identity(),
        pivotTransform: identity(),
        frameRate: 0,
        frameCount: 0,
        maxRepeatCount: 1,
        durationTicks: 0,
        keys: [],
        frameBuffer: new Uint32Array(),
        drawings: [],
        ...overrides,
    };
}

function animationKey(property: number, timeTicks: number, values: Partial<ImmAnimationKey>): ImmAnimationKey {
    return {
        property,
        interpolation: 0,
        timeTicks,
        boolValue: false,
        uintValue: 0,
        floatValue: 0,
        doubleValue: 0,
        transformValue: identity(),
        ...values,
    };
}

function drawing(x: number, color: number): ImmDrawing {
    const c = new THREE.Color(color);
    return {
        biggestStroke: 1,
        descriptors: new Uint32Array([0, 3, 0, 0]),
        bounds: new Float32Array([-1, -1, 0, 1, 1, 0]),
        points: new Float32Array(),
        pointTimes: new Float32Array([0, 0.25, 0.5]),
        geometries: [{
            brushType: 0,
            triangleCount: 1,
            positions: new Float32Array([x - 0.7, -0.5, 0, x + 0.7, -0.5, 0, x, 0.7, 0]),
            colors: new Float32Array([c.r, c.g, c.b, 1, c.r, c.g, c.b, 1, c.r, c.g, c.b, 1]),
            progress: new Float32Array([0, 0.25, 0.5]),
            indices: new Uint16Array([0, 1, 2]),
        }],
    };
}

function picture(contentType: number, viewerLocked: boolean): NonNullable<ImmLayer["picture"]> {
    const dimensions = contentType === 3 ? [8, 6] : contentType === 4 ? [2, 12] : contentType === 2 ? [8, 8] : [8, 4];
    const [width = 1, height = 1] = dimensions;
    const pixels = new Uint8Array(width * height * 4);
    fill(pixels, width, 0, 0, width, height, [80, 100, 130, 255]);
    if (contentType === 2) {
        fill(pixels, width, 0, 0, width, height / 2, [255, 0, 0, 255]);
        fill(pixels, width, 0, height / 2, width, height / 2, [0, 0, 255, 255]);
    }
    if (contentType === 3) {
        const cells = [[2, 1], [0, 1], [1, 0], [1, 2], [1, 1], [3, 1]];
        cells.forEach(([x = 0, y = 0], face) => fill(pixels, width, x * 2, y * 2, 2, 2, faceColor(face)));
    }
    if (contentType === 4) {
        for (let face = 0; face < 6; face++) fill(pixels, width, 0, face * 2, 2, 2, faceColor(face));
    }
    return { contentType, viewerLocked, width, height, hasAlpha: false, pixels };
}

function faceColor(face: number): [number, number, number, number] {
    return [
        [255, 0, 0, 255],
        [0, 255, 0, 255],
        [0, 0, 255, 255],
        [255, 255, 0, 255],
        [255, 0, 255, 255],
        [0, 255, 255, 255],
    ][face] as [number, number, number, number];
}

function fill(
    pixels: Uint8Array,
    imageWidth: number,
    x0: number,
    y0: number,
    width: number,
    height: number,
    color: [number, number, number, number],
): void {
    for (let y = y0; y < y0 + height; y++) {
        for (let x = x0; x < x0 + width; x++) pixels.set(color, (y * imageWidth + x) * 4);
    }
}

function identity(): ImmTransform {
    return { rotation: [0, 0, 0, 1], scale: 1, flip: 0, translation: [0, 0, 0] };
}

function findLayer(root: THREE.Object3D, id: number): THREE.Object3D | undefined {
    let result: THREE.Object3D | undefined;
    root.traverse((object) => { if (object.userData.immLayerId === id) result = object; });
    return result;
}

function findPaintMesh(root: THREE.Object3D): THREE.Mesh | undefined {
    let result: THREE.Mesh | undefined;
    root.traverse((object) => {
        if (object instanceof THREE.Mesh && object.userData.immLayerType === "paint" && object.parent?.userData.immLayerId === 2) result = object;
    });
    return result;
}
