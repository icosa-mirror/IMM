#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ImmCore
{
    enum class AudioDecodeStatus { Ok = 0, InvalidData = -1, ResourceLimit = -2, OutOfMemory = -3, Unsupported = -4 };
    struct DecodedAudioPcm16
    {
        int rate = 0, channels = 0;
        std::vector<int16_t> samples;
    };
    // Offline, synchronous decode. The limit bounds interleaved PCM bytes, not input/codec scratch memory.
    // Format IDs follow IMM assets: WAV=3, Vorbis=4, Opus=5. No playback state is used.
    AudioDecodeStatus DecodeAudioPcm16(const uint8_t* data, size_t size, uint32_t format, int maxPcmBytes, DecodedAudioPcm16& output);
}
