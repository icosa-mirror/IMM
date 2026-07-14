import * as THREE from "three";
import {
    IMM_ANIM_DRAW_IN_TIME,
    IMM_LAYER_PAINT,
    IMM_LAYER_MODEL,
    IMM_LAYER_PICTURE,
    IMM_KEEP_ALIVE_BLINK,
    IMM_KEEP_ALIVE_WIGGLE,
    IMM_PICTURE_2D,
    IMM_PICTURE_CUBEMAP_CROSS,
    IMM_PICTURE_CUBEMAP_VERTICAL,
    IMM_PICTURE_EQUIRECT_MONO,
    IMM_PICTURE_EQUIRECT_STEREO,
    type ImmDocument,
    type ImmLayer,
    type ImmModel,
    type ImmPaintGeometry,
    type ImmTransform,
} from "../format/imm-document";
import {
    evaluateImmDocument,
    type ImmEvaluatedLayer,
    type ImmPlaybackSnapshot,
} from "../runtime/imm-playback";
import { IMM_BLUE_NOISE_64_BASE64 } from "./blue-noise-data";

type ImmCoverageMode = "sample-mask" | "alpha-to-coverage" | "alpha-hash";

export type ImmHostCompatibilityWarningCode =
    | "depth-buffer"
    | "logarithmic-depth"
    | "reversed-depth"
    | "camera-projection"
    | "camera-near"
    | "camera-far";

export interface ImmHostCompatibilityWarning {
    code: ImmHostCompatibilityWarningCode;
    message: string;
}

export interface ImmHostDepthContract {
    depthBits: number | null;
    logarithmicDepthBuffer: boolean;
    reversedDepthBuffer: boolean;
}

export interface ImmThreeDiagnostics {
    paintLayerCount: number;
    modelLayerCount: number;
    pictureLayerCount: number;
    meshCount: number;
    triangleCount: number;
    geometryBuildMs: number;
    alphaMode: ImmCoverageMode;
    depthBits: number | null;
    stencilBits: number | null;
    sampleCount: number | null;
    maxSamples: number | null;
    programmableSampleMask: boolean;
    maxTextureSize: number | null;
    colorMode: "srgb-output-no-tone-mapping";
    activeDrawingCount: number;
    hostCompatibilityWarnings: ReadonlyArray<ImmHostCompatibilityWarning>;
}

export interface ImmThreeViewOptions {
    renderer?: THREE.WebGLRenderer;
    parent?: THREE.Object3D;
}

interface PaintRecord {
    layer: ImmLayer;
    node: THREE.Group;
    activeDrawing: number;
    resources: Array<{ dispose(): void }>;
    materials: THREE.ShaderMaterial[];
}

interface PictureRecord {
    layer: ImmLayer;
    node: THREE.Group;
    mesh: THREE.Mesh;
    material: THREE.ShaderMaterial;
    viewerLocked: boolean;
}

interface ModelRecord {
    layer: ImmLayer;
    node: THREE.Group;
    mesh: THREE.Mesh;
    material: THREE.ShaderMaterial;
}

export class ImmThreeView {
    readonly object3d = new THREE.Group();
    readonly diagnostics: ImmThreeDiagnostics;

    readonly #document: ImmDocument;
    readonly #nodes = new Map<number, THREE.Group>();
    readonly #paint = new Map<number, PaintRecord>();
    readonly #models = new Map<number, ModelRecord>();
    readonly #pictures = new Map<number, PictureRecord>();
    readonly #resources: Array<{ dispose(): void }> = [];
    readonly #coverageMode: ImmCoverageMode;
    readonly #sampleCount: number | null;
    readonly #hostDepthContract: ImmHostDepthContract;
    readonly #blueNoise: THREE.DataArrayTexture;
    #timeTicks = 0;
    #coverageFrame = 0;

    constructor(document: ImmDocument, options: ImmThreeViewOptions = {}) {
        this.#document = document;
        this.object3d.name = "IMM document";
        const context = options.renderer?.getContext();
        const sampleCount = context === undefined ? null : Number(context.getParameter(context.SAMPLES));
        this.#sampleCount = sampleCount;
        const programmableSampleMask = context !== undefined
            && context.getExtension("OES_sample_variables") !== null;
        this.#hostDepthContract = {
            depthBits: context === undefined ? null : Number(context.getParameter(context.DEPTH_BITS)),
            logarithmicDepthBuffer: options.renderer?.capabilities.logarithmicDepthBuffer ?? false,
            reversedDepthBuffer: options.renderer?.capabilities.reversedDepthBuffer ?? false,
        };
        this.#coverageMode = programmableSampleMask && sampleCount !== null && sampleCount > 0
            ? "sample-mask"
            : sampleCount !== null && sampleCount > 0 ? "alpha-to-coverage" : "alpha-hash";
        this.#blueNoise = createBlueNoiseTexture();
        this.#resources.push(this.#blueNoise);
        const startedAt = performance.now();

        for (const layer of document.layers) {
            const node = new THREE.Group();
            node.name = layer.name;
            node.userData.immLayerId = layer.id;
            this.#nodes.set(layer.id, node);
            const parent = layer.parentId < 0 ? this.object3d : this.#nodes.get(layer.parentId) ?? this.object3d;
            parent.add(node);
        }

        let paintLayerCount = 0;
        let modelLayerCount = 0;
        let pictureLayerCount = 0;
        let meshCount = 0;
        let triangleCount = 0;
        for (const layer of document.layers) {
            const node = this.#nodes.get(layer.id);
            if (node === undefined) continue;
            if (layer.type === IMM_LAYER_PAINT && layer.drawings.length > 0) {
                const record: PaintRecord = {
                    layer,
                    node,
                    activeDrawing: -1,
                    resources: [],
                    materials: [],
                };
                this.#paint.set(layer.id, record);
                const built = this.#activateDrawing(record, 0);
                paintLayerCount++;
                meshCount += built.meshes;
                triangleCount += built.triangles;
            } else if (layer.type === IMM_LAYER_MODEL && layer.model !== undefined) {
                const record = this.#createModel(layer, node);
                this.#models.set(layer.id, record);
                modelLayerCount++;
                meshCount++;
                triangleCount += triangleCountFor(record.mesh.geometry);
            } else if (layer.type === IMM_LAYER_PICTURE && layer.picture !== undefined) {
                const record = this.#createPicture(layer, node);
                if (record !== null) {
                    this.#pictures.set(layer.id, record);
                    pictureLayerCount++;
                    meshCount++;
                    triangleCount += triangleCountFor(record.mesh.geometry);
                }
            }
        }

        this.diagnostics = {
            paintLayerCount,
            modelLayerCount,
            pictureLayerCount,
            meshCount,
            triangleCount,
            geometryBuildMs: performance.now() - startedAt,
            alphaMode: this.#coverageMode,
            depthBits: this.#hostDepthContract.depthBits,
            stencilBits: context === undefined ? null : Number(context.getParameter(context.STENCIL_BITS)),
            sampleCount,
            maxSamples: context === undefined ? null : Number(context.getParameter(
                (context as WebGL2RenderingContext).MAX_SAMPLES,
            )),
            programmableSampleMask,
            maxTextureSize: options.renderer?.capabilities.maxTextureSize ?? null,
            colorMode: "srgb-output-no-tone-mapping",
            activeDrawingCount: this.#paint.size,
            hostCompatibilityWarnings: validateImmHostCompatibility(undefined, this.#hostDepthContract),
        };
        this.setTimeTicks(0);
        options.parent?.add(this.object3d);
    }

    get timeTicks(): number {
        return this.#timeTicks;
    }

    setTimeSeconds(timeSeconds: number, camera?: THREE.Camera): void {
        this.setTimeTicks(Math.round(timeSeconds * this.#document.ticksPerSecond), camera);
    }

    setTimeTicks(timeTicks: number, camera?: THREE.Camera): void {
        this.applySnapshot(evaluateImmDocument(this.#document, timeTicks), camera);
    }

    /** Applies a caller-owned evaluation so multiple consumers can share one frame snapshot. */
    applySnapshot(snapshot: ImmPlaybackSnapshot, camera?: THREE.Camera): void {
        this.#timeTicks = snapshot.timeTicks;
        this.#coverageFrame = Math.floor(snapshot.timeTicks * 60 / this.#document.ticksPerSecond) & 63;
        for (const state of snapshot.layers.values()) this.#applyLayerState(state);
        if (camera !== undefined) {
            this.#updateViewerLocked(camera);
            this.diagnostics.hostCompatibilityWarnings = validateImmHostCompatibility(camera, this.#hostDepthContract);
        }
        this.object3d.updateMatrixWorld();
    }

    /** Pins stochastic coverage for deterministic captures and host-controlled render sequencing. */
    setCoverageFrame(frame: number): void {
        this.#coverageFrame = Math.max(0, Math.trunc(frame)) & 63;
        for (const record of this.#paint.values()) {
            for (const material of record.materials) material.uniforms.immFrame!.value = this.#coverageFrame;
        }
        for (const record of this.#pictures.values()) record.material.uniforms.immFrame!.value = this.#coverageFrame;
        for (const record of this.#models.values()) record.material.uniforms.immFrame!.value = this.#coverageFrame;
    }

    /** The host owns the renderer and clock; time is an explicit document-relative value. */
    update(timeSeconds: number, camera: THREE.Camera): void {
        this.setTimeSeconds(timeSeconds, camera);
    }

    /** Uploads newly decoded content without rebuilding already resident layers. */
    refreshLayer(layerId: number, drawingId?: number, camera?: THREE.Camera): void {
        const layer = this.#document.layers.find((candidate) => candidate.id === layerId);
        const node = this.#nodes.get(layerId);
        if (layer === undefined || node === undefined) return;
        const startedAt = performance.now();

        const paint = this.#paint.get(layerId);
        if (paint !== undefined && (drawingId === undefined || paint.activeDrawing === drawingId)) {
            const previousMeshes = paint.node.children.length;
            const previousTriangles = objectTriangleCount(paint.node);
            paint.activeDrawing = -1;
            const state = evaluateImmDocument(this.#document, this.#timeTicks).layers.get(layerId);
            if (state !== undefined) this.#applyLayerState(state);
            this.diagnostics.meshCount += paint.node.children.length - previousMeshes;
            this.diagnostics.triangleCount += objectTriangleCount(paint.node) - previousTriangles;
        } else if (layer.type === IMM_LAYER_PICTURE && layer.picture !== undefined && !this.#pictures.has(layerId)) {
            const record = this.#createPicture(layer, node);
            if (record !== null) {
                this.#pictures.set(layerId, record);
                this.diagnostics.pictureLayerCount++;
                this.diagnostics.meshCount++;
                this.diagnostics.triangleCount += triangleCountFor(record.mesh.geometry);
            }
        } else if (layer.type === IMM_LAYER_MODEL && layer.model !== undefined && !this.#models.has(layerId)) {
            const record = this.#createModel(layer, node);
            this.#models.set(layerId, record);
            this.diagnostics.modelLayerCount++;
            this.diagnostics.meshCount++;
            this.diagnostics.triangleCount += triangleCountFor(record.mesh.geometry);
        }
        this.diagnostics.geometryBuildMs += performance.now() - startedAt;
        this.setTimeTicks(this.#timeTicks, camera);
    }

    dispose(): void {
        this.object3d.removeFromParent();
        for (const record of this.#paint.values()) this.#disposePaintResources(record);
        for (const resource of this.#resources) resource.dispose();
        this.#resources.length = 0;
        this.#paint.clear();
        this.#models.clear();
        this.#pictures.clear();
        this.#nodes.clear();
        this.object3d.clear();
    }

    #applyLayerState(state: ImmEvaluatedLayer): void {
        const node = this.#nodes.get(state.layer.id);
        if (node === undefined) return;
        node.visible = state.visible;
        applyTransform(node, state.transform);

        const paint = this.#paint.get(state.layer.id);
        if (paint !== undefined) {
            if (paint.activeDrawing !== state.drawingIndex) this.#activateDrawing(paint, state.drawingIndex);
            const drawInEnabled = state.layer.keys.some((key) => key.property === IMM_ANIM_DRAW_IN_TIME);
            for (const material of paint.materials) {
                material.uniforms.immOpacity!.value = state.opacity;
                material.uniforms.immDrawIn!.value = drawInEnabled ? state.drawInTime : 1;
                material.uniforms.immTime!.value = this.#timeTicks / this.#document.ticksPerSecond;
                material.uniforms.immFrame!.value = this.#coverageFrame;
            }
        }

        const picture = this.#pictures.get(state.layer.id);
        if (picture !== undefined) {
            picture.material.uniforms.immOpacity!.value = state.opacity;
            picture.material.uniforms.immFrame!.value = this.#coverageFrame;
        }

        const model = this.#models.get(state.layer.id);
        if (model !== undefined) {
            model.material.uniforms.immOpacity!.value = state.opacity;
            model.material.uniforms.immFrame!.value = this.#coverageFrame;
        }
    }

    #activateDrawing(record: PaintRecord, drawingIndex: number): { meshes: number; triangles: number } {
        this.#disposePaintResources(record);
        record.node.clear();
        record.activeDrawing = drawingIndex;
        const drawing = record.layer.drawings[drawingIndex];
        if (drawing === undefined) return { meshes: 0, triangles: 0 };
        let meshes = 0;
        let triangles = 0;
        for (const packed of drawing.geometries) {
            const geometry = createPaintGeometry(packed);
            const material = createPaintMaterial(
                packed.brushType,
                this.#coverageMode,
                record.layer,
                this.#blueNoise,
                this.#sampleCount,
            );
            const mesh = new THREE.Mesh(geometry, material);
            mesh.name = `${record.layer.name} drawing ${drawingIndex} brush ${packed.brushType}`;
            mesh.userData.immLayerType = "paint";
            mesh.userData.immDrawingIndex = drawingIndex;
            record.node.add(mesh);
            record.resources.push(geometry, material);
            record.materials.push(material);
            meshes++;
            triangles += packed.triangleCount;
        }
        return { meshes, triangles };
    }

    #disposePaintResources(record: PaintRecord): void {
        for (const resource of record.resources) resource.dispose();
        record.resources.length = 0;
        record.materials.length = 0;
    }

    #createModel(layer: ImmLayer, node: THREE.Group): ModelRecord {
        const model = layer.model;
        if (model === undefined) throw new Error(`Model layer ${layer.id} has no geometry`);
        const geometry = createModelGeometry(model);
        const material = createModelMaterial(
            model,
            layer.opacity,
            this.#coverageMode,
            this.#blueNoise,
            this.#sampleCount,
        );
        const mesh = new THREE.Mesh(geometry, material);
        mesh.name = layer.name;
        mesh.userData.immLayerType = "model";
        node.add(mesh);
        this.#resources.push(geometry, material);
        return { layer, node, mesh, material };
    }

    #createPicture(layer: ImmLayer, node: THREE.Group): PictureRecord | null {
        const picture = layer.picture;
        if (picture === undefined) return null;
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

        let geometry: THREE.BufferGeometry;
        let material: THREE.ShaderMaterial;
        if (picture.contentType === IMM_PICTURE_2D) {
            const aspect = picture.height > 0 ? picture.width / picture.height : 1;
            geometry = new THREE.PlaneGeometry(2 * aspect, 2);
            material = createPicture2DMaterial(
                texture, layer.opacity, this.#coverageMode, this.#blueNoise, this.#sampleCount,
            );
        } else if (picture.contentType === IMM_PICTURE_EQUIRECT_MONO ||
            picture.contentType === IMM_PICTURE_EQUIRECT_STEREO) {
            geometry = new THREE.SphereGeometry(100, 64, 32);
            material = createEquirectMaterial(
                texture, layer.opacity, picture.contentType === IMM_PICTURE_EQUIRECT_STEREO,
                this.#coverageMode, this.#blueNoise, this.#sampleCount,
            );
        } else if (picture.contentType === IMM_PICTURE_CUBEMAP_CROSS ||
            picture.contentType === IMM_PICTURE_CUBEMAP_VERTICAL) {
            geometry = new THREE.SphereGeometry(100, 64, 32);
            material = createCubemapAtlasMaterial(
                texture, layer.opacity, picture.contentType === IMM_PICTURE_CUBEMAP_VERTICAL,
                this.#coverageMode, this.#blueNoise, this.#sampleCount,
            );
        } else {
            texture.dispose();
            return null;
        }
        const mesh = new THREE.Mesh(geometry, material);
        mesh.name = layer.name;
        mesh.userData.immLayerType = "picture";
        mesh.userData.immPictureType = picture.contentType;
        mesh.renderOrder = picture.contentType === IMM_PICTURE_2D ? 0 : -10_000;
        if (picture.contentType !== IMM_PICTURE_2D) {
            material.depthTest = false;
            material.depthWrite = false;
            material.side = THREE.BackSide;
        }
        if (picture.contentType === IMM_PICTURE_EQUIRECT_STEREO) {
            mesh.onBeforeRender = (_renderer, _scene, camera) => {
                const viewport = (camera as THREE.PerspectiveCamera & { viewport?: THREE.Vector4 }).viewport;
                material.uniforms.immEye!.value = viewport !== undefined && viewport.x > 0 ? 1 : 0;
            };
        }
        node.add(mesh);
        this.#resources.push(texture, geometry, material);
        return { layer, node, mesh, material, viewerLocked: picture.viewerLocked };
    }

    #updateViewerLocked(camera: THREE.Camera): void {
        camera.updateMatrixWorld();
        const worldPosition = new THREE.Vector3().setFromMatrixPosition(camera.matrixWorld);
        const worldRotation = new THREE.Quaternion().setFromRotationMatrix(camera.matrixWorld);
        for (const record of this.#pictures.values()) {
            if (!record.viewerLocked) continue;
            const parent = record.node.parent;
            const is2D = record.layer.picture?.contentType === IMM_PICTURE_2D;
            const lockedWorldPosition = is2D
                ? record.node.position.clone().applyQuaternion(worldRotation).add(worldPosition)
                : worldPosition.clone();
            const lockedWorldRotation = worldRotation.clone().multiply(record.node.quaternion);
            if (parent === null) {
                record.node.position.copy(lockedWorldPosition);
                record.node.quaternion.copy(lockedWorldRotation);
            } else {
                parent.updateMatrixWorld();
                record.node.position.copy(parent.worldToLocal(lockedWorldPosition));
                const parentRotation = new THREE.Quaternion().setFromRotationMatrix(parent.matrixWorld).invert();
                record.node.quaternion.copy(parentRotation.multiply(lockedWorldRotation));
            }
        }
    }
}

function objectTriangleCount(object: THREE.Object3D): number {
    let triangles = 0;
    object.traverse((child) => {
        if (child instanceof THREE.Mesh) triangles += triangleCountFor(child.geometry);
    });
    return triangles;
}

export function validateImmHostCompatibility(
    camera: THREE.Camera | undefined,
    depth: ImmHostDepthContract,
): ImmHostCompatibilityWarning[] {
    const warnings: ImmHostCompatibilityWarning[] = [];
    if (depth.depthBits !== null && depth.depthBits < 24) {
        warnings.push({
            code: "depth-buffer",
            message: `Host framebuffer has ${depth.depthBits} depth bits; native desktop parity uses D24S8.`,
        });
    }
    if (depth.logarithmicDepthBuffer) {
        warnings.push({
            code: "logarithmic-depth",
            message: "Host renderer uses logarithmic depth; native desktop parity uses linear depth.",
        });
    }
    if (depth.reversedDepthBuffer) {
        warnings.push({
            code: "reversed-depth",
            message: "Host renderer uses reversed depth; native desktop parity uses conventional LESS_EQUAL depth.",
        });
    }
    if (camera === undefined) return warnings;
    if (!(camera instanceof THREE.PerspectiveCamera)) {
        warnings.push({
            code: "camera-projection",
            message: "Host camera is not perspective; native desktop parity uses a perspective projection.",
        });
        return warnings;
    }
    if (!approximatelyEqual(camera.near, 0.01)) {
        warnings.push({
            code: "camera-near",
            message: `Host camera near plane is ${camera.near}; native desktop parity uses 0.01.`,
        });
    }
    if (!approximatelyEqual(camera.far, 20_000)) {
        warnings.push({
            code: "camera-far",
            message: `Host camera far plane is ${camera.far}; native desktop parity uses 20000.`,
        });
    }
    return warnings;
}

function approximatelyEqual(actual: number, expected: number): boolean {
    return Math.abs(actual - expected) <= Math.max(1e-6, Math.abs(expected) * 1e-6);
}

function createPaintGeometry(packed: ImmPaintGeometry): THREE.BufferGeometry {
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute("position", new THREE.BufferAttribute(packed.positions, 3));
    geometry.setAttribute("color", new THREE.BufferAttribute(packed.colors, 4));
    geometry.setAttribute("immDirection", new THREE.BufferAttribute(packed.directions, 3));
    geometry.setAttribute("immVisibility", new THREE.BufferAttribute(packed.visibility, 1));
    geometry.setAttribute("immMask", new THREE.BufferAttribute(packed.masks, 1));
    geometry.setAttribute("immProgress", new THREE.BufferAttribute(packed.progress, 1));
    geometry.setIndex(new THREE.BufferAttribute(packed.indices, 1));
    geometry.computeBoundingBox();
    geometry.computeBoundingSphere();
    return geometry;
}

function createModelGeometry(model: ImmModel): THREE.BufferGeometry {
    const vertexCount = model.positions.length / 3;
    if (!Number.isInteger(vertexCount) || vertexCount === 0 || model.colors.length !== vertexCount * 3 ||
        (model.normals.length !== 0 && model.normals.length !== vertexCount * 3) ||
        model.indices.length === 0 || model.indices.length % 3 !== 0) {
        throw new RangeError("IMM model buffers do not describe indexed RGB triangles");
    }
    for (const index of model.indices) {
        if (index >= vertexCount) throw new RangeError(`IMM model index ${index} exceeds ${vertexCount} vertices`);
    }
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute("position", new THREE.BufferAttribute(model.positions, 3));
    geometry.setAttribute("color", new THREE.BufferAttribute(model.colors, 3));
    if (model.normals.length > 0) geometry.setAttribute("normal", new THREE.BufferAttribute(model.normals, 3));
    geometry.setIndex(new THREE.BufferAttribute(model.indices, 1));
    geometry.computeBoundingBox();
    geometry.computeBoundingSphere();
    return geometry;
}

function createModelMaterial(
    model: ImmModel,
    opacity: number,
    coverageMode: ImmCoverageMode,
    blueNoise: THREE.DataArrayTexture,
    sampleCount: number | null,
): THREE.RawShaderMaterial {
    const activeSamples = Math.max(1, Math.min(8, sampleCount ?? 1));
    return new THREE.RawShaderMaterial({
        glslVersion: THREE.GLSL3,
        defines: {
            IMM_SAMPLE_MASK: coverageMode === "sample-mask" ? 1 : 0,
            IMM_ALPHA_HASH: coverageMode === "alpha-hash" ? 1 : 0,
            IMM_SAMPLE_COUNT: activeSamples,
        },
        uniforms: {
            immOpacity: { value: opacity }, immBlueNoise: { value: blueNoise }, immFrame: { value: 0 },
        },
        vertexShader: `
            precision highp float; precision highp int;
            uniform mat4 modelViewMatrix; uniform mat4 projectionMatrix;
            in vec3 position; in vec3 color; out vec3 immColor;
            void main(){ immColor=color; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0); }
        `,
        fragmentShader: `
            #if IMM_SAMPLE_MASK == 1
            #extension GL_OES_sample_variables : require
            #endif
            precision highp float; precision highp int;
            uniform float immOpacity; uniform int immFrame; uniform highp sampler2DArray immBlueNoise;
            in vec3 immColor; out vec4 outColor;
            float coverageNoise(){
                ivec2 pixel=ivec2(gl_FragCoord.xy)&ivec2(63);
                return texelFetch(immBlueNoise,ivec3(pixel,immFrame&63),0).r;
            }
            int coverageMask(float alpha,float noise){
                const int sampleCount=IMM_SAMPLE_COUNT;
                uint bits=(1u<<uint(sampleCount))-1u;
                float dithered=clamp(alpha+0.99*(noise-0.5)/float(sampleCount),0.0,1.0);
                uint covered=uint(dithered*float(sampleCount)+0.5);
                uint mask=((bits<<uint(sampleCount))>>covered)&bits;
                uint shift=uint(noise*float(sampleCount-1))%uint(sampleCount);
                return int((((mask<<uint(sampleCount))|mask)>>shift)&bits);
            }
            void main(){
                float coverage=clamp(immOpacity,0.0,1.0); float noise=coverageNoise();
                vec3 srgb=mix(pow(immColor,vec3(0.41666))*1.055-vec3(0.055),immColor*12.92,
                    vec3(lessThanEqual(immColor,vec3(0.0031308))));
                #if IMM_SAMPLE_MASK == 1
                    outColor=vec4(srgb,1.0); gl_SampleMask[0]=coverageMask(coverage,noise);
                #elif IMM_ALPHA_HASH == 1
                    if(coverage<noise) discard; outColor=vec4(srgb,1.0);
                #else
                    outColor=vec4(srgb,coverage);
                #endif
            }
        `,
        transparent: false,
        blending: THREE.NoBlending,
        depthTest: true,
        depthWrite: true,
        depthFunc: THREE.LessEqualDepth,
        side: THREE.DoubleSide,
        alphaToCoverage: coverageMode === "alpha-to-coverage",
        wireframe: model.wireframe,
        toneMapped: false,
    });
}

function createBlueNoiseTexture(): THREE.DataArrayTexture {
    const encoded = atob(IMM_BLUE_NOISE_64_BASE64);
    const bytes = new Uint8Array(encoded.length);
    for (let index = 0; index < encoded.length; index++) bytes[index] = encoded.charCodeAt(index);
    const texture = new THREE.DataArrayTexture(bytes, 64, 64, 64);
    texture.name = "IMM native 64x64x64 blue noise";
    texture.format = THREE.RedFormat;
    texture.type = THREE.UnsignedByteType;
    texture.minFilter = THREE.NearestFilter;
    texture.magFilter = THREE.NearestFilter;
    texture.wrapS = THREE.RepeatWrapping;
    texture.wrapT = THREE.RepeatWrapping;
    texture.generateMipmaps = false;
    texture.unpackAlignment = 1;
    texture.needsUpdate = true;
    return texture;
}

function createPaintMaterial(
    brushType: number,
    coverageMode: ImmCoverageMode,
    layer: ImmLayer,
    blueNoise: THREE.DataArrayTexture,
    sampleCount: number | null,
): THREE.RawShaderMaterial {
    const keepAlive = layer.keepAlive;
    const parameters = keepAlive?.parameters ?? [];
    const activeSamples = Math.max(1, Math.min(8, sampleCount ?? 1));
    return new THREE.RawShaderMaterial({
        glslVersion: THREE.GLSL3,
        defines: {
            IMM_SAMPLE_MASK: coverageMode === "sample-mask" ? 1 : 0,
            IMM_ALPHA_HASH: coverageMode === "alpha-hash" ? 1 : 0,
            IMM_SAMPLE_COUNT: activeSamples,
        },
        uniforms: {
            immOpacity: { value: 1 }, immDrawIn: { value: 1 }, immTime: { value: 0 }, immFrame: { value: 0 },
            immBlueNoise: { value: blueNoise },
            immKeepAliveType: { value: keepAlive?.type ?? 0 },
            immWaveform: { value: keepAlive?.waveform ?? 0 },
            immWiggle: { value: new THREE.Vector3(parameters[0] ?? 0, parameters[1] ?? 0, parameters[2] ?? 0) },
            immBlink: { value: new THREE.Vector4(parameters[0] ?? 0, parameters[1] ?? 1, parameters[2] ?? 1, parameters[3] ?? 0) },
            immBlinkMaxIn: { value: parameters[4] ?? 1 },
        },
        vertexShader: `
            precision highp float; precision highp int;
            uniform mat4 modelViewMatrix; uniform mat4 projectionMatrix;
            in vec3 position; in vec4 color; in float immProgress; in vec3 immDirection;
            in float immVisibility; in float immMask;
            out vec4 immColor; out float immVertexProgress; out float immDirectional; flat out float immMaskSeed;
            uniform int immKeepAliveType; uniform float immTime; uniform vec3 immWiggle;
            void main(){
                immColor=color; immVertexProgress=immProgress; immMaskSeed=immMask;
                vec3 animatedPosition=position;
                if(immKeepAliveType==${IMM_KEEP_ALIVE_WIGGLE}) animatedPosition+=immWiggle.z*sin(immWiggle.x*position.yzx+immWiggle.y*immTime);
                vec3 cpos=(modelViewMatrix*vec4(animatedPosition,1.0)).xyz;
                immDirectional=1.0;
                if(immVisibility<0.5){
                    vec3 viewDirection=normalize(mat3(modelViewMatrix)*immDirection);
                    immDirectional=pow(clamp(dot(viewDirection,normalize(cpos)),0.0,1.0),2.0);
                }
                gl_Position=projectionMatrix*vec4(cpos,1.0);
            }
        `,
        fragmentShader: `
            #if IMM_SAMPLE_MASK == 1
            #extension GL_OES_sample_variables : require
            #endif
            precision highp float; precision highp int;
            uniform float immOpacity; uniform float immDrawIn;
            uniform int immKeepAliveType; uniform int immWaveform; uniform float immTime; uniform int immFrame;
            uniform vec4 immBlink; uniform float immBlinkMaxIn;
            uniform highp sampler2DArray immBlueNoise;
            in vec4 immColor; in float immVertexProgress; in float immDirectional; flat in float immMaskSeed;
            out vec4 outColor;
            float coverageNoise(){
                ivec2 pixel=ivec2(gl_FragCoord.xy)&ivec2(63);
                return texelFetch(immBlueNoise,ivec3(pixel,immFrame&63),0).r;
            }
            int nativeCoverageMask(float alpha,float noise){
                const int sampleCount=IMM_SAMPLE_COUNT;
                uint bits=(1u<<uint(sampleCount))-1u;
                float dithered=clamp(alpha+0.99*(noise-0.5)/float(sampleCount),0.0,1.0);
                uint covered=uint(dithered*float(sampleCount)+0.5);
                uint mask=((bits<<uint(sampleCount))>>covered)&bits;
                uint shift=(uint(noise*float(sampleCount-1))+uint(max(immMaskSeed,0.0)))%uint(sampleCount);
                return int((((mask<<uint(sampleCount))|mask)>>shift)&bits);
            }
            float keepAliveWave(){
                float phase=fract(immTime*immBlink.x);
                if(immWaveform==1) return phase<0.5?0.0:1.0;
                if(immWaveform==2) return phase;
                if(immWaveform==3) return 1.0-abs(2.0*phase-1.0);
                return 0.5+0.5*sin(6.2831853*phase);
            }
            void main(){
                float reveal=smoothstep(0.3,1.0,2.0*immDrawIn-immVertexProgress);
                float blink=1.0;
                if(immKeepAliveType==${IMM_KEEP_ALIVE_BLINK}){
                    float mapped=clamp((keepAliveWave()-immBlink.w)/max(immBlinkMaxIn-immBlink.w,0.00001),0.0,1.0);
                    blink=mix(immBlink.y,immBlink.z,mapped);
                }
                float coverage=clamp(immColor.a*immOpacity*immDirectional*reveal*blink,0.0,1.0);
                float noise=coverageNoise();
                vec4 linearColor=vec4(immColor.rgb,1.0);
                vec4 srgbColor=vec4(mix(pow(linearColor.rgb,vec3(0.41666))*1.055-vec3(0.055),
                    linearColor.rgb*12.92,vec3(lessThanEqual(linearColor.rgb,vec3(0.0031308)))),1.0);
                #if IMM_SAMPLE_MASK == 1
                    outColor=srgbColor;
                    gl_SampleMask[0]=nativeCoverageMask(coverage,noise);
                #elif IMM_ALPHA_HASH == 1
                    if(coverage<noise) discard;
                    outColor=srgbColor;
                #else
                    outColor=vec4(srgbColor.rgb,coverage);
                #endif
            }
        `,
        transparent: false,
        blending: THREE.NoBlending,
        depthTest: true,
        depthWrite: true,
        depthFunc: THREE.LessEqualDepth,
        side: brushType <= 1 ? THREE.DoubleSide : THREE.FrontSide,
        alphaToCoverage: coverageMode === "alpha-to-coverage",
        toneMapped: false,
    });
}

function createPicture2DMaterial(
    texture: THREE.DataTexture,
    opacity: number,
    coverageMode: ImmCoverageMode,
    blueNoise: THREE.DataArrayTexture,
    sampleCount: number | null,
): THREE.RawShaderMaterial {
    return createCoveragePictureMaterial({
        texture, opacity, coverageMode, blueNoise, sampleCount,
        vertexShader: `${PICTURE_VERTEX_HEADER} in vec2 uv; out vec2 immUv;
            void main(){ immUv=uv; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0); }`,
        fragmentBody: `in vec2 immUv;
            void main(){ immWriteCoverage(texture(immPicture,vec2(immUv.x,1.0-immUv.y))); }`,
        side: THREE.DoubleSide,
    });
}

const PICTURE_VERTEX_HEADER = `
    precision highp float; precision highp int;
    uniform mat4 modelViewMatrix; uniform mat4 projectionMatrix; in vec3 position;
`;

const directionVertexShader = `${PICTURE_VERTEX_HEADER}
    out vec3 immDirection;
    void main(){ immDirection=normalize(position); gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0); }
`;

function createEquirectMaterial(
    texture: THREE.DataTexture,
    opacity: number,
    stereo: boolean,
    coverageMode: ImmCoverageMode,
    blueNoise: THREE.DataArrayTexture,
    sampleCount: number | null,
): THREE.RawShaderMaterial {
    return createCoveragePictureMaterial({
        texture, opacity, coverageMode, blueNoise, sampleCount, vertexShader: directionVertexShader,
        extraUniforms: { immEye: { value: 0 } },
        fragmentBody: `
            uniform float immEye; in vec3 immDirection; const float IMM_PI=3.1415927;
            void main(){
                vec3 d=normalize(immDirection);
                vec2 uv=vec2(0.5+0.5*atan(d.x,-d.z)/IMM_PI,acos(clamp(d.y,-1.0,1.0))/IMM_PI);
                ${stereo ? "uv.y=uv.y*0.5+0.5*immEye;" : ""}
                immWriteCoverage(texture(immPicture,uv));
            }
        `,
    });
}

function createCubemapAtlasMaterial(
    texture: THREE.DataTexture,
    opacity: number,
    vertical: boolean,
    coverageMode: ImmCoverageMode,
    blueNoise: THREE.DataArrayTexture,
    sampleCount: number | null,
): THREE.RawShaderMaterial {
    return createCoveragePictureMaterial({
        texture, opacity, coverageMode, blueNoise, sampleCount, vertexShader: directionVertexShader,
        fragmentBody: `
            in vec3 immDirection;
            vec3 faceUv(vec3 d){
                vec3 a=abs(d); float face; vec2 uv;
                if(a.x>=a.y&&a.x>=a.z){ face=d.x>0.0?0.0:1.0; uv=vec2(d.x>0.0?-d.z:d.z,d.y)/a.x; }
                else if(a.y>=a.z){ face=d.y>0.0?2.0:3.0; uv=vec2(d.x,d.y>0.0?-d.z:d.z)/a.y; }
                else { face=d.z>0.0?4.0:5.0; uv=vec2(d.z>0.0?d.x:-d.x,d.y)/a.z; }
                return vec3(uv*0.5+0.5,face);
            }
            void main(){
                vec3 f=faceUv(normalize(immDirection)); vec2 uv;
                ${vertical ? `uv=vec2(f.x,(f.z+1.0-f.y)/6.0);` : `
                    vec2 cell;
                    if(f.z<0.5) cell=vec2(2.0,1.0); else if(f.z<1.5) cell=vec2(0.0,1.0);
                    else if(f.z<2.5) cell=vec2(1.0,0.0); else if(f.z<3.5) cell=vec2(1.0,2.0);
                    else if(f.z<4.5) cell=vec2(1.0,1.0); else cell=vec2(3.0,1.0);
                    vec2 localUv=(f.z<1.5||f.z>=3.5)?vec2(1.0-f.x,1.0-f.y):f.xy;
                    uv=(cell+localUv)/vec2(4.0,3.0);
                `}
                immWriteCoverage(texture(immPicture,uv));
            }
        `,
    });
}

interface CoveragePictureOptions {
    texture: THREE.DataTexture;
    opacity: number;
    coverageMode: ImmCoverageMode;
    blueNoise: THREE.DataArrayTexture;
    sampleCount: number | null;
    vertexShader: string;
    fragmentBody: string;
    extraUniforms?: Record<string, THREE.IUniform>;
    side?: THREE.Side;
}

function createCoveragePictureMaterial(options: CoveragePictureOptions): THREE.RawShaderMaterial {
    const activeSamples = Math.max(1, Math.min(8, options.sampleCount ?? 1));
    return new THREE.RawShaderMaterial({
        glslVersion: THREE.GLSL3,
        defines: {
            IMM_SAMPLE_MASK: options.coverageMode === "sample-mask" ? 1 : 0,
            IMM_ALPHA_HASH: options.coverageMode === "alpha-hash" ? 1 : 0,
            IMM_SAMPLE_COUNT: activeSamples,
        },
        uniforms: {
            immPicture: { value: options.texture }, immOpacity: { value: options.opacity },
            immBlueNoise: { value: options.blueNoise }, immFrame: { value: 0 },
            ...options.extraUniforms,
        },
        vertexShader: options.vertexShader,
        fragmentShader: `${PICTURE_COVERAGE_HEADER}\n${options.fragmentBody}`,
        transparent: false,
        blending: THREE.NoBlending,
        depthTest: true,
        depthWrite: true,
        depthFunc: THREE.LessEqualDepth,
        side: options.side ?? THREE.FrontSide,
        alphaToCoverage: options.coverageMode === "alpha-to-coverage",
        toneMapped: false,
    });
}

const PICTURE_COVERAGE_HEADER = `
    #if IMM_SAMPLE_MASK == 1
    #extension GL_OES_sample_variables : require
    #endif
    precision highp float; precision highp int;
    uniform sampler2D immPicture; uniform highp sampler2DArray immBlueNoise;
    uniform float immOpacity; uniform int immFrame; out vec4 outColor;
    float immCoverageNoise(){
        ivec2 pixel=ivec2(gl_FragCoord.xy)&ivec2(63);
        return texelFetch(immBlueNoise,ivec3(pixel,immFrame&63),0).r;
    }
    int immCoverageMask(float alpha,float noise){
        const int sampleCount=IMM_SAMPLE_COUNT;
        uint bits=(1u<<uint(sampleCount))-1u;
        float dithered=clamp(alpha+0.99*(noise-0.5)/float(sampleCount),0.0,1.0);
        uint covered=uint(dithered*float(sampleCount)+0.5);
        uint mask=((bits<<uint(sampleCount))>>covered)&bits;
        uint shift=uint(noise*float(sampleCount-1))%uint(sampleCount);
        return int((((mask<<uint(sampleCount))|mask)>>shift)&bits);
    }
    vec4 immSrgb(vec4 value){
        return vec4(mix(pow(value.rgb,vec3(0.41666))*1.055-vec3(0.055),value.rgb*12.92,
            vec3(lessThanEqual(value.rgb,vec3(0.0031308)))),1.0);
    }
    void immWriteCoverage(vec4 texel){
        float coverage=clamp(texel.a*immOpacity,0.0,1.0); float noise=immCoverageNoise();
        vec4 color=immSrgb(vec4(texel.rgb,1.0));
        #if IMM_SAMPLE_MASK == 1
            outColor=color; gl_SampleMask[0]=immCoverageMask(coverage,noise);
        #elif IMM_ALPHA_HASH == 1
            if(coverage<noise) discard; outColor=color;
        #else
            outColor=vec4(color.rgb,coverage);
        #endif
    }
`;

function triangleCountFor(geometry: THREE.BufferGeometry): number {
    return geometry.index?.count !== undefined
        ? geometry.index.count / 3
        : geometry.getAttribute("position").count / 3;
}

function applyTransform(object: THREE.Object3D, transform: ImmTransform): void {
    object.position.fromArray(transform.translation);
    object.quaternion.fromArray(transform.rotation);
    object.scale.setScalar(transform.scale);
    if (transform.flip === 1) object.scale.x *= -1;
    if (transform.flip === 2) object.scale.y *= -1;
    if (transform.flip === 3) object.scale.z *= -1;
}
