import assert from "node:assert/strict";
import { packPaintGeometry, SECTION_COUNTS } from "../js/imm-web-geometry.mjs";

const POINT_FLOATS = 14;
const strokeCount = SECTION_COUNTS.length;
const descriptors = new Uint32Array(strokeCount * 4);
const points = new Float32Array(strokeCount * 2 * POINT_FLOATS);

for (let brushType = 0; brushType < strokeCount; brushType++) {
    descriptors.set([brushType * 2, 2, brushType, 0], brushType * 4);
    writePoint(brushType * 2, 0, 0, brushType * 3, brushType);
    writePoint(brushType * 2 + 1, 0, 0, brushType * 3 + 2, brushType);
}

const pointTimes = new Float32Array(10);
for (let index = 0; index < pointTimes.length; index++) pointTimes[index] = index / 10;
const geometries = packPaintGeometry({ descriptors, points, pointTimes });
assert.equal(geometries.length, 5);
for (let brushType = 0; brushType < geometries.length; brushType++) {
    const geometry = geometries[brushType];
    const sectionCount = SECTION_COUNTS[brushType];
    assert.equal(geometry.brushType, brushType);
    assert.equal(geometry.positions.length, 2 * sectionCount * 3);
    assert.equal(geometry.colors.length, 2 * sectionCount * 4);
    assert.equal(geometry.progress.length, 2 * sectionCount);
    assert.equal(geometry.indices.length, sectionCount * 6);
    assert.equal(geometry.triangleCount, sectionCount * 2);
    assert.ok(geometry.indices instanceof Uint16Array);
    assert.ok(Array.from(geometry.positions).every(Number.isFinite));
    assert.deepEqual(Array.from(geometry.indices.slice(0, 6)), [0, sectionCount, 1, 1, sectionCount, sectionCount + 1]);
    assert.deepEqual(Array.from(geometry.colors.slice(0, 4)), [brushType / 4, 0.25, 0.5, 0.75]);
    assert.ok(Array.from(geometry.progress.slice(0, sectionCount)).every(
        (value) => Math.abs(value - brushType / 5) < 1e-6));
}

console.log("IMM web geometry: all five brush topologies passed exact buffer assertions");

function writePoint(pointIndex, x, y, z, brushType) {
    const offset = pointIndex * POINT_FLOATS;
    points.set([
        x, y, z,
        0, 1, 0,
        0, 0, -1,
        brushType / 4, 0.25, 0.5, 0.75,
        1,
    ], offset);
}
