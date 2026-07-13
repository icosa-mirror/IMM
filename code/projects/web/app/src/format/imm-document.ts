export interface ImmTransform {
    rotation: [number, number, number, number];
    scale: number;
    flip: number;
    translation: [number, number, number];
}

export interface ImmDrawing {
    biggestStroke: number;
    descriptors: Uint32Array;
    bounds: Float32Array;
    points: Float32Array;
    geometries: ImmPaintGeometry[];
}

export interface ImmPaintGeometry {
    brushType: number;
    triangleCount: number;
    positions: Float32Array;
    colors: Float32Array;
    indices: Uint16Array | Uint32Array;
}

export interface ImmPicture {
    contentType: number;
    viewerLocked: boolean;
    width: number;
    height: number;
    hasAlpha: boolean;
    pixels: Uint8Array;
}

export interface ImmLayer {
    id: number;
    type: number;
    name: string;
    visible: boolean;
    opacity: number;
    defaultSpawn: boolean;
    localTransform: ImmTransform;
    worldTransform: ImmTransform;
    pivotTransform: ImmTransform;
    frameRate: number;
    frameCount: number;
    maxRepeatCount: number;
    frameBuffer: Uint32Array;
    drawings: ImmDrawing[];
    picture?: ImmPicture;
}

export interface ImmDecodeMetrics {
    decodeMs: number;
    marshalMs: number;
    packMs: number;
}

export interface ImmDocument {
    schemaVersion: number;
    backgroundColor: [number, number, number];
    layers: ImmLayer[];
    metrics: ImmDecodeMetrics;
}

export const IMM_LAYER_PAINT = 1;
export const IMM_LAYER_PICTURE = 4;

export const IMM_PICTURE_2D = 0;
export const IMM_PICTURE_EQUIRECT_MONO = 1;
