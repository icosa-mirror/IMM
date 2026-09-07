/** Test-only instrumentation of the host's actual Three.js and WebGL operations. */
export async function installDrawingChurnProbe() {
    const THREE = await import("three");
    const viewer = window.__galleryImm.viewer;
    const view = viewer.immAsset.view;
    const gl = viewer.renderer.getContext();
    const originals = [];
    const sourceIds = new WeakMap();
    const seen = new Set();
    const paintGeometries = new WeakSet();
    let nextId = 0, frame, uniqueGeometryBytes = 0;
    const fresh = () => ({ builds: 0, revisits: 0, geometryBytes: 0, disposals: 0,
        bufferUploads: 0, uploadBytes: 0, uploadCallMs: 0, snapshotMs: 0, renderMs: 0,
        boundsMs: 0, boundsCalls: 0, boundsVertices: 0 });
    frame = fresh();
    const sourceId = source => {
        if (!sourceIds.has(source)) sourceIds.set(source, ++nextId);
        return sourceIds.get(source);
    };
    function register(node, mesh, count) {
        if (mesh.userData?.immLayerType !== "paint" || !mesh.geometry) return;
        const geometry = mesh.geometry;
        paintGeometries.add(geometry);
        const arrays = [...Object.values(geometry.attributes).map(a => a.array), geometry.index?.array].filter(Boolean);
        const key = `${node.uuid}:${mesh.userData.immDrawingIndex}:${arrays.map(sourceId).join(",")}`;
        const bytes = [...new Set(arrays)].reduce((sum, a) => sum + a.byteLength, 0);
        if (!seen.has(key)) uniqueGeometryBytes += bytes;
        if (count) {
            frame.builds++;
            frame.revisits += Number(seen.has(key));
            frame.geometryBytes += bytes;
        }
        seen.add(key);
    }
    view.object3d.traverse(mesh => { if (mesh.parent) register(mesh.parent, mesh, false); });
    function wrap(target, name, factory) {
        const original = target[name];
        const replacement = factory(original);
        target[name] = replacement;
        originals.push(() => { if (target[name] === replacement) target[name] = original; });
    }
    wrap(THREE.Object3D.prototype, "add", original => function(...children) {
        for (const child of children) register(this, child, true);
        return original.apply(this, children);
    });
    wrap(THREE.BufferGeometry.prototype, "dispose", original => function(...args) {
        if (paintGeometries.has(this)) frame.disposals++;
        return original.apply(this, args);
    });
    for (const name of ["computeBoundingBox", "computeBoundingSphere"]) {
        wrap(THREE.BufferGeometry.prototype, name, original => function(...args) {
            const start = performance.now();
            const result = original.apply(this, args);
            frame.boundsMs += performance.now() - start;
            frame.boundsCalls++;
            frame.boundsVertices += this.attributes.position?.count ?? 0;
            return result;
        });
    }
    for (const name of ["bufferData", "bufferSubData"]) {
        wrap(gl, name, original => function(...args) {
            const source = args[name === "bufferData" ? 1 : 2];
            const offset = args[3] ?? 0;
            const length = args[4];
            const bytes = typeof source === "number" ? source : source == null ? 0
                : ArrayBuffer.isView(source) && source.BYTES_PER_ELEMENT
                    ? (length || source.length - offset) * source.BYTES_PER_ELEMENT : source.byteLength;
            const start = performance.now();
            const result = original.apply(this, args);
            frame.uploadCallMs += performance.now() - start;
            frame.bufferUploads++;
            frame.uploadBytes += bytes;
            return result;
        });
    }
    wrap(view, "applySnapshot", original => function(...args) {
        const start = performance.now();
        const result = original.apply(this, args);
        frame.snapshotMs += performance.now() - start;
        return result;
    });
    wrap(viewer.renderer, "render", original => function(...args) {
        const start = performance.now();
        const result = original.apply(this, args);
        frame.renderMs += performance.now() - start;
        return result;
    });
    window.__immDrawingProbe = {
        takeFrame() { const result = frame; frame = fresh(); return result; },
        inventory() { return { uniqueGeometries: seen.size, uniqueGeometryBytes }; },
        restore() { for (const restore of originals.reverse()) restore(); delete window.__immDrawingProbe; },
    };
    return { scope: "paint mesh creation/disposal; all WebGL buffer uploads; CPU call timings, not GPU duration" };
}
