import ts from "typescript";

/** Inject a disabled-by-default timer into the test-served bundle only. */
export function instrumentDrawingActivation(source) {
    const file = ts.createSourceFile("imm-three-loader.js", source, ts.ScriptTarget.Latest, true, ts.ScriptKind.JS);
    const matches = [];
    function visit(node) {
        // Vite minifies private method names. Match the activation's mesh annotation instead.
        if (ts.isMethodDeclaration(node) && node.body) {
            const text = node.body.getText(file);
            if (text.includes(" drawing ") && text.includes(" brush ") && text.includes("immDrawingIndex")) matches.push(node);
        }
        ts.forEachChild(node, visit);
    }
    visit(file);
    if (matches.length !== 1) throw new Error(`Expected one drawing activation method, found ${matches.length}`);
    const body = matches[0].body;
    const start = body.getStart(file), end = body.end;
    return `${source.slice(0, start)}{
        const immProbe = globalThis.__immDrawingActivationTimer;
        const immStartedAt = immProbe ? performance.now() : 0;
        try { ${source.slice(start + 1, end - 1)} }
        finally { if (immProbe) immProbe(performance.now() - immStartedAt); }
    }${source.slice(end)}`;
}

/** Lightweight CPU and upload counters; no per-mesh bookkeeping. */
export function installDrawingActivationTimer() {
    const viewer = window.__galleryImm.viewer;
    const gl = viewer.renderer.getContext();
    const originals = [];
    const fresh = () => ({ activations: 0, activationMs: 0, bufferUploads: 0, uploadBytes: 0,
        uploadCallMs: 0, renderMs: 0 });
    let frame = fresh();
    window.__immDrawingActivationTimer = elapsed => { frame.activations++; frame.activationMs += elapsed; };
    function wrap(target, name, factory) {
        const original = target[name];
        const replacement = factory(original);
        target[name] = replacement;
        originals.push(() => { if (target[name] === replacement) target[name] = original; });
    }
    for (const name of ["bufferData", "bufferSubData"]) {
        wrap(gl, name, original => function(...args) {
            const source = args[name === "bufferData" ? 1 : 2];
            const offset = args[3] ?? 0, length = args[4];
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
    wrap(viewer.renderer, "render", original => function(...args) {
        const start = performance.now();
        const result = original.apply(this, args);
        frame.renderMs += performance.now() - start;
        return result;
    });
    window.__immDrawingProbe = {
        takeFrame() { const result = frame; frame = fresh(); return result; },
        inventory() { return { scope: "drawing activation CPU includes disposal, bounds, geometry and material creation; upload timing is CPU submission, not GPU duration" }; },
        restore() {
            for (const restore of originals.reverse()) restore();
            delete window.__immDrawingProbe;
            delete window.__immDrawingActivationTimer;
        },
    };
    return { lightweight: true };
}
