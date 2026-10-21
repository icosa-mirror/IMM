#include "piWaveDecode.h"
#include "../piEncodedAudio.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <memory>
#include <new>
#include <codec.h>
#include <opus_multistream.h>

namespace ImmCore
{
namespace
{
    uint32_t U16(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8); }
    uint32_t U32(const uint8_t* p) { return U16(p) | (U16(p + 2) << 16); }
    uint64_t U64(const uint8_t* p) { return uint64_t(U32(p)) | (uint64_t(U32(p + 4)) << 32); }
    bool PageSize(const uint8_t* data, size_t size, size_t offset, size_t& header, size_t& body)
    {
        if (offset > size || size - offset < 27) return false;
        header = 27 + data[offset + 26]; body = 0;
        if (header > size - offset) return false;
        for (size_t i = 27; i < header; i++) body += data[offset + i];
        return body <= size - offset - header;
    }
    // Input has already passed bounded page/serial/sequence/CRC validation. Keep at most one packet copy.
    // A final empty EOS page still marks the last completed packet as end-of-stream.
    template<typename Callback> bool Packets(const uint8_t* data, size_t size, Callback callback)
    {
        size_t total = 0, header = 0, body = 0;
        uint64_t finalGranule = UINT64_MAX;
        for (size_t offset = 0; offset < size; offset += header + body)
        {
            if (!PageSize(data, size, offset, header, body)) return false;
            for (size_t i = 27; i < header; i++) if (data[offset + i] < 255) total++;
            if (data[offset + 5] & 4) finalGranule = U64(data + offset + 6);
        }
        size_t number = 0;
        std::vector<uint8_t> packet;
        for (size_t offset = 0; offset < size; offset += header + body)
        {
            if (!PageSize(data, size, offset, header, body)) return false;
            size_t lastComplete = 0;
            for (size_t i = 27; i < header; i++) if (data[offset + i] < 255) lastComplete = i;
            size_t cursor = offset + header;
            for (size_t i = 27; i < header; i++)
            {
                size_t length = data[offset + i];
                packet.insert(packet.end(), data + cursor, data + cursor + length); cursor += length;
                if (length == 255) continue;
                uint64_t granule = U64(data + offset + 6);
                bool hasGranule = i == lastComplete && granule != UINT64_MAX;
                bool eos = number + 1 == total;
                if (eos && finalGranule != UINT64_MAX) { granule = finalGranule; hasGranule = true; }
                if (!callback(packet, number++, granule, hasGranule, eos)) return false;
                packet.clear();
            }
        }
        return total != 0 && number == total && packet.empty();
    }
    int16_t S16(double value)
    {
        value = std::round(value * 32768.0);
        return static_cast<int16_t>(std::max(-32768.0, std::min(32767.0, value)));
    }
    AudioDecodeStatus Wav(const uint8_t* data, size_t size, size_t budget, DecodedAudioPcm16& output)
    {
        uint32_t codec = 0, bits = 0; size_t position = 0, bytes = 0;
        for (size_t cursor = 12; cursor < size;)
        {
            uint32_t length = U32(data + cursor + 4);
            if (!std::memcmp(data + cursor, "fmt ", 4)) { codec = U16(data + cursor + 8); bits = U16(data + cursor + 22); }
            if (!std::memcmp(data + cursor, "data", 4)) { position = cursor + 8; bytes = length; }
            cursor += 8 + size_t(length) + (length & 1);
        }
        size_t stride = bits / 8, count = bytes / stride;
        if (count > budget) return AudioDecodeStatus::ResourceLimit;
        output.samples.resize(count);
        for (size_t i = 0; i < count; i++)
        {
            const uint8_t* sample = data + position + i * stride;
            if (codec == 3)
            {
                uint32_t raw = U32(sample); float value; std::memcpy(&value, &raw, sizeof(value));
                if (!std::isfinite(value)) return AudioDecodeStatus::InvalidData;
                output.samples[i] = S16(value);
            }
            else if (bits == 8) output.samples[i] = static_cast<int16_t>((int(sample[0]) - 128) * 256);
            else
            {
                int64_t value = bits == 16 ? U16(sample) : bits == 24 ? (U16(sample) | (uint32_t(sample[2]) << 16)) : U32(sample);
                if (value & (int64_t(1) << (bits - 1))) value -= int64_t(1) << bits;
                output.samples[i] = bits == 16 ? static_cast<int16_t>(value) : S16(double(value) / double(uint64_t(1) << (bits - 1)));
            }
        }
        return AudioDecodeStatus::Ok;
    }
    struct VorbisOwner
    {
        vorbis_info info{}; vorbis_comment comment{}; vorbis_dsp_state dsp{}; vorbis_block block{};
        bool dspReady = false, blockReady = false;
        VorbisOwner() { vorbis_info_init(&info); vorbis_comment_init(&comment); }
        ~VorbisOwner() { if (blockReady) vorbis_block_clear(&block); if (dspReady) vorbis_dsp_clear(&dsp); vorbis_comment_clear(&comment); vorbis_info_clear(&info); }
    };
    AudioDecodeStatus Vorbis(const uint8_t* data, size_t size, size_t budget, DecodedAudioPcm16& output)
    {
        VorbisOwner state;
        AudioDecodeStatus status = AudioDecodeStatus::InvalidData;
        bool ok = Packets(data, size, [&](const std::vector<uint8_t>& bytes, size_t number, uint64_t granule, bool hasGranule, bool eos)
        {
            if (bytes.empty() || (hasGranule && granule > INT64_MAX)) return false;
            ogg_packet packet{}; packet.packet = const_cast<uint8_t*>(bytes.data()); packet.bytes = static_cast<long>(bytes.size());
            packet.b_o_s = number == 0; packet.e_o_s = eos; packet.granulepos = hasGranule ? static_cast<ogg_int64_t>(granule) : -1; packet.packetno = static_cast<ogg_int64_t>(number);
            if (number < 3)
            {
                if (vorbis_synthesis_headerin(&state.info, &state.comment, &packet) != 0) return false;
                if (number == 2)
                {
                    if (state.info.channels != output.channels || state.info.rate != output.rate || vorbis_synthesis_init(&state.dsp, &state.info) != 0) return false;
                    state.dspReady = true;
                    if (vorbis_block_init(&state.dsp, &state.block) != 0) return false;
                    state.blockReady = true;
                }
                return !eos;
            }
            if (!state.blockReady || vorbis_synthesis(&state.block, &packet) != 0 || vorbis_synthesis_blockin(&state.dsp, &state.block) != 0) return false;
            float** pcm = nullptr; int frames;
            while ((frames = vorbis_synthesis_pcmout(&state.dsp, &pcm)) > 0)
            {
                size_t count = size_t(frames) * output.channels, start = output.samples.size();
                if (count > budget - start) { status = AudioDecodeStatus::ResourceLimit; return false; }
                output.samples.resize(start + count);
                for (int frame = 0; frame < frames; frame++) for (int channel = 0; channel < output.channels; channel++)
                {
                    float value = pcm[channel][frame]; if (!std::isfinite(value)) return false;
                    output.samples[start + size_t(frame) * output.channels + channel] = S16(value);
                }
                if (vorbis_synthesis_read(&state.dsp, frames) != 0) return false;
            }
            return frames == 0;
        });
        return ok && state.blockReady ? AudioDecodeStatus::Ok : status;
    }
    bool Tags(const std::vector<uint8_t>& packet)
    {
        if (packet.size() < 16 || std::memcmp(packet.data(), "OpusTags", 8)) return false;
        size_t cursor = 12; uint32_t vendor = U32(packet.data() + 8);
        if (vendor > packet.size() - cursor) return false; cursor += vendor;
        if (packet.size() - cursor < 4) return false;
        uint32_t comments = U32(packet.data() + cursor); cursor += 4;
        if (comments > (packet.size() - cursor) / 4) return false;
        for (uint32_t i = 0; i < comments; i++)
        {
            if (packet.size() - cursor < 4) return false;
            uint32_t length = U32(packet.data() + cursor); cursor += 4;
            if (length > packet.size() - cursor) return false; cursor += length;
        }
        return true;
    }
    struct OpusCloser { void operator()(OpusMSDecoder* decoder) const { if (decoder) opus_multistream_decoder_destroy(decoder); } };
    AudioDecodeStatus Opus(const uint8_t* data, size_t size, size_t budget, DecodedAudioPcm16& output)
    {
        uint64_t rawFrames = 0, previousFrames = 0, previousGranule = 0, initialGranule = 0, end = 0;
        bool firstPage = true, ended = false;
        int skip = 0, gain = 0, streams = 1, coupled = output.channels == 2 ? 1 : 0;
        unsigned char mapping[4] = { 0, 1, 2, 3 };
        // RFC 7845 sections 4.2-4.5: initial granule offsets do not change pre-skip;
        // the final granule may trim the last decoded packet/page.
        bool valid = Packets(data, size, [&](const std::vector<uint8_t>& packet, size_t number, uint64_t granule, bool hasGranule, bool eos)
        {
            if (number == 0)
            {
                skip = U16(packet.data() + 10); gain = U16(packet.data() + 16); if (gain >= 32768) gain -= 65536;
                if (packet[18] != 0) { streams = packet[19]; coupled = packet[20]; std::copy(packet.begin() + 21, packet.begin() + 21 + output.channels, mapping); }
                return hasGranule && granule == 0 && !eos;
            }
            if (number == 1) return Tags(packet) && hasGranule && granule == 0 && !eos;
            if (packet.empty()) return false;
            int frames = opus_packet_get_nb_samples(packet.data(), static_cast<opus_int32>(packet.size()), 48000);
            if (frames <= 0 || frames > 5760) return false;
            rawFrames += frames;
            if (hasGranule)
            {
                if (granule > INT64_MAX) return false;
                if (firstPage)
                {
                    if (granule < rawFrames && !eos) return false;
                    initialGranule = granule >= rawFrames ? granule - rawFrames : 0;
                    firstPage = false;
                }
                else if (granule < previousGranule || granule > previousGranule + rawFrames - previousFrames || (!eos && granule != previousGranule + rawFrames - previousFrames)) return false;
                previousFrames = rawFrames; previousGranule = granule;
                if (eos) { if (granule < initialGranule + skip) return false; end = granule - initialGranule; ended = true; }
            }
            return true;
        });
        if (!valid || !ended || end <= uint64_t(skip) || end > rawFrames) return AudioDecodeStatus::InvalidData;
        uint64_t frames = end - skip;
        if (frames > budget / output.channels) return AudioDecodeStatus::ResourceLimit;
        int error = OPUS_OK;
        std::unique_ptr<OpusMSDecoder, OpusCloser> decoder(opus_multistream_decoder_create(48000, output.channels, streams, coupled, mapping, &error));
        if (!decoder) return error == OPUS_ALLOC_FAIL ? AudioDecodeStatus::OutOfMemory : AudioDecodeStatus::InvalidData;
        if (opus_multistream_decoder_ctl(decoder.get(), OPUS_SET_GAIN(gain)) != OPUS_OK) return AudioDecodeStatus::InvalidData;
        output.samples.resize(static_cast<size_t>(frames) * output.channels);
        std::vector<opus_int16> buffer(5760 * output.channels);
        uint64_t cursor = 0; size_t written = 0;
        valid = Packets(data, size, [&](const std::vector<uint8_t>& packet, size_t number, uint64_t, bool, bool)
        {
            if (number < 2) return true;
            int count = opus_multistream_decode(decoder.get(), packet.data(), static_cast<opus_int32>(packet.size()), buffer.data(), 5760, 0);
            if (count <= 0) return false;
            uint64_t first = std::max(cursor, uint64_t(skip)), last = std::min(cursor + count, end);
            if (last > first)
            {
                size_t samples = static_cast<size_t>(last - first) * output.channels;
                if (samples > output.samples.size() - written) return false;
                std::copy_n(buffer.data() + static_cast<size_t>(first - cursor) * output.channels, samples, output.samples.data() + written); written += samples;
            }
            cursor += count; return true;
        });
        return valid && written == output.samples.size() && cursor == rawFrames ? AudioDecodeStatus::Ok : AudioDecodeStatus::InvalidData;
    }
}
AudioDecodeStatus DecodeAudioPcm16(const uint8_t* data, size_t size, uint32_t format, int maxPcmBytes, DecodedAudioPcm16& output)
{
    output = {};
    if (!data || size > INT32_MAX || maxPcmBytes <= 0) return AudioDecodeStatus::InvalidData;
    if (format != 3 && format != 4 && format != 5) return AudioDecodeStatus::Unsupported;
    try
    {
        DecodedAudioPcm16 decoded;
        if (!InspectEncodedAudio(data, size, format, decoded.channels, decoded.rate)) return AudioDecodeStatus::InvalidData;
        size_t budget = static_cast<size_t>(maxPcmBytes) / sizeof(int16_t);
        auto status = format == 3 ? Wav(data, size, budget, decoded) : format == 4 ? Vorbis(data, size, budget, decoded) : Opus(data, size, budget, decoded);
        if (status != AudioDecodeStatus::Ok) return status;
        if (decoded.samples.empty() || decoded.samples.size() % decoded.channels != 0) return AudioDecodeStatus::InvalidData;
        output = std::move(decoded); return AudioDecodeStatus::Ok;
    }
    catch (const std::bad_alloc&) { return AudioDecodeStatus::OutOfMemory; }
}
}
