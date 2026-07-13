import * as THREE from "three";
import type { ImmDrawing } from "../format/imm-document";

const POINT_FLOATS = 14;
const SECTION_COUNTS = [2, 2, 7, 7, 4] as const;
const EPSILON = 1e-7;
type Vec3 = [number, number, number];

export interface PaintGeometryResult {
    geometry: THREE.BufferGeometry;
    brushType: number;
    triangleCount: number;
}

interface StrokeGeometrySize {
    strokeIndex: number;
    pointOffset: number;
    pointCount: number;
    sectionCount: number;
    vertexOffset: number;
    indexOffset: number;
}

export function buildPaintGeometries(drawing: ImmDrawing): PaintGeometryResult[] {
    const results: PaintGeometryResult[] = [];
    for (let brushType = 0; brushType < SECTION_COUNTS.length; brushType++) {
        const strokes: StrokeGeometrySize[] = [];
        let vertexCount = 0;
        let indexCount = 0;
        for (let strokeIndex = 0; strokeIndex < drawing.descriptors.length / 4; strokeIndex++) {
            const descriptorOffset = strokeIndex * 4;
            if (drawing.descriptors[descriptorOffset + 2] !== brushType) continue;
            const pointOffset = drawing.descriptors[descriptorOffset] ?? 0;
            const pointCount = drawing.descriptors[descriptorOffset + 1] ?? 0;
            if (pointCount < 2) continue;
            const sectionCount = SECTION_COUNTS[brushType] ?? 2;
            strokes.push({
                strokeIndex,
                pointOffset,
                pointCount,
                sectionCount,
                vertexOffset: vertexCount,
                indexOffset: indexCount,
            });
            vertexCount += pointCount * sectionCount;
            indexCount += (pointCount - 1) * sectionCount * 6;
        }
        if (vertexCount === 0) continue;

        const positions = new Float32Array(vertexCount * 3);
        const colors = new Float32Array(vertexCount * 4);
        const indices = vertexCount > 65_535
            ? new Uint32Array(indexCount)
            : new Uint16Array(indexCount);
        for (const stroke of strokes) {
            buildStroke(drawing, stroke, brushType, positions, colors, indices);
        }

        const geometry = new THREE.BufferGeometry();
        geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
        geometry.setAttribute("color", new THREE.BufferAttribute(colors, 4));
        geometry.setIndex(new THREE.BufferAttribute(indices, 1));
        geometry.computeBoundingBox();
        geometry.computeBoundingSphere();
        results.push({ geometry, brushType, triangleCount: indexCount / 3 });
    }
    return results;
}

function buildStroke(
    drawing: ImmDrawing,
    stroke: StrokeGeometrySize,
    brushType: number,
    positions: Float32Array,
    colors: Float32Array,
    indices: Uint16Array | Uint32Array,
): void {
    const tangent: Vec3 = [0, 0, 0];
    const basisU: Vec3 = [0, 0, 0];
    const basisV: Vec3 = [0, 0, 0];
    for (let pointIndex = 0; pointIndex < stroke.pointCount; pointIndex++) {
        computeBasis(drawing.points, stroke.pointOffset, stroke.pointCount, pointIndex, tangent, basisU, basisV);
        const sourceOffset = (stroke.pointOffset + pointIndex) * POINT_FLOATS;
        const px = read(drawing.points, sourceOffset);
        const py = read(drawing.points, sourceOffset + 1);
        const pz = read(drawing.points, sourceOffset + 2);
        const width = read(drawing.points, sourceOffset + 13);
        for (let sectionIndex = 0; sectionIndex < stroke.sectionCount; sectionIndex++) {
            const [sectionX, sectionY] = sectionPosition(brushType, sectionIndex, stroke.sectionCount);
            const vertexIndex = stroke.vertexOffset + pointIndex * stroke.sectionCount + sectionIndex;
            const positionOffset = vertexIndex * 3;
            positions[positionOffset] = px + width * (basisU[0] * sectionX + basisV[0] * sectionY);
            positions[positionOffset + 1] = py + width * (basisU[1] * sectionX + basisV[1] * sectionY);
            positions[positionOffset + 2] = pz + width * (basisU[2] * sectionX + basisV[2] * sectionY);
            const colorOffset = vertexIndex * 4;
            colors[colorOffset] = read(drawing.points, sourceOffset + 9);
            colors[colorOffset + 1] = read(drawing.points, sourceOffset + 10);
            colors[colorOffset + 2] = read(drawing.points, sourceOffset + 11);
            colors[colorOffset + 3] = read(drawing.points, sourceOffset + 12);
        }
    }

    let target = stroke.indexOffset;
    for (let pointIndex = 0; pointIndex < stroke.pointCount - 1; pointIndex++) {
        for (let sectionIndex = 0; sectionIndex < stroke.sectionCount; sectionIndex++) {
            const nextSection = (sectionIndex + 1) % stroke.sectionCount;
            const current = stroke.vertexOffset + pointIndex * stroke.sectionCount;
            const next = current + stroke.sectionCount;
            indices[target++] = current + sectionIndex;
            indices[target++] = next + sectionIndex;
            indices[target++] = current + nextSection;
            indices[target++] = current + nextSection;
            indices[target++] = next + sectionIndex;
            indices[target++] = next + nextSection;
        }
    }
}

function sectionPosition(brushType: number, index: number, count: number): [number, number] {
    if (brushType <= 1) return index === 0 ? [-1, 0] : [1, 0];
    if (brushType === 4) {
        return [[-1, -1], [1, -1], [1, 1], [-1, 1]][index] as [number, number];
    }
    const angle = Math.PI * 2 * index / count;
    return [Math.cos(angle), Math.sin(angle) * (brushType === 3 ? 0.3 : 1)];
}

function computeBasis(
    points: Float32Array,
    strokeOffset: number,
    pointCount: number,
    pointIndex: number,
    tangent: Vec3,
    basisU: Vec3,
    basisV: Vec3,
): void {
    tangent[0] = tangent[1] = tangent[2] = 0;
    addFirstDirection(points, strokeOffset, pointCount, pointIndex, 1, tangent);
    addFirstDirection(points, strokeOffset, pointCount, pointIndex, -1, tangent);
    if (!normalize(tangent)) {
        const first = strokeOffset * POINT_FLOATS;
        const last = (strokeOffset + pointCount - 1) * POINT_FLOATS;
        tangent[0] = read(points, last) - read(points, first) + 0.000001;
        tangent[1] = read(points, last + 1) - read(points, first + 1) + 0.000002;
        tangent[2] = read(points, last + 2) - read(points, first + 2) + 0.000003;
        normalize(tangent);
    }

    const pointOffset = (strokeOffset + pointIndex) * POINT_FLOATS;
    const nx = read(points, pointOffset + 3);
    const ny = read(points, pointOffset + 4);
    const nz = read(points, pointOffset + 5);
    basisU[0] = ny * tangent[2] - nz * tangent[1];
    basisU[1] = nz * tangent[0] - nx * tangent[2];
    basisU[2] = nx * tangent[1] - ny * tangent[0];
    if (!normalize(basisU)) {
        if (Math.abs(tangent[0]) < 0.9) {
            basisU[0] = 0; basisU[1] = tangent[2]; basisU[2] = tangent[1];
        } else if (Math.abs(tangent[1]) < 0.9) {
            basisU[0] = -tangent[2]; basisU[1] = 0; basisU[2] = tangent[0];
        } else {
            basisU[0] = tangent[1]; basisU[1] = -tangent[0]; basisU[2] = 0;
        }
        normalize(basisU);
    }
    basisV[0] = tangent[1] * basisU[2] - tangent[2] * basisU[1];
    basisV[1] = tangent[2] * basisU[0] - tangent[0] * basisU[2];
    basisV[2] = tangent[0] * basisU[1] - tangent[1] * basisU[0];
    normalize(basisV);
}

function addFirstDirection(
    points: Float32Array,
    strokeOffset: number,
    pointCount: number,
    pointIndex: number,
    step: -1 | 1,
    target: Vec3,
): void {
    const source = (strokeOffset + pointIndex) * POINT_FLOATS;
    for (let candidate = pointIndex + step; candidate >= 0 && candidate < pointCount; candidate += step) {
        const offset = (strokeOffset + candidate) * POINT_FLOATS;
        const direction: Vec3 = step === 1
            ? [read(points, offset) - read(points, source), read(points, offset + 1) - read(points, source + 1), read(points, offset + 2) - read(points, source + 2)]
            : [read(points, source) - read(points, offset), read(points, source + 1) - read(points, offset + 1), read(points, source + 2) - read(points, offset + 2)];
        if (normalize(direction)) {
            target[0] += direction[0];
            target[1] += direction[1];
            target[2] += direction[2];
            return;
        }
    }
}

function normalize(value: Vec3): boolean {
    const length = Math.hypot(value[0], value[1], value[2]);
    if (length < EPSILON || !Number.isFinite(length)) return false;
    value[0] /= length;
    value[1] /= length;
    value[2] /= length;
    return true;
}

function read(values: Float32Array, index: number): number {
    return values[index] ?? 0;
}
