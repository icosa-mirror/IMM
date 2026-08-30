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

assertDuplicateEndpointsRemainFinite();
assertStrokeMasksVisibilityAndProgress();
assertLargeDrawingUsesWideIndices();

console.log("IMM web geometry: brush, endpoint, mask, progress, and large-index assertions passed");

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

function assertDuplicateEndpointsRemainFinite() {
    const duplicatePoints = new Float32Array(4 * POINT_FLOATS);
    const duplicateTimes = new Float32Array([0, 0.25, 0.75, 1]);
    writeFixturePoint(duplicatePoints, 0, 0, 0, 0);
    writeFixturePoint(duplicatePoints, 1, 0, 0, 0);
    writeFixturePoint(duplicatePoints, 2, 0, 0, 2);
    writeFixturePoint(duplicatePoints, 3, 0, 0, 2);
    const [geometry] = packPaintGeometry({
        descriptors: new Uint32Array([0, 4, 2, 0]),
        points: duplicatePoints,
        pointTimes: duplicateTimes,
    });
    assert.ok(Array.from(geometry.positions).every(Number.isFinite));
    assert.ok(Array.from(geometry.directions).every(Number.isFinite));
    assert.equal(geometry.progress[0], 0);
    assert.equal(geometry.progress.at(-1), 1);
}

function assertStrokeMasksVisibilityAndProgress() {
    const fixturePoints = new Float32Array(4 * POINT_FLOATS);
    for (let index = 0; index < 4; index++) writeFixturePoint(fixturePoints, index, index, 0, 0);
    const [geometry] = packPaintGeometry({
        descriptors: new Uint32Array([
            0, 2, 0, 1,
            2, 2, 0, 2,
        ]),
        points: fixturePoints,
        pointTimes: new Float32Array([0, 0.4, 0.6, 1]),
    });
    assert.deepEqual(Array.from(geometry.visibility), [1, 1, 1, 1, 2, 2, 2, 2]);
    assert.deepEqual(Array.from(geometry.masks), [0, 0, 0, 0, 1, 1, 1, 1]);
    const expectedProgress = [0, 0, 0.4, 0.4, 0.6, 0.6, 1, 1];
    assert.ok(Array.from(geometry.progress).every(
        (value, index) => Math.abs(value - expectedProgress[index]) < 1e-6));
}

function assertLargeDrawingUsesWideIndices() {
    const pointCount = 32_768;
    const fixturePoints = new Float32Array(pointCount * POINT_FLOATS);
    for (let index = 0; index < pointCount; index++) {
        writeFixturePoint(fixturePoints, index, index * 0.001, 0, 0);
    }
    const [geometry] = packPaintGeometry({
        descriptors: new Uint32Array([0, pointCount, 0, 0]),
        points: fixturePoints,
        pointTimes: new Float32Array(pointCount),
    });
    assert.ok(geometry.indices instanceof Uint32Array);
    assert.equal(geometry.positions.length / 3, 65_536);
    assert.equal(Math.max(...geometry.indices.subarray(geometry.indices.length - 12)), 65_535);
}

function writeFixturePoint(target, pointIndex, x, y, z) {
    target.set([
        x, y, z,
        0, 1, 0,
        0, 0, -1,
        0.25, 0.5, 0.75, 0.8,
        1,
    ], pointIndex * POINT_FLOATS);
}
