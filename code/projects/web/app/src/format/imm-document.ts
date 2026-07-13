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

export interface ImmAnimationKey {
    property: number;
    interpolation: number;
    timeTicks: number;
    boolValue: boolean;
    uintValue: number;
    floatValue: number;
    doubleValue: number;
    transformValue: ImmTransform;
}

export interface ImmChapter {
    startTicks: number;
    endTicks: number;
    markerAction: number;
}

export interface ImmLayer {
    id: number;
    parentId: number;
    type: number;
    name: string;
    visible: boolean;
    isTimeline: boolean;
    opacity: number;
    defaultSpawn: boolean;
    localTransform: ImmTransform;
    worldTransform: ImmTransform;
    pivotTransform: ImmTransform;
    frameRate: number;
    frameCount: number;
    maxRepeatCount: number;
    durationTicks: number;
    keys: ImmAnimationKey[];
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
    ticksPerSecond: number;
    animateOnStart: boolean;
    durationTicks: number;
    chapters: ImmChapter[];
    layers: ImmLayer[];
    metrics: ImmDecodeMetrics;
}

export const IMM_LAYER_PAINT = 1;
export const IMM_LAYER_PICTURE = 4;

export const IMM_ANIM_VISIBILITY = 0;
export const IMM_ANIM_OPACITY = 1;
export const IMM_ANIM_DRAW_IN_TIME = 5;
export const IMM_ANIM_ACTION = 6;
export const IMM_ANIM_LOOP = 7;
export const IMM_ANIM_OFFSET = 8;
export const IMM_ANIM_TRANSFORM = 9;

export const IMM_INTERPOLATION_NONE = 0;
export const IMM_INTERPOLATION_LINEAR = 1;
export const IMM_INTERPOLATION_SMOOTHSTEP = 2;
export const IMM_INTERPOLATION_EASE_IN = 3;
export const IMM_INTERPOLATION_EASE_OUT = 4;

export const IMM_ACTION_STOP = 0;
export const IMM_ACTION_PLAY = 1;
export const IMM_ACTION_LOOP = 2;
export const IMM_ACTION_MAKE_DEFAULT = 3;

export const IMM_PICTURE_2D = 0;
export const IMM_PICTURE_EQUIRECT_MONO = 1;
export const IMM_PICTURE_EQUIRECT_STEREO = 2;
export const IMM_PICTURE_CUBEMAP_CROSS = 3;
export const IMM_PICTURE_CUBEMAP_VERTICAL = 4;
