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
    assert.equal(geometry.directions.length, 2 * sectionCount * 3);
    assert.equal(geometry.visibility.length, 2 * sectionCount);
    assert.equal(geometry.masks.length, 2 * sectionCount);
    assert.equal(geometry.progress.length, 2 * sectionCount);
    assert.equal(geometry.indices.length, sectionCount * 6);
    assert.equal(geometry.triangleCount, sectionCount * 2);
    assert.ok(geometry.indices instanceof Uint16Array);
    assert.ok(Array.from(geometry.positions).every(Number.isFinite));
    assert.deepEqual(Array.from(geometry.indices.slice(0, 6)), [0, 1, sectionCount, 1, sectionCount + 1, sectionCount]);
    assert.deepEqual(Array.from(geometry.colors.slice(0, 4)), [brushType / 4, 0.25, 0.5, 0.75]);
    assert.deepEqual(Array.from(geometry.directions.slice(0, 3)), [0, 0, -1]);
    assert.ok(Array.from(geometry.visibility).every((value) => value === 0));
    assert.ok(Array.from(geometry.masks).every((value) => value === brushType));
    assert.ok(Array.from(geometry.progress.slice(0, sectionCount)).every(
        (value) => Math.abs(value - brushType / 5) < 1e-6));
    if (brushType >= 2) assertOutwardFacing(geometry);
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

function assertOutwardFacing(geometry) {
    const [ia, ib, ic] = geometry.indices;
    const a = position(geometry.positions, ia);
    const b = position(geometry.positions, ib);
    const c = position(geometry.positions, ic);
    const ab = b.map((value, index) => value - a[index]);
    const ac = c.map((value, index) => value - a[index]);
    const normal = [
        ab[1] * ac[2] - ab[2] * ac[1],
        ab[2] * ac[0] - ab[0] * ac[2],
        ab[0] * ac[1] - ab[1] * ac[0],
    ];
    const radial = [
        (a[0] + b[0] + c[0]) / 3,
        (a[1] + b[1] + c[1]) / 3,
        0,
    ];
    assert.ok(normal[0] * radial[0] + normal[1] * radial[1] > 0,
        `Brush ${geometry.brushType} generated inward-facing tube triangles`);
}

function position(positions, index) {
    return Array.from(positions.slice(index * 3, index * 3 + 3));
}
