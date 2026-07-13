import createDecoderModule from "./imm-web-decoder.mjs";


const SUMMARY_SIZE = 72;
const ERROR_SIZE = 176;
const ERROR_MESSAGE_OFFSET = 16;
const ERROR_MESSAGE_CAPACITY = 160;
const MAX_WASM32_SOURCE_SIZE = 0xffff_ffff;
const LAYER_INFO_SIZE = 276;
const LAYER_NAME_OFFSET = 20;
const LAYER_NAME_CAPACITY = 256;
const TRANSFORM_SIZE = 36;
const ANIMATION_INFO_SIZE = 16;
const STROKE_INFO_SIZE = 40;
const STROKE_POINT_SIZE = 56;
const STROKE_POINT_FLOATS = STROKE_POINT_SIZE / Float32Array.BYTES_PER_ELEMENT;
const PICTURE_INFO_SIZE = 28;

const decoder = await createDecoderModule();


function readSummary(memory, pointer) {
    return {
        schemaVersion: memory.getUint32(pointer, true),
        formatVersion: memory.getUint32(pointer + 4, true),
        sourceSize: memory.getBigUint64(pointer + 8, true),
        chunkCount: memory.getUint32(pointer + 16, true),
        chunkFlags: memory.getUint32(pointer + 20, true),
        sequenceType: memory.getUint32(pointer + 24, true),
        sequenceCapabilities: memory.getUint32(pointer + 28, true),
        sequenceOffset: memory.getBigUint64(pointer + 32, true),
        sequenceSize: memory.getBigUint64(pointer + 40, true),
        resourceTableOffset: memory.getBigUint64(pointer + 48, true),
        resourceTableSize: memory.getBigUint64(pointer + 56, true),
        assetCount: memory.getUint32(pointer + 64, true),
    };
}


function readError(memory, pointer) {
    const status = memory.getUint32(pointer, true);
    const byteOffset = memory.getBigUint64(pointer + 8, true);
    const bytes = decoder.HEAPU8.subarray(
        pointer + ERROR_MESSAGE_OFFSET,
        pointer + ERROR_MESSAGE_OFFSET + ERROR_MESSAGE_CAPACITY,
    );
    const terminator = bytes.indexOf(0);
    const messageBytes = terminator >= 0 ? bytes.subarray(0, terminator) : bytes;
    return {
        status,
        byteOffset,
        message: new TextDecoder().decode(messageBytes),
    };
}


function inspect(source) {
    if (!(source instanceof ArrayBuffer)) {
        throw new TypeError("inspect requires an ArrayBuffer");
    }
    if (source.byteLength > MAX_WASM32_SOURCE_SIZE) {
        throw new RangeError("IMM source exceeds the Wasm32 address space");
    }

    const sourcePointer = decoder._malloc(source.byteLength);
    const summaryPointer = decoder._malloc(SUMMARY_SIZE);
    const errorPointer = decoder._malloc(ERROR_SIZE);
    try {
        decoder.HEAPU8.set(new Uint8Array(source), sourcePointer);
        const status = decoder._imm_web_inspect(
            sourcePointer,
            source.byteLength,
            summaryPointer,
            errorPointer,
        );
        const memory = new DataView(decoder.HEAPU8.buffer);
        if (status !== 0) {
            return { ok: false, error: readError(memory, errorPointer) };
        }
        return { ok: true, summary: readSummary(memory, summaryPointer) };
    } finally {
        decoder._free(errorPointer);
        decoder._free(summaryPointer);
        decoder._free(sourcePointer);
    }
}


function readTransform(memory, pointer) {
    return {
        rotation: [
            memory.getFloat32(pointer, true),
            memory.getFloat32(pointer + 4, true),
            memory.getFloat32(pointer + 8, true),
            memory.getFloat32(pointer + 12, true),
        ],
        scale: memory.getFloat32(pointer + 16, true),
        flip: memory.getUint32(pointer + 20, true),
        translation: [
            memory.getFloat32(pointer + 24, true),
            memory.getFloat32(pointer + 28, true),
            memory.getFloat32(pointer + 32, true),
        ],
    };
}


function readCString(pointer, capacity) {
    const bytes = decoder.HEAPU8.subarray(pointer, pointer + capacity);
    const terminator = bytes.indexOf(0);
    return new TextDecoder().decode(terminator < 0 ? bytes : bytes.subarray(0, terminator));
}


function decodeScene(source) {
    if (!(source instanceof ArrayBuffer)) {
        throw new TypeError("decode requires an ArrayBuffer");
    }
    if (source.byteLength > MAX_WASM32_SOURCE_SIZE) {
        throw new RangeError("IMM source exceeds the Wasm32 address space");
    }

    const startedAt = performance.now();
    const sourcePointer = decoder._malloc(source.byteLength);
    const errorPointer = decoder._malloc(ERROR_SIZE);
    try {
        decoder.HEAPU8.set(new Uint8Array(source), sourcePointer);
        const status = decoder._imm_web_decode_scene(sourcePointer, source.byteLength, errorPointer);
        let memory = new DataView(decoder.HEAPU8.buffer);
        if (status !== 0) {
            return { ok: false, error: readError(memory, errorPointer) };
        }
        const decodedAt = performance.now();
        const backgroundPointer = decoder._malloc(3 * Float32Array.BYTES_PER_ELEMENT);
        const layerPointer = decoder._malloc(LAYER_INFO_SIZE);
        const localPointer = decoder._malloc(TRANSFORM_SIZE);
        const worldPointer = decoder._malloc(TRANSFORM_SIZE);
        const pivotPointer = decoder._malloc(TRANSFORM_SIZE);
        const animationPointer = decoder._malloc(ANIMATION_INFO_SIZE);
        const strokeInfoPointer = decoder._malloc(STROKE_INFO_SIZE);
        const pictureInfoPointer = decoder._malloc(PICTURE_INFO_SIZE);
        try {
            decoder._imm_web_get_background_color(backgroundPointer, 3);
            memory = new DataView(decoder.HEAPU8.buffer);
            const backgroundColor = [
                memory.getFloat32(backgroundPointer, true),
                memory.getFloat32(backgroundPointer + 4, true),
                memory.getFloat32(backgroundPointer + 8, true),
            ];
            const layers = [];
            const transfers = [];
            const layerCount = decoder._imm_web_get_layer_count();
            for (let layerIndex = 0; layerIndex < layerCount; layerIndex++) {
                if (decoder._imm_web_get_layer_info(layerIndex, layerPointer) === 0) {
                    throw new Error(`Could not read decoded layer ${layerIndex}`);
                }
                if (decoder._imm_web_get_layer_transforms(
                    layerIndex, localPointer, worldPointer, pivotPointer) === 0) {
                    throw new Error(`Could not read transforms for decoded layer ${layerIndex}`);
                }
                memory = new DataView(decoder.HEAPU8.buffer);
                const layer = {
                    id: memory.getUint32(layerPointer, true),
                    type: memory.getUint32(layerPointer + 4, true),
                    name: readCString(layerPointer + LAYER_NAME_OFFSET, LAYER_NAME_CAPACITY),
                    visible: memory.getUint32(layerPointer + 12, true) !== 0,
                    opacity: memory.getFloat32(layerPointer + 16, true),
                    localTransform: readTransform(memory, localPointer),
                    worldTransform: readTransform(memory, worldPointer),
                    pivotTransform: readTransform(memory, pivotPointer),
                    frameRate: 0,
                    frameCount: 0,
                    maxRepeatCount: 0,
                    frameBuffer: new Uint32Array(),
                    drawings: [],
                };

                if (layer.type === 1 && decoder._imm_web_get_animation_info(layerIndex, animationPointer) !== 0) {
                    memory = new DataView(decoder.HEAPU8.buffer);
                    layer.frameRate = memory.getUint32(animationPointer, true);
                    layer.frameCount = memory.getUint32(animationPointer + 4, true);
                    layer.maxRepeatCount = memory.getUint32(animationPointer + 8, true);
                    if (layer.frameCount > 0) {
                        const framesPointer = decoder._malloc(layer.frameCount * Uint32Array.BYTES_PER_ELEMENT);
                        try {
                            const frameCount = decoder._imm_web_get_frame_buffer(
                                layerIndex, framesPointer, layer.frameCount);
                            layer.frameBuffer = new Uint32Array(frameCount);
                            layer.frameBuffer.set(new Uint32Array(
                                decoder.HEAPU8.buffer, framesPointer, frameCount));
                            transfers.push(layer.frameBuffer.buffer);
                        } finally {
                            decoder._free(framesPointer);
                        }
                    }

                    const drawingCount = decoder._imm_web_get_drawing_count(layerIndex);
                    for (let drawingIndex = 0; drawingIndex < drawingCount; drawingIndex++) {
                        const strokeCount = decoder._imm_web_get_stroke_count(layerIndex, drawingIndex);
                        const descriptors = new Uint32Array(strokeCount * 4);
                        const bounds = new Float32Array(strokeCount * 6);
                        const pointCounts = new Uint32Array(strokeCount);
                        let totalPointCount = 0;
                        for (let strokeIndex = 0; strokeIndex < strokeCount; strokeIndex++) {
                            if (decoder._imm_web_get_stroke_info(
                                layerIndex, drawingIndex, strokeIndex, strokeInfoPointer) === 0) {
                                throw new Error(`Could not read stroke ${layerIndex}/${drawingIndex}/${strokeIndex}`);
                            }
                            memory = new DataView(decoder.HEAPU8.buffer);
                            const pointCount = memory.getUint32(strokeInfoPointer + 8, true);
                            pointCounts[strokeIndex] = pointCount;
                            descriptors[strokeIndex * 4] = totalPointCount;
                            descriptors[strokeIndex * 4 + 1] = pointCount;
                            descriptors[strokeIndex * 4 + 2] = memory.getUint32(strokeInfoPointer, true);
                            descriptors[strokeIndex * 4 + 3] = memory.getUint32(strokeInfoPointer + 4, true);
                            for (let component = 0; component < 6; component++) {
                                bounds[strokeIndex * 6 + component] = memory.getFloat32(
                                    strokeInfoPointer + 16 + component * 4, true);
                            }
                            totalPointCount += pointCount;
                        }

                        const points = new Float32Array(totalPointCount * STROKE_POINT_FLOATS);
                        const maximumPointCount = pointCounts.reduce((maximum, value) => Math.max(maximum, value), 0);
                        const pointsPointer = maximumPointCount > 0
                            ? decoder._malloc(maximumPointCount * STROKE_POINT_SIZE)
                            : 0;
                        try {
                            for (let strokeIndex = 0; strokeIndex < strokeCount; strokeIndex++) {
                                const pointCount = pointCounts[strokeIndex];
                                if (pointCount === 0) continue;
                                const copied = decoder._imm_web_get_stroke_points(
                                    layerIndex, drawingIndex, strokeIndex, pointsPointer, pointCount);
                                if (copied !== pointCount) {
                                    throw new Error(`Could not read points for stroke ${layerIndex}/${drawingIndex}/${strokeIndex}`);
                                }
                                points.set(
                                    new Float32Array(decoder.HEAPU8.buffer, pointsPointer, copied * STROKE_POINT_FLOATS),
                                    descriptors[strokeIndex * 4] * STROKE_POINT_FLOATS,
                                );
                            }
                        } finally {
                            if (pointsPointer !== 0) decoder._free(pointsPointer);
                        }
                        layer.drawings.push({
                            biggestStroke: decoder._imm_web_get_drawing_biggest_stroke(layerIndex, drawingIndex),
                            descriptors,
                            bounds,
                            points,
                        });
                        transfers.push(descriptors.buffer, bounds.buffer, points.buffer);
                    }
                } else if (layer.type === 4 && decoder._imm_web_get_picture_info(layerIndex, pictureInfoPointer) !== 0) {
                    memory = new DataView(decoder.HEAPU8.buffer);
                    const dataSize = memory.getUint32(pictureInfoPointer + 24, true);
                    const pixelsPointer = decoder._malloc(dataSize);
                    try {
                        const copied = decoder._imm_web_get_picture_pixels(layerIndex, pixelsPointer, dataSize);
                        if (copied !== dataSize) {
                            throw new Error(`Could not read picture pixels for layer ${layerIndex}`);
                        }
                        const pixels = new Uint8Array(dataSize);
                        pixels.set(decoder.HEAPU8.subarray(pixelsPointer, pixelsPointer + dataSize));
                        layer.picture = {
                            contentType: memory.getUint32(pictureInfoPointer + 4, true),
                            viewerLocked: memory.getUint32(pictureInfoPointer + 8, true) !== 0,
                            width: memory.getUint32(pictureInfoPointer + 12, true),
                            height: memory.getUint32(pictureInfoPointer + 16, true),
                            hasAlpha: memory.getUint32(pictureInfoPointer + 20, true) !== 0,
                            pixels,
                        };
                        transfers.push(pixels.buffer);
                    } finally {
                        decoder._free(pixelsPointer);
                    }
                }
                layers.push(layer);
            }
            const marshalledAt = performance.now();
            return {
                ok: true,
                document: {
                    schemaVersion: decoder._imm_web_schema_version(),
                    backgroundColor,
                    layers,
                    metrics: {
                        decodeMs: decodedAt - startedAt,
                        marshalMs: marshalledAt - decodedAt,
                    },
                },
                transfers,
            };
        } finally {
            decoder._free(pictureInfoPointer);
            decoder._free(strokeInfoPointer);
            decoder._free(animationPointer);
            decoder._free(pivotPointer);
            decoder._free(worldPointer);
            decoder._free(localPointer);
            decoder._free(layerPointer);
            decoder._free(backgroundPointer);
            decoder._imm_web_release_scene();
        }
    } finally {
        decoder._free(errorPointer);
        decoder._free(sourcePointer);
    }
}


async function handleMessage(message, send) {
    const { requestId, type, source } = message;
    if (type !== "inspect" && type !== "decode") {
        send({
            requestId,
            ok: false,
            error: { status: 1, byteOffset: 0n, message: `Unknown decoder request type: ${type}` },
        });
        return;
    }

    try {
        if (type === "inspect") {
            send({ requestId, ...inspect(source) });
        } else {
            const result = decodeScene(source);
            const transfers = result.transfers ?? [];
            delete result.transfers;
            send({ requestId, ...result }, transfers);
        }
    } catch (error) {
        send({
            requestId,
            ok: false,
            error: {
                status: 1,
                byteOffset: 0n,
                message: error instanceof Error ? error.message : String(error),
            },
        });
    }
}


if (typeof self !== "undefined" && typeof self.postMessage === "function") {
    self.onmessage = (event) => handleMessage(
        event.data,
        (value, transfers = []) => self.postMessage(value, transfers),
    );
} else {
    const { parentPort } = await import("node:worker_threads");
    if (parentPort === null) {
        throw new Error("IMM decoder worker requires a worker message port");
    }
    parentPort.on("message", (value) => handleMessage(
        value,
        (response, transfers = []) => parentPort.postMessage(response, transfers),
    ));
}
