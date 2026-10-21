#pragma once

#include <cstdint>
#include <cstring>

namespace ImmCore
{
    namespace EncodedAudioDetails
    {
        struct OggCrcTable
        {
            uint32_t values[256]{};
            constexpr OggCrcTable()
            {
                for (uint32_t i = 0; i < 256; i++)
                {
                    uint32_t value = i << 24;
                    for (int bit = 0; bit < 8; bit++) value = (value << 1) ^ ((value & 0x80000000u) ? 0x04c11db7u : 0u);
                    values[i] = value;
                }
            }
        };
        inline bool VerifyOggChecksum(const uint8_t* page, uint64_t headerBytes, uint64_t bodyBytes)
        {
            if (!page || headerBytes < 27 || headerBytes > 282 || bodyBytes > 65025) return false;
            static constexpr OggCrcTable table{};
            uint32_t crc = 0;
            for (uint64_t i = 0; i < headerBytes + bodyBytes; i++)
            {
                const uint8_t byte = i >= 22 && i < 26 ? 0 : page[i];
                crc = (crc << 8) ^ table.values[(crc >> 24) ^ byte];
            }
            const uint32_t expected = uint32_t(page[22]) | (uint32_t(page[23]) << 8) | (uint32_t(page[24]) << 16) | (uint32_t(page[25]) << 24);
            return crc == expected;
        }
    }
    // Inspect bounded WAV or single-stream Ogg framing without decoding/re-encoding audio packets.
    // Format values match IMM AssetFormat: WAV=3, Vorbis=4, Opus=5.
    inline bool InspectEncodedAudio(const uint8_t* data, uint64_t size, uint32_t format, int& channels, int& rate)
    {
        channels = rate = 0;
        if (!data) return false;
        const auto u16 = [](const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8); };
        const auto u32 = [](const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); };
        if (format == 3)
        {
            if (size < 12 || std::memcmp(data, "RIFF", 4) || std::memcmp(data + 8, "WAVE", 4)) return false;
            const uint64_t end = uint64_t(u32(data + 4)) + 8;
            if (end != size) return false;
            uint64_t cursor = 12, sampleBytes = 0;
            uint32_t alignment = 0;
            bool foundFormat = false, foundData = false;
            while (cursor < end)
            {
                if (end - cursor < 8) return false;
                const uint8_t* chunk = data + cursor;
                const uint32_t length = u32(chunk + 4);
                cursor += 8;
                if (length > end - cursor) return false;
                if (!std::memcmp(chunk, "fmt ", 4))
                {
                    if (foundFormat || length < 16) return false;
                    const uint8_t* header = data + cursor;
                    const uint32_t codec = u16(header), bits = u16(header + 14);
                    if ((codec != 1 && codec != 3) || bits == 0 || bits > 32 || bits % 8 != 0 ||
                        (codec == 3 && bits != 32)) return false;
                    channels = static_cast<int>(u16(header + 2));
                    const uint32_t sampleRate = u32(header + 4);
                    if (sampleRate > 192000) return false;
                    rate = static_cast<int>(sampleRate);
                    alignment = u16(header + 12);
                    if (alignment != uint32_t(channels) * (bits / 8) || u32(header + 8) != sampleRate * alignment) return false;
                    foundFormat = true;
                }
                else if (!std::memcmp(chunk, "data", 4))
                {
                    if (foundData) return false;
                    sampleBytes = length; foundData = true;
                }
                const uint64_t padded = uint64_t(length) + (length & 1);
                if (padded > end - cursor) return false;
                cursor += padded;
            }
            if (!foundFormat || !foundData || !alignment || !sampleBytes || sampleBytes % alignment) return false;
        }
        else if (format == 4 || format == 5)
        {
            uint64_t cursor = 0;
            uint32_t serial = 0, sequence = 0;
            bool first = true, ended = false, continued = false;
            while (cursor < size)
            {
                if (ended || size - cursor < 27) return false;
                const uint8_t* page = data + cursor;
                if (std::memcmp(page, "OggS", 4) || page[4] != 0 || (page[5] & ~7)) return false;
                const uint32_t segments = page[26];
                const uint64_t headerBytes = 27 + segments;
                if (headerBytes > size - cursor) return false;
                uint64_t bodyBytes = 0;
                for (uint32_t i = 0; i < segments; i++) bodyBytes += page[27 + i];
                if (bodyBytes > size - cursor - headerBytes) return false;
                if (!EncodedAudioDetails::VerifyOggChecksum(page, headerBytes, bodyBytes)) return false;
                if (first)
                {
                    if (!(page[5] & 2) || (page[5] & 1) || segments == 0 || page[27] == 255) return false;
                    serial = u32(page + 14); sequence = u32(page + 18);
                    const uint8_t* identification = page + headerBytes;
                    if (format == 5)
                    {
                        if (page[27] < 19 || std::memcmp(identification, "OpusHead", 8) || identification[8] != 1) return false;
                        channels = identification[9]; rate = 48000;
                        const uint8_t mapping = identification[18];
                        if (mapping == 0)
                        {
                            if (channels > 2 || page[27] != 19) return false;
                        }
                        else
                        {
                            if ((mapping != 1 && mapping != 2) || page[27] != 21 + channels) return false;
                            const uint32_t streams = identification[19], coupled = identification[20];
                            if (!streams || coupled > streams || streams + coupled > 255) return false;
                            for (int i = 0; i < channels; i++)
                                if (identification[21 + i] != 255 && identification[21 + i] >= streams + coupled) return false;
                        }
                    }
                    else
                    {
                        if (page[27] < 30 || identification[0] != 1 || std::memcmp(identification + 1, "vorbis", 6) || u32(identification + 7) != 0) return false;
                        channels = identification[11];
                        const uint8_t small = identification[28] & 15, large = identification[28] >> 4;
                        if (small < 6 || large > 13 || large < small || identification[29] != 1) return false;
                        const uint32_t sampleRate = u32(identification + 12);
                        if (sampleRate > 192000) return false;
                        rate = static_cast<int>(sampleRate);
                    }
                    first = false;
                }
                else if ((page[5] & 2) || ((page[5] & 1) != 0) != continued || u32(page + 14) != serial || u32(page + 18) != ++sequence) return false;
                if (segments) continued = page[26 + segments] == 255;
                ended = (page[5] & 4) != 0;
                if (ended && continued) return false;
                cursor += headerBytes + bodyBytes;
            }
            if (first || !ended) return false;
        }
        else return false;
        return rate >= 8000 && rate <= 192000 && (channels == 1 || channels == 2 || channels == 4);
    }
}
