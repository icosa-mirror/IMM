import { describe, expect, test } from "bun:test";
import {
    IMM_ATTENUATION_LINEAR,
    IMM_ATTENUATION_LOGARITHMIC,
    IMM_MODIFIER_CONE,
    IMM_MODIFIER_FRUSTUM,
    computeDirectionalGain,
    computeDistanceGain,
    computeAudioDriftSeconds,
    immAudioMimeType,
} from "../src/audio/imm-web-audio";
import {
    IMM_ASSET_OGG,
    IMM_ASSET_OPUS,
    IMM_ASSET_WAV,
    IMM_SOUND_POSITIONAL,
    type ImmSound,
    type ImmTransform,
} from "../src/format/imm-document";

const identity: ImmTransform = {
    rotation: [0, 0, 0, 1],
    scale: 1,
    flip: 0,
    translation: [0, 0, 0],
};

function sound(overrides: Partial<ImmSound> = {}): ImmSound {
    return {
        type: IMM_SOUND_POSITIONAL,
        assetFormat: IMM_ASSET_OPUS,
        channelCount: 2,
        looping: false,
        playOnLoad: true,
        gain: 1,
        attenuationType: 0,
        attenuationMin: 1,
        attenuationMax: 10,
        modifierType: 0,
        modifierParameters: [0, 0, 0, 0],
        bytes: new Uint8Array(),
        ...overrides,
    };
}

describe("IMM native audio contracts", () => {
    test("measures looping drift across buffer wrap boundaries", () => {
        expect(computeAudioDriftSeconds(60.012, 0.002, 60, true)).toBeCloseTo(0.01, 9);
        expect(computeAudioDriftSeconds(0.002, 59.992, 60, true)).toBeCloseTo(0.01, 9);
        expect(computeAudioDriftSeconds(60.012, 0.002, 60, false)).toBeCloseTo(60.01, 9);
    });

    test("maps encoded asset formats to their browser container and codec types", () => {
        expect(immAudioMimeType(IMM_ASSET_WAV)).toBe("audio/wav");
        expect(immAudioMimeType(IMM_ASSET_OGG)).toBe('audio/ogg; codecs="vorbis"');
        expect(immAudioMimeType(IMM_ASSET_OPUS)).toBe('audio/ogg; codecs="opus"');
        expect(immAudioMimeType(999)).toBeUndefined();
    });

    test("matches native linear distance attenuation and world scale", () => {
        const value = sound({ attenuationType: IMM_ATTENUATION_LINEAR });
        expect(computeDistanceGain(value, 1)).toBe(1);
        expect(computeDistanceGain(value, 5.5)).toBeCloseTo(0.5);
        expect(computeDistanceGain(value, 10)).toBe(0);
        expect(computeDistanceGain(value, 11, 2)).toBeCloseTo(0.5);
    });

    test("uses the native logarithmic factor and maximum-distance mute", () => {
        const value = sound({ attenuationType: IMM_ATTENUATION_LOGARITHMIC });
        const factor = 5 / Math.log2(10 / 1.001);
        expect(computeDistanceGain(value, 2)).toBeCloseTo((1 / 2) ** factor);
        expect(computeDistanceGain(value, 10)).toBe(0);
    });

    test("matches the native angular cone smoothstep including outside gain", () => {
        const value = sound({
            modifierType: IMM_MODIFIER_CONE,
            modifierParameters: [Math.PI / 6, Math.PI / 6, 0.2, 0],
        });
        expect(computeDirectionalGain(value, identity, [0, 0, -2])).toBe(1);
        expect(computeDirectionalGain(value, identity, [0, 0, 2])).toBeCloseTo(0.2);
        expect(computeDirectionalGain(value, identity, [2, 0, -2])).toBeCloseTo(0.6);
    });

    test("matches native rectangular-frustum projection and flipped forward axes", () => {
        const value = sound({
            modifierType: IMM_MODIFIER_FRUSTUM,
            modifierParameters: [Math.PI / 6, Math.PI / 6, Math.PI / 6, 0.1],
        });
        expect(computeDirectionalGain(value, identity, [0, 0, -2])).toBe(1);
        expect(computeDirectionalGain(value, identity, [0, 0, 2])).toBe(0);
        expect(computeDirectionalGain(value, { ...identity, flip: 3 }, [0, 0, 2])).toBe(1);
        expect(computeDirectionalGain(value, identity, [4, 0, -2])).toBeCloseTo(0.1);
    });
});
