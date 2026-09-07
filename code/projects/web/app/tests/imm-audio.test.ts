import { describe, expect, test } from "bun:test";
import {
    ImmWebAudio,
    IMM_ATTENUATION_LINEAR,
    IMM_ATTENUATION_LOGARITHMIC,
    IMM_MODIFIER_CONE,
    IMM_MODIFIER_FRUSTUM,
    computeDirectionalGain,
    computeDistanceGain,
    computeAudioDriftSeconds,
    audioContextTimelineDelta,
    immAudioMimeType,
} from "../src/audio/imm-web-audio";
import {
    IMM_ASSET_OGG,
    IMM_ASSET_OPUS,
    IMM_ASSET_WAV,
    IMM_SOUND_POSITIONAL,
    IMM_SOUND_FLAT,
    type ImmSound,
    type ImmTransform,
    type ImmDocument,
} from "../src/format/imm-document";
import type { ImmPlaybackSnapshot } from "../src/runtime/imm-playback";

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

function stagedAudioFixture(decode: (bytes: ArrayBuffer) => Promise<AudioBuffer> = async () => ({} as AudioBuffer)) {
    const document = { layers: [{ id: 1, name: "first", type: 5, sound: sound({ bytes: new Uint8Array([1]) }) }] } as unknown as ImmDocument;
    let decodes = 0, closes = 0, starts = 0, stops = 0;
    const sourceOffsets: number[] = [];
    const context = {
        state: "suspended", currentTime: 0, destination: {}, listener: {},
        createGain: () => ({ gain: { cancelScheduledValues() {}, setValueAtTime() {} }, connect() {}, disconnect() {} }),
        decodeAudioData: (bytes: ArrayBuffer) => { decodes++; return decode(bytes); },
        close: async () => { closes++; },
        resume: async () => { context.state = "running"; },
        suspend: async () => { context.state = "suspended"; },
        createBufferSource: () => ({ connect() {}, disconnect() {}, addEventListener() {},
            start(_when: number, offset: number) { starts++; sourceOffsets.push(offset); }, stop() { stops++; } }),
    };
    const audio = new ImmWebAudio(document, { context: context as unknown as AudioContext });
    return { document, audio, context, sourceOffsets, counts: () => ({ decodes, closes }), transportCounts: () => ({ starts, stops }) };
}

describe("staged audio residency", () => {
    test("arrival of another sound does not restart a sound already playing", async () => {
        const { document, audio, transportCounts } = stagedAudioFixture(async () => ({ duration: 10 } as AudioBuffer));
        document.ticksPerSecond = 100;
        document.layers[0]!.sound!.type = IMM_SOUND_FLAT;
        document.layers[0]!.keys = [];
        await audio.prepare();
        const snapshot = { layers: new Map([[1, { layer: document.layers[0], visible: true,
            opacity: 1, localTimeTicks: 0, worldTransform: identity }]]) } as unknown as ImmPlaybackSnapshot;
        audio.update(snapshot, identity);
        expect(transportCounts()).toEqual({ starts: 1, stops: 0 });
        document.layers.push({ ...document.layers[0]!, id: 2,
            sound: sound({ type: IMM_SOUND_FLAT, bytes: new Uint8Array([2]) }) });
        snapshot.layers.set(2, { ...snapshot.layers.get(1)!, layer: document.layers[1]! });
        await audio.refreshLayer(2);
        expect(transportCounts()).toEqual({ starts: 1, stops: 0 });
        audio.update(snapshot, identity);
        expect(transportCounts()).toEqual({ starts: 2, stops: 0 });
        await audio.dispose();
        expect(transportCounts()).toEqual({ starts: 2, stops: 2 });
    });

    test("a decode finishing between frames starts at the next snapshot without restarting resident audio", async () => {
        let finishSecond!: (buffer: AudioBuffer) => void;
        const { document, audio, context, sourceOffsets, transportCounts } = stagedAudioFixture(async bytes => {
            if (new Uint8Array(bytes)[0] === 2) return new Promise(resolve => { finishSecond = resolve; });
            return { duration: 10 } as AudioBuffer;
        });
        document.ticksPerSecond = 100;
        document.layers[0]!.sound!.type = IMM_SOUND_FLAT;
        document.layers[0]!.keys = [];
        document.layers.push({ ...document.layers[0]!, id: 2,
            sound: sound({ type: IMM_SOUND_FLAT, bytes: new Uint8Array([2]) }) });
        const preparation = audio.prepare();
        await audio.refreshLayer(1);
        await audio.setTransportPlaying(true);
        const snapshot = { layers: new Map(document.layers.map(layer => [layer.id, {
            layer, visible: true, opacity: 1, localTimeTicks: 100, worldTransform: identity,
        }])) } as unknown as ImmPlaybackSnapshot;
        audio.update(snapshot, identity);
        expect(sourceOffsets).toEqual([1]);

        // Audio advances while the next sound decodes and the last visual snapshot remains at 1s.
        context.currentTime = 0.5;
        finishSecond({ duration: 10 } as AudioBuffer);
        await preparation;
        expect(transportCounts()).toEqual({ starts: 1, stops: 0 });
        expect(audio.timelineDeltaSeconds(0.1)).toBe(0.5);
        for (const state of snapshot.layers.values()) state.localTimeTicks = 150;
        audio.update(snapshot, identity);
        expect(sourceOffsets).toEqual([1, 1.5]);
        expect(transportCounts()).toEqual({ starts: 2, stops: 0 });

        context.currentTime = 0.75;
        expect(audio.timelineDeltaSeconds(0.1)).toBe(0.25);
        for (const state of snapshot.layers.values()) state.localTimeTicks = 175;
        audio.update(snapshot, identity);
        expect(audio.diagnostics.currentDrift).toHaveLength(2);
        expect(audio.diagnostics.maximumAbsoluteDriftSeconds).toBeCloseTo(0, 9);
        await audio.dispose();
    });

    test("new audio preserves decoded sounds, mute state and the existing context", async () => {
        const { document, audio, counts } = stagedAudioFixture();
        await audio.prepare();
        audio.setMuted(true);
        document.layers.push({ ...document.layers[0]!, id: 2, sound: sound({ bytes: new Uint8Array([2]) }) });
        await audio.refreshLayer(2);
        await audio.refreshLayer(1);
        expect(counts()).toEqual({ decodes: 2, closes: 0 });
        expect(audio.diagnostics.decodedSounds).toBe(2);
        expect(audio.diagnostics.muted).toBe(true);
        await audio.dispose();
        expect(counts().closes).toBe(1);
        expect(audio.diagnostics.decodedSounds).toBe(0);
    });

    test("an arrival concurrent with initial preparation shares the pending decode", async () => {
        let complete!: (buffer: AudioBuffer) => void;
        const { audio, counts } = stagedAudioFixture(() => new Promise(resolve => { complete = resolve; }));
        const preparation = audio.prepare();
        const refresh = audio.refreshLayer(1);
        expect(counts().decodes).toBe(1);
        complete({} as AudioBuffer);
        await Promise.all([preparation, refresh]);
        expect(audio.diagnostics.decodedSounds).toBe(1);
        await audio.dispose();
    });

    test("a decode completing after disposal cannot repopulate audio residency", async () => {
        let complete!: (buffer: AudioBuffer) => void;
        const { audio } = stagedAudioFixture(() => new Promise(resolve => { complete = resolve; }));
        const preparation = audio.prepare();
        await audio.dispose();
        complete({} as AudioBuffer);
        await preparation;
        expect(audio.diagnostics.decodedSounds).toBe(0);
    });

    test("a failed arrival leaves resident sounds available and reports the failure", async () => {
        const { document, audio } = stagedAudioFixture(async bytes => {
            if (new Uint8Array(bytes)[0] === 2) throw new Error("Unsupported audio");
            return {} as AudioBuffer;
        });
        await audio.prepare();
        document.layers.push({ ...document.layers[0]!, id: 2, sound: sound({ bytes: new Uint8Array([2]) }) });
        await audio.refreshLayer(2);
        expect(audio.diagnostics.decodedSounds).toBe(1);
        expect(audio.diagnostics.decodeFailures.map(failure => failure.layerId)).toEqual([2]);
        await audio.dispose();
    });
});

describe("IMM native audio contracts", () => {
    test("measures looping drift across buffer wrap boundaries", () => {
        expect(computeAudioDriftSeconds(60.012, 0.002, 60, true)).toBeCloseTo(0.01, 9);
        expect(computeAudioDriftSeconds(0.002, 59.992, 60, true)).toBeCloseTo(0.01, 9);
        expect(computeAudioDriftSeconds(60.012, 0.002, 60, false)).toBeCloseTo(60.01, 9);
    });

    test("does not cap audio-clock advancement after a delayed render frame", () => {
        expect(audioContextTimelineDelta(12.75, 10.25, 0.1)).toBe(2.5);
        expect(audioContextTimelineDelta(10, null, 0.016)).toBe(0.016);
        expect(audioContextTimelineDelta(9, 10, 0.016)).toBe(0);
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
