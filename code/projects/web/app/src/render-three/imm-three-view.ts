import * as THREE from "three";
import {
    IMM_LAYER_PAINT,
    IMM_LAYER_PICTURE,
    IMM_PICTURE_EQUIRECT_MONO,
    type ImmDocument,
    type ImmLayer,
    type ImmTransform,
} from "../format/imm-document";

export interface ImmThreeDiagnostics {
    paintLayerCount: number;
    pictureLayerCount: number;
    meshCount: number;
    triangleCount: number;
    geometryBuildMs: number;
    alphaMode: "alpha-to-coverage" | "alpha-blend";
    maxTextureSize: number | null;
    colorMode: "srgb-output-no-tone-mapping";
}

export interface ImmThreeViewOptions {
    renderer?: THREE.WebGLRenderer;
    parent?: THREE.Object3D;
}

export class ImmThreeView {
    readonly object3d = new THREE.Group();
    readonly diagnostics: ImmThreeDiagnostics;
    readonly #resources: Array<{ dispose(): void }> = [];

    readonly #alphaToCoverage: boolean;

    constructor(document: ImmDocument, options: ImmThreeViewOptions = {}) {
        this.object3d.name = "IMM document";
        this.#alphaToCoverage = options.renderer?.getContext().getContextAttributes()?.antialias === true;
        const startedAt = performance.now();
        let paintLayerCount = 0;
        let pictureLayerCount = 0;
        let meshCount = 0;
        let triangleCount = 0;
        for (const layer of document.layers) {
            if (!layer.visible) continue;
            if (layer.type === IMM_LAYER_PAINT) {
                const result = this.#addPaintLayer(layer);
                if (result.meshes > 0) paintLayerCount++;
                meshCount += result.meshes;
                triangleCount += result.triangles;
            } else if (layer.type === IMM_LAYER_PICTURE && layer.picture !== undefined) {
                const mesh = this.#addPictureLayer(layer);
                if (mesh !== null) {
                    pictureLayerCount++;
                    meshCount++;
                    triangleCount += mesh.geometry.index?.count !== undefined
                        ? mesh.geometry.index.count / 3
                        : mesh.geometry.getAttribute("position").count / 3;
                }
            }
        }
        this.diagnostics = {
            paintLayerCount,
            pictureLayerCount,
            meshCount,
            triangleCount,
            geometryBuildMs: performance.now() - startedAt,
            alphaMode: this.#alphaToCoverage ? "alpha-to-coverage" : "alpha-blend",
            maxTextureSize: options.renderer?.capabilities.maxTextureSize ?? null,
            colorMode: "srgb-output-no-tone-mapping",
        };
        options.parent?.add(this.object3d);
    }

    update(_timeSeconds: number, _camera: THREE.Camera): void {
        this.object3d.updateMatrixWorld();
    }

    dispose(): void {
        this.object3d.removeFromParent();
        for (const resource of this.#resources) resource.dispose();
        this.#resources.length = 0;
        this.object3d.clear();
    }

    #addPaintLayer(layer: ImmLayer): { meshes: number; triangles: number } {
        const drawingIndex = selectDrawing(layer);
        const drawing = layer.drawings[drawingIndex];
        if (drawing === undefined) return { meshes: 0, triangles: 0 };
        const group = new THREE.Group();
        group.name = layer.name;
        applyTransform(group, layer.worldTransform);
        let meshes = 0;
        let triangles = 0;
        for (const result of drawing.geometries) {
            const geometry = new THREE.BufferGeometry();
            geometry.setAttribute("position", new THREE.BufferAttribute(result.positions, 3));
            geometry.setAttribute("color", new THREE.BufferAttribute(result.colors, 4));
            geometry.setIndex(new THREE.BufferAttribute(result.indices, 1));
            geometry.computeBoundingBox();
            geometry.computeBoundingSphere();
            const material = new THREE.MeshBasicMaterial({
                vertexColors: true,
                transparent: true,
                opacity: layer.opacity,
                depthTest: true,
                depthWrite: true,
                side: result.brushType <= 1 ? THREE.DoubleSide : THREE.FrontSide,
                alphaToCoverage: this.#alphaToCoverage,
                toneMapped: false,
            });
            const mesh = new THREE.Mesh(geometry, material);
            mesh.name = `${layer.name} brush ${result.brushType}`;
            mesh.userData.immLayerType = "paint";
            mesh.frustumCulled = true;
            group.add(mesh);
            this.#resources.push(geometry, material);
            meshes++;
            triangles += result.triangleCount;
        }
        this.object3d.add(group);
        return { meshes, triangles };
    }

    #addPictureLayer(layer: ImmLayer): THREE.Mesh | null {
        const picture = layer.picture;
        if (picture === undefined || picture.contentType !== IMM_PICTURE_EQUIRECT_MONO) return null;
        const texture = new THREE.DataTexture(
            picture.pixels,
            picture.width,
            picture.height,
            THREE.RGBAFormat,
            THREE.UnsignedByteType,
        );
        texture.colorSpace = THREE.SRGBColorSpace;
        texture.wrapS = THREE.RepeatWrapping;
        texture.needsUpdate = true;
        const geometry = new THREE.SphereGeometry(100, 64, 32);
        geometry.scale(-1, 1, 1);
        const material = new THREE.MeshBasicMaterial({
            map: texture,
            side: THREE.FrontSide,
            depthTest: false,
            depthWrite: false,
            toneMapped: false,
        });
        const mesh = new THREE.Mesh(geometry, material);
        mesh.name = layer.name;
        mesh.userData.immLayerType = "picture";
        mesh.renderOrder = -10_000;
        applyTransform(mesh, layer.worldTransform);
        this.object3d.add(mesh);
        this.#resources.push(texture, geometry, material);
        return mesh;
    }
}

function selectDrawing(layer: ImmLayer): number {
    if (layer.frameBuffer.length === 0) return 0;
    const selected = layer.frameBuffer[layer.frameBuffer.length - 1] ?? 0;
    return selected < layer.drawings.length ? selected : 0;
}

function applyTransform(object: THREE.Object3D, transform: ImmTransform): void {
    object.position.fromArray(transform.translation);
    object.quaternion.fromArray(transform.rotation);
    const scale = transform.scale;
    object.scale.set(scale, scale, scale);
    if (transform.flip === 1) object.scale.x *= -1;
    if (transform.flip === 2) object.scale.y *= -1;
    if (transform.flip === 3) object.scale.z *= -1;
}
