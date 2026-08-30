export interface ImmTransform {
    rotation: [number, number, number, number];
    scale: number;
    flip: number;
    translation: [number, number, number];
}

export interface ImmDrawing {
    biggestStroke: number;
    strokeCount: number;
    pointCount: number;
    geometries: ImmPaintGeometry[];
}

export interface ImmPaintGeometry {
    brushType: number;
    triangleCount: number;
    positions: Float32Array;
    colors: Float32Array;
    directions: Float32Array;
    visibility: Uint8Array;
    masks: Uint8Array;
    progress: Float32Array;
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

export interface ImmModel {
    shadingModel: number;
    wireframe: boolean;
    positions: Float32Array;
    colors: Float32Array;
    normals: Float32Array;
    indices: Uint16Array | Uint32Array;
}

export interface ImmSound {
    type: number;
    assetFormat: number;
    channelCount: number;
    looping: boolean;
    playOnLoad: boolean;
    gain: number;
    attenuationType: number;
    attenuationMin: number;
    attenuationMax: number;
    modifierType: number;
    modifierParameters: number[];
    bytes: Uint8Array;
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

export interface ImmKeepAlive {
    type: number;
    waveform: number;
    parameters: number[];
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
    /** Native spawn tracking origin. Null for non-spawn layers. */
    spawnTracking?: "eye" | "floor" | null;
    localTransform: ImmTransform;
    worldTransform: ImmTransform;
    pivotTransform: ImmTransform;
    frameRate: number;
    frameCount: number;
    maxRepeatCount: number;
    durationTicks: number;
    keys: ImmAnimationKey[];
    keepAlive?: ImmKeepAlive;
    frameBuffer: Uint32Array;
    drawings: ImmDrawing[];
    model?: ImmModel;
    picture?: ImmPicture;
    sound?: ImmSound;
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

/** Version of the worker-to-main document contract consumed by this build. */
export const IMM_DOCUMENT_SCHEMA_VERSION = 5;

export const IMM_LAYER_PAINT = 1;
export const IMM_LAYER_MODEL = 3;
export const IMM_LAYER_PICTURE = 4;
export const IMM_LAYER_SOUND = 5;

export const IMM_SOUND_FLAT = 0;
export const IMM_SOUND_AMBISONIC = 1;
export const IMM_SOUND_POSITIONAL = 2;

export const IMM_ASSET_WAV = 3;
export const IMM_ASSET_OGG = 4;
export const IMM_ASSET_OPUS = 5;

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

export const IMM_KEEP_ALIVE_NONE = 0;
export const IMM_KEEP_ALIVE_WIGGLE = 1;
export const IMM_KEEP_ALIVE_BLINK = 2;

export const IMM_PICTURE_2D = 0;
export const IMM_PICTURE_EQUIRECT_MONO = 1;
export const IMM_PICTURE_EQUIRECT_STEREO = 2;
export const IMM_PICTURE_CUBEMAP_CROSS = 3;
export const IMM_PICTURE_CUBEMAP_VERTICAL = 4;
