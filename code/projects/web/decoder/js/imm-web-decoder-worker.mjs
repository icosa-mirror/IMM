import createDecoderModule from "./imm-web-decoder.mjs";


const SUMMARY_SIZE = 72;
const ERROR_SIZE = 176;
const ERROR_MESSAGE_OFFSET = 16;
const ERROR_MESSAGE_CAPACITY = 160;
const MAX_WASM32_SOURCE_SIZE = 0xffff_ffff;

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


async function handleMessage(message, send) {
    const { requestId, type, source } = message;
    if (type !== "inspect") {
        send({
            requestId,
            ok: false,
            error: { status: 1, byteOffset: 0n, message: `Unknown decoder request type: ${type}` },
        });
        return;
    }

    try {
        send({ requestId, ...inspect(source) });
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
    self.onmessage = (event) => handleMessage(event.data, (value) => self.postMessage(value));
} else {
    const { parentPort } = await import("node:worker_threads");
    if (parentPort === null) {
        throw new Error("IMM decoder worker requires a worker message port");
    }
    parentPort.on("message", (value) => handleMessage(value, (response) => parentPort.postMessage(response)));
}
