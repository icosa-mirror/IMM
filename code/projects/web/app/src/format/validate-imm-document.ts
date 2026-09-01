import {
    IMM_DOCUMENT_SCHEMA_VERSION,
    type ImmDocument,
    type ImmDrawing,
    type ImmPaintGeometry,
} from "./imm-document";

interface VersionedSummary {
    schemaVersion: number;
}

interface DrawingDelta {
    type: "drawing";
    layerId: number;
    drawingId: number;
    drawing: ImmDrawing;
    metrics: StagedRequestMetrics;
}

interface AssetDelta {
    type: "asset";
    layerId: number;
    metrics: StagedRequestMetrics;
}

interface StagedRequestMetrics {
    type: "drawing" | "asset";
    layerId: number;
    drawingId?: number;
    decodeMs: number;
    nativeBuildMs: number;
    wasmCopyMs: number;
    packetParseMs: number;
    transferMs: number;
    adapterMs: number;
    lookupMs: number;
    packetBytes: number;
}

export type ValidatedStagedDelta = DrawingDelta | AssetDelta;

export function assertSupportedSummary(summary: VersionedSummary): void {
    assertSchemaVersion(summary.schemaVersion, "summary");
}

export function assertSupportedDocument(document: ImmDocument): void {
    assertSchemaVersion(document.schemaVersion, "document");
    if (!Array.isArray(document.layers)) {
        fail("document.layers must be an array");
    }
    for (const layer of document.layers) {
        if (!Array.isArray(layer.drawings)) {
            fail(`layer ${layer.id} drawings must be an array`);
        }
        layer.drawings.forEach((drawing, drawingId) => assertDrawing(drawing, `layer ${layer.id} drawing ${drawingId}`));
    }
}

export function assertSupportedDelta(delta: ValidatedStagedDelta): void {
    if (delta.type === "drawing") {
        assertNonNegativeInteger(delta.layerId, "drawing delta layerId");
        assertNonNegativeInteger(delta.drawingId, "drawing delta drawingId");
        assertStagedMetrics(delta.metrics, "drawing", delta.layerId, delta.drawingId);
        assertDrawing(delta.drawing, `layer ${delta.layerId} drawing ${delta.drawingId}`);
        return;
    }
    if (delta.type === "asset") {
        assertNonNegativeInteger(delta.layerId, "asset delta layerId");
        assertStagedMetrics(delta.metrics, "asset", delta.layerId);
        return;
    }
    fail("staged delta has an unsupported type");
}

function assertStagedMetrics(
    metrics: StagedRequestMetrics,
    type: "drawing" | "asset",
    layerId: number,
    drawingId?: number,
): void {
    if (metrics === undefined || metrics.type !== type || metrics.layerId !== layerId ||
        metrics.drawingId !== drawingId) {
        fail(`${type} delta metrics identity does not match the request`);
    }
    for (const field of [
        "decodeMs", "nativeBuildMs", "wasmCopyMs", "packetParseMs",
        "transferMs", "adapterMs", "lookupMs", "packetBytes",
    ] as const) {
        if (!Number.isFinite(metrics[field]) || metrics[field] < 0) {
            fail(`${type} delta metrics ${field} must be a finite non-negative number`);
        }
    }
}

function assertSchemaVersion(actual: number, kind: string): void {
    if (actual !== IMM_DOCUMENT_SCHEMA_VERSION) {
        fail(`${kind} schema ${actual} is unsupported; expected ${IMM_DOCUMENT_SCHEMA_VERSION}`);
    }
}

function assertDrawing(drawing: ImmDrawing, context: string): void {
    assertNonNegativeInteger(drawing.strokeCount, `${context} strokeCount`);
    assertNonNegativeInteger(drawing.pointCount, `${context} pointCount`);
    if (!Number.isFinite(drawing.biggestStroke) || drawing.biggestStroke < 0) {
        fail(`${context} biggestStroke must be a finite non-negative number`);
    }
    if (!Array.isArray(drawing.geometries)) {
        fail(`${context} geometries must be an array`);
    }
    const seenBrushes = new Set<number>();
    drawing.geometries.forEach((geometry, index) => {
        assertGeometry(geometry, `${context} geometry ${index}`);
        if (seenBrushes.has(geometry.brushType)) {
            fail(`${context} repeats brush type ${geometry.brushType}`);
        }
        seenBrushes.add(geometry.brushType);
    });
}

function assertGeometry(geometry: ImmPaintGeometry, context: string): void {
    if (!Number.isInteger(geometry.brushType) || geometry.brushType < 0 || geometry.brushType > 4) {
        fail(`${context} brushType must be an integer from 0 through 4`);
    }
    assertTypedArray(geometry.positions, Float32Array, `${context} positions`);
    assertTypedArray(geometry.colors, Float32Array, `${context} colors`);
    assertTypedArray(geometry.directions, Float32Array, `${context} directions`);
    assertTypedArray(geometry.visibility, Uint8Array, `${context} visibility`);
    assertTypedArray(geometry.masks, Uint8Array, `${context} masks`);
    assertTypedArray(geometry.progress, Float32Array, `${context} progress`);
    if (!(geometry.indices instanceof Uint16Array) && !(geometry.indices instanceof Uint32Array)) {
        fail(`${context} indices must be Uint16Array or Uint32Array`);
    }
    if (geometry.positions.length === 0 || geometry.positions.length % 3 !== 0) {
        fail(`${context} positions must contain one or more vec3 values`);
    }
    const vertexCount = geometry.positions.length / 3;
    assertLength(geometry.colors, vertexCount * 4, `${context} colors`);
    assertLength(geometry.directions, vertexCount * 3, `${context} directions`);
    assertLength(geometry.visibility, vertexCount, `${context} visibility`);
    assertLength(geometry.masks, vertexCount, `${context} masks`);
    assertLength(geometry.progress, vertexCount, `${context} progress`);
    if (geometry.indices.length === 0 || geometry.indices.length % 3 !== 0) {
        fail(`${context} indices must contain complete triangles`);
    }
    if (!Number.isInteger(geometry.triangleCount) || geometry.triangleCount !== geometry.indices.length / 3) {
        fail(`${context} triangleCount does not match the index buffer`);
    }
    for (const index of geometry.indices) {
        if (index >= vertexCount) {
            fail(`${context} index ${index} exceeds vertex count ${vertexCount}`);
        }
    }
}

function assertTypedArray<T extends Float32Array | Uint8Array>(
    value: unknown,
    constructor: { new(...args: never[]): T },
    context: string,
): asserts value is T {
    if (!(value instanceof constructor)) {
        fail(`${context} has the wrong typed-array representation`);
    }
}

function assertLength(value: { length: number }, expected: number, context: string): void {
    if (value.length !== expected) {
        fail(`${context} length ${value.length} does not match expected length ${expected}`);
    }
}

function assertNonNegativeInteger(value: number, context: string): void {
    if (!Number.isInteger(value) || value < 0) {
        fail(`${context} must be a non-negative integer`);
    }
}

function fail(message: string): never {
    throw new Error(`Invalid IMM decoder packet: ${message}`);
}
