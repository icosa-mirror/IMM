import { describe, expect, test } from "bun:test";
import {
    PAINT_PACKET_SCHEMA_VERSION,
    parsePaintPacket,
} from "../../decoder/js/imm-web-paint-packet.mjs";

const LAYER_ID = 7;
const DRAWING_ID = 11;

function packet(): Uint8Array {
    const bytes = new Uint8Array(258);
    const view = new DataView(bytes.buffer);
    view.setUint32(0, PAINT_PACKET_SCHEMA_VERSION, true);
    view.setUint32(4, bytes.byteLength, true);
    view.setBigUint64(8, (BigInt(LAYER_ID) << 32n) | BigInt(DRAWING_ID), true);
    view.setUint32(16, 1, true);
    view.setUint32(20, LAYER_ID, true);
    view.setUint32(24, DRAWING_ID, true);
    view.setUint32(28, 1, true);
    view.setUint32(32, 2, true);
    view.setUint32(36, 1, true);
    view.setFloat32(40, 0.5, true);
    view.setUint32(44, 64, true);

    view.setUint32(64, 0, true);
    view.setUint32(68, 3, true);
    view.setUint32(72, 1, true);
    view.setUint32(76, 2, true);
    view.setUint32(80, 112, true);
    view.setUint32(84, 148, true);
    view.setUint32(88, 196, true);
    view.setUint32(92, 232, true);
    view.setUint32(96, 235, true);
    view.setUint32(100, 240, true);
    view.setUint32(104, 252, true);
    view.setUint32(108, 3, true);
    new Uint16Array(bytes.buffer, 252, 3).set([0, 1, 2]);
    return bytes;
}

describe("native paint packet validation", () => {
    test("parses a versioned, aligned packet into zero-copy attribute views", () => {
        const bytes = packet();
        const parsed = parsePaintPacket(bytes, LAYER_ID, DRAWING_ID);
        expect(parsed.resourceId).toBe((BigInt(LAYER_ID) << 32n) | BigInt(DRAWING_ID));
        expect(parsed.generation).toBe(1);
        expect(parsed.drawing.geometries).toHaveLength(1);
        expect(parsed.drawing.geometries[0]?.indices).toEqual(new Uint16Array([0, 1, 2]));
        expect(parsed.drawing.geometries[0]?.positions.buffer).toBe(bytes.buffer);
    });

    test("rejects unsupported schemas and inconsistent identities", () => {
        const schemaMismatch = packet();
        new DataView(schemaMismatch.buffer).setUint32(0, 99, true);
        expect(() => parsePaintPacket(schemaMismatch, LAYER_ID, DRAWING_ID)).toThrow("unsupported");

        const wrongResource = packet();
        new DataView(wrongResource.buffer).setBigUint64(8, 1n, true);
        expect(() => parsePaintPacket(wrongResource, LAYER_ID, DRAWING_ID)).toThrow("Malformed paint packet header");
    });

    test("rejects truncated, misaligned, and out-of-range geometry data", () => {
        const truncated = packet().slice(0, 257);
        expect(() => parsePaintPacket(truncated, LAYER_ID, DRAWING_ID)).toThrow("Malformed paint packet header");

        const misaligned = packet();
        new DataView(misaligned.buffer).setUint32(80, 113, true);
        expect(() => parsePaintPacket(misaligned, LAYER_ID, DRAWING_ID)).toThrow("misaligned");

        const invalidIndex = packet();
        new Uint16Array(invalidIndex.buffer, 252, 3)[2] = 3;
        expect(() => parsePaintPacket(invalidIndex, LAYER_ID, DRAWING_ID)).toThrow("exceeds 3 vertices");
    });
});
