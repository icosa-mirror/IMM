#include <TBE_AudioEngine.h>
#include <TBE_AudioObject.h>
#include "libImmCore/src/libSound/windows/memStream.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace TBE;

static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

// Four distinguishable half-second PCM sections let us check rendered samples,
// rather than trusting a successful play() result or the SDK's playback clock alone.
static std::vector<unsigned char> makeWav()
{
    const int sampleRate = 48000, samples = sampleRate * 2;
    std::vector<unsigned char> wav(44 + samples * 2);
    auto put = [&](size_t offset, uint32_t value, size_t size) { std::memcpy(wav.data() + offset, &value, size); };
    std::memcpy(wav.data(), "RIFF", 4);
    put(4, static_cast<uint32_t>(wav.size() - 8), 4);
    std::memcpy(wav.data() + 8, "WAVEfmt ", 8);
    put(16, 16, 4); put(20, 1, 2); put(22, 1, 2);
    put(24, sampleRate, 4); put(28, sampleRate * 2, 4);
    put(32, 2, 2); put(34, 16, 2);
    std::memcpy(wav.data() + 36, "data", 4); put(40, samples * 2, 4);
    const double markers[] = { 0.1, 0.3, 0.2, 0.4 };
    for (int i = 0; i < samples; ++i)
    {
        const auto sample = static_cast<int16_t>(32767 * markers[i / (sampleRate / 2)] * std::sin(i * 6.283185307179586 * 1000 / sampleRate));
        std::memcpy(wav.data() + 44 + i * 2, &sample, 2);
    }
    return wav;
}

int main(int argc, char** argv)
{
    if (argc == 3 && std::strcmp(argv[1], "--write-wav") == 0)
    {
        auto wav = makeWav();
        std::ofstream output(argv[2], std::ios::binary);
        output.write(reinterpret_cast<const char*>(wav.data()), wav.size());
        return output ? 0 : 1;
    }
    AudioEngine* engine = nullptr;
    try
    {
        EngineInitSettings settings{};
        settings.audioSettings.deviceType = AudioDeviceType::DISABLED;
        settings.audioSettings.sampleRate = 48000;
        settings.audioSettings.bufferSize = 960;
        settings.experimental.useFba = true;
        settings.experimental.fbaNumThreads = 0;
        require(TBE_CreateAudioEngine(engine, settings) == EngineError::OK, "create engine");
        auto wav = makeWav();
        if (argc == 2)
        {
            std::ifstream input(argv[1], std::ios::binary);
            require(static_cast<bool>(input), "open encoded marker fixture");
            wav.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        }
        int pendingCount = 0;
        auto seek = [&](AudioObject* sound, float ms)
        {
            const auto result = sound->seekToMs(ms);
            std::printf("seek %.0f ms: %d open=%d state=%d duration=%.3f elapsed=%.3f\n", ms, static_cast<int>(result), sound->isOpen(), static_cast<int>(sound->getPlayState()), sound->getAssetDurationInMs(), sound->getElapsedTimeInMs());
            require(result == EngineError::OK || result == EngineError::PENDING, "seek rejected");
            if (result == EngineError::PENDING) ++pendingCount;
        };
        auto mix = [&](int blocks)
        {
            float buffer[1920]{};
            for (int i = 0; i < blocks; ++i)
            {
                engine->update();
                require(engine->getAudioMix(buffer, 1920, 2) == EngineError::OK, "mix");
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            double squareSum = 0;
            for (float sample : buffer) squareSum += sample * sample;
            return std::sqrt(squareSum / 1920);
        };
        auto check = [&](AudioObject* sound, double expectedMs, double marker, const char* name, int blocks = 6)
        {
            const double rms = mix(blocks);
            const double elapsed = sound->getElapsedTimeInMs();
            std::printf("%s: elapsedMs=%.3f sampleRms=%.5f\n", name, elapsed, rms);
            require(std::abs(elapsed - expectedMs) < 60, "seek target not preserved in playback clock");
            // Sine RMS is amplitude / sqrt(2); flat mono to stereo adds another -3 dB.
            require(std::abs(rms - marker / 2) < 0.015, "rendered audio does not match seek target");
        };
        for (float initialMs : { 0.0f, 800.0f, 1900.0f })
        {
            AudioObject* sound = nullptr;
            require(engine->createAudioObject(sound) == EngineError::OK, "create sound");
            const auto openResult = argc == 3 && std::strcmp(argv[1], "--file") == 0
                ? sound->open(argv[2])
                : sound->open(new ImmCore::MemStream(wav.size(), wav.data()), true);
            require(openResult == EngineError::OK, "open marker audio");
            sound->shouldSpatialise(false);
            // No readiness wait between open, seek and play: matches initial plugin playback.
            if (initialMs == 1900) require(sound->enableLooping(true), "enable looping");
            seek(sound, initialMs);
            require(sound->play() == EngineError::OK, "initial play");
            check(sound, initialMs == 1900 ? 140 : initialMs + 120, initialMs == 800 ? 0.3 : 0.1,
                initialMs == 1900 ? "loop wraps from seek target" : "initial playback", initialMs == 1900 ? 12 : 6);

            require(sound->stop() == EngineError::OK, "stop for restart");
            seek(sound, 0); // The plugin's Stop() queues this reset.
            mix(2);
            const auto restartSeek = sound->seekToMs(0);
            std::printf("post-stop restart seek: %d\n", static_cast<int>(restartSeek));
            // Match the backend: play still runs if the SDK rejects the restart seek.
            require(sound->play() == EngineError::OK, "restart play");
            check(sound, 120, 0.1, "restart at zero offset");
            sound->close();
            engine->destroyAudioObject(sound);
        }
        require(pendingCount > 0, "test did not exercise PENDING seeks");
        TBE_DestroyAudioEngine(engine);
        std::printf("IMM_AUDIO360_PENDING PASS: initial playback, nonzero initial seek, looping and zero restart (%d pending seeks)\n", pendingCount);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "IMM_AUDIO360_PENDING FAIL: %s\n", error.what());
        if (engine) TBE_DestroyAudioEngine(engine);
        return 1;
    }
}
