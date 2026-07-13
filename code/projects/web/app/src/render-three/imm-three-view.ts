import * as THREE from "three";
import {
    IMM_ANIM_DRAW_IN_TIME,
    IMM_LAYER_PAINT,
    IMM_LAYER_PICTURE,
    IMM_PICTURE_2D,
    IMM_PICTURE_CUBEMAP_CROSS,
    IMM_PICTURE_CUBEMAP_VERTICAL,
    IMM_PICTURE_EQUIRECT_MONO,
    IMM_PICTURE_EQUIRECT_STEREO,
    type ImmDocument,
    type ImmLayer,
    type ImmPaintGeometry,
    type ImmTransform,
} from "../format/imm-document";
import { evaluateImmDocument, type ImmEvaluatedLayer } from "../runtime/imm-playback";

export interface ImmThreeDiagnostics {
    paintLayerCount: number;
    pictureLayerCount: number;
    meshCount: number;
    triangleCount: number;
    geometryBuildMs: number;
    alphaMode: "alpha-to-coverage" | "alpha-blend";
    maxTextureSize: number | null;
    colorMode: "srgb-output-no-tone-mapping";
    activeDrawingCount: number;
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

export class ImmThreeView {
    readonly object3d = new THREE.Group();
    readonly diagnostics: ImmThreeDiagnostics;

    readonly #document: ImmDocument;
    readonly #nodes = new Map<number, THREE.Group>();
    readonly #paint = new Map<number, PaintRecord>();
    readonly #pictures = new Map<number, PictureRecord>();
    readonly #resources: Array<{ dispose(): void }> = [];
    readonly #alphaToCoverage: boolean;
    #timeTicks = 0;

    constructor(document: ImmDocument, options: ImmThreeViewOptions = {}) {
        this.#document = document;
        this.object3d.name = "IMM document";
        this.#alphaToCoverage = options.renderer?.getContext().getContextAttributes()?.antialias === true;
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
            pictureLayerCount,
            meshCount,
            triangleCount,
            geometryBuildMs: performance.now() - startedAt,
            alphaMode: this.#alphaToCoverage ? "alpha-to-coverage" : "alpha-blend",
            maxTextureSize: options.renderer?.capabilities.maxTextureSize ?? null,
            colorMode: "srgb-output-no-tone-mapping",
            activeDrawingCount: this.#paint.size,
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
        const snapshot = evaluateImmDocument(this.#document, timeTicks);
        this.#timeTicks = snapshot.timeTicks;
        for (const state of snapshot.layers.values()) this.#applyLayerState(state);
        if (camera !== undefined) this.#updateViewerLocked(camera);
        this.object3d.updateMatrixWorld();
    }

    /** The host owns the renderer and clock; time is an explicit document-relative value. */
    update(timeSeconds: number, camera: THREE.Camera): void {
        this.setTimeSeconds(timeSeconds, camera);
    }

    dispose(): void {
        this.object3d.removeFromParent();
        for (const record of this.#paint.values()) this.#disposePaintResources(record);
        for (const resource of this.#resources) resource.dispose();
        this.#resources.length = 0;
        this.#paint.clear();
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
            }
        }

        const picture = this.#pictures.get(state.layer.id);
        if (picture !== undefined) picture.material.uniforms.immOpacity!.value = state.opacity;
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
            const material = createPaintMaterial(packed.brushType, this.#alphaToCoverage);
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
            geometry = new THREE.PlaneGeometry(picture.height > 0 ? picture.width / picture.height : 1, 1);
            material = createPicture2DMaterial(texture, layer.opacity);
        } else if (picture.contentType === IMM_PICTURE_EQUIRECT_MONO ||
            picture.contentType === IMM_PICTURE_EQUIRECT_STEREO) {
            geometry = new THREE.SphereGeometry(100, 64, 32);
            material = createEquirectMaterial(texture, layer.opacity, picture.contentType === IMM_PICTURE_EQUIRECT_STEREO);
        } else if (picture.contentType === IMM_PICTURE_CUBEMAP_CROSS ||
            picture.contentType === IMM_PICTURE_CUBEMAP_VERTICAL) {
            geometry = new THREE.SphereGeometry(100, 64, 32);
            material = createCubemapAtlasMaterial(texture, layer.opacity, picture.contentType === IMM_PICTURE_CUBEMAP_VERTICAL);
        } else {
            texture.dispose();
            return null;
        }
        const mesh = new THREE.Mesh(geometry, material);
        mesh.name = layer.name;
        mesh.userData.immLayerType = "picture";
        mesh.userData.immPictureType = picture.contentType;
        mesh.renderOrder = picture.contentType === IMM_PICTURE_2D ? 0 : -10_000;
        material.transparent = picture.hasAlpha || layer.opacity < 1;
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
            const lockedWorldPosition = record.node.position.clone().applyQuaternion(worldRotation).add(worldPosition);
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

function createPaintGeometry(packed: ImmPaintGeometry): THREE.BufferGeometry {
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute("position", new THREE.BufferAttribute(packed.positions, 3));
    geometry.setAttribute("color", new THREE.BufferAttribute(packed.colors, 4));
    geometry.setAttribute("immProgress", new THREE.BufferAttribute(packed.progress, 1));
    geometry.setIndex(new THREE.BufferAttribute(packed.indices, 1));
    geometry.computeBoundingBox();
    geometry.computeBoundingSphere();
    return geometry;
}

function createPaintMaterial(brushType: number, alphaToCoverage: boolean): THREE.ShaderMaterial {
    return new THREE.ShaderMaterial({
        uniforms: { immOpacity: { value: 1 }, immDrawIn: { value: 1 } },
        vertexShader: `
            attribute float immProgress;
            varying vec4 immColor; varying float immVertexProgress;
            void main(){ immColor=color; immVertexProgress=immProgress; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0); }
        `,
        fragmentShader: `
            uniform float immOpacity; uniform float immDrawIn;
            varying vec4 immColor; varying float immVertexProgress;
            void main(){
                float reveal=smoothstep(immVertexProgress-0.008,immVertexProgress+0.008,immDrawIn);
                gl_FragColor=vec4(immColor.rgb,immColor.a*immOpacity*reveal);
                #include <tonemapping_fragment>
                #include <colorspace_fragment>
            }
        `,
        vertexColors: true,
        transparent: true,
        depthTest: true,
        depthWrite: true,
        side: brushType <= 1 ? THREE.DoubleSide : THREE.FrontSide,
        alphaToCoverage,
        toneMapped: false,
    });
}

function createPicture2DMaterial(texture: THREE.DataTexture, opacity: number): THREE.ShaderMaterial {
    return new THREE.ShaderMaterial({
        uniforms: { immPicture: { value: texture }, immOpacity: { value: opacity } },
        vertexShader: `varying vec2 immUv; void main(){ immUv=uv; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0); }`,
        fragmentShader: `
            uniform sampler2D immPicture; uniform float immOpacity; varying vec2 immUv;
            void main(){
                vec4 c=texture2D(immPicture,immUv);
                gl_FragColor=vec4(c.rgb,c.a*immOpacity);
                #include <tonemapping_fragment>
                #include <colorspace_fragment>
            }
        `,
        side: THREE.DoubleSide,
        depthTest: true,
        depthWrite: true,
        toneMapped: false,
    });
}

const directionVertexShader = `
    varying vec3 immDirection;
    void main(){ immDirection=normalize(position); gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0); }
`;

function createEquirectMaterial(texture: THREE.DataTexture, opacity: number, stereo: boolean): THREE.ShaderMaterial {
    return new THREE.ShaderMaterial({
        uniforms: { immPicture: { value: texture }, immOpacity: { value: opacity }, immEye: { value: 0 } },
        vertexShader: directionVertexShader,
        fragmentShader: `
            uniform sampler2D immPicture; uniform float immOpacity; uniform float immEye;
            varying vec3 immDirection; const float IMM_PI=3.1415927;
            void main(){
                vec3 d=normalize(immDirection);
                vec2 uv=vec2(0.5+0.5*atan(d.x,-d.z)/IMM_PI,acos(clamp(d.y,-1.0,1.0))/IMM_PI);
                ${stereo ? "uv.y=uv.y*0.5+0.5*immEye;" : ""}
                vec4 c=texture2D(immPicture,uv); gl_FragColor=vec4(c.rgb,c.a*immOpacity);
                #include <tonemapping_fragment>
                #include <colorspace_fragment>
            }
        `,
        toneMapped: false,
    });
}

function createCubemapAtlasMaterial(texture: THREE.DataTexture, opacity: number, vertical: boolean): THREE.ShaderMaterial {
    return new THREE.ShaderMaterial({
        uniforms: { immPicture: { value: texture }, immOpacity: { value: opacity } },
        vertexShader: directionVertexShader,
        fragmentShader: `
            uniform sampler2D immPicture; uniform float immOpacity; varying vec3 immDirection;
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
                    uv=(cell+vec2(f.x,1.0-f.y))/vec2(4.0,3.0);
                `}
                vec4 c=texture2D(immPicture,uv); gl_FragColor=vec4(c.rgb,c.a*immOpacity);
                #include <tonemapping_fragment>
                #include <colorspace_fragment>
            }
        `,
        toneMapped: false,
    });
}

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
