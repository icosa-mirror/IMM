#include "libImmCore/src/libSound/windows/piSoundEngineAudioSDKBackend.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace ImmCore;

static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    piSoundEngineBackend* backend = nullptr;
    piLog log;
    std::atomic<int> errors{0};
    log.SetCallback([](void* context, int severity, const wchar_t* message) {
        if (severity == 1)
        {
            ++*static_cast<std::atomic<int>*>(context);
            std::fwprintf(stderr, L"IMM_AUDIO360_BACKEND error: %ls\n", message);
        }
    }, &errors);
    try
    {
        require(argc == 2 || argc == 3, "provide marker file and optional buffer size");
        std::ifstream input(argv[1], std::ios::binary);
        require(static_cast<bool>(input), "open marker audio");
        std::vector<unsigned char> audio(std::istreambuf_iterator<char>(input), {});
        backend = piSoundEngineAudioSDKBackend::Create(&log);
        require(backend != nullptr, "create backend");
        piSoundEngineAudioSDKBackend::setDisableAudioDevice(backend, true);
        piSoundEngineBackend::Configuration config;
        config.mBufferSize = argc == 3 ? std::atoi(argv[2]) : 960;
        require(config.mBufferSize == 480 || config.mBufferSize == 960 || config.mBufferSize == 1920, "supported test buffer size");
        require(backend->Init(nullptr, -1, &config), "initialize backend");
        auto* engine = backend->GetEngine();
        auto mix = [&](int blocks)
        {
            std::vector<float> buffer(config.mBufferSize * 2);
            const int calls = (blocks * 960 + config.mBufferSize - 1) / config.mBufferSize;
            for (int i = 0; i < calls; ++i)
            {
                backend->Tick();
                piSoundEngineAudioSDKBackend::getAudioMix(backend, buffer.data(), config.mBufferSize);
                std::this_thread::sleep_for(std::chrono::milliseconds(config.mBufferSize / 48));
            }
            double squareSum = 0;
            for (float sample : buffer) squareSum += sample * sample;
            return std::sqrt(squareSum / buffer.size());
        };
        auto check = [&](double amplitude, const char* message, int blocks = 6)
        {
            const double rms = mix(blocks);
            std::printf("%s: sampleRms=%.5f expected=%.5f\n", message, rms, amplitude / 2);
            require(std::abs(rms - amplitude / 2) < 0.015, message);
        };
        for (bool looping : { false, true })
        {
            const int id = engine->AddSound(1, audio.size(), audio.data(), false);
            require(id >= 0, "add memory-backed marker sound");
            engine->SetLooping(id, looping);
            require(engine->Play(id, looping ? 1900000 : 800000), "initial play");
            check(looping ? 0.1 : 0.3, "initial/loop seek", looping ? 12 : 6);
            for (int repetition = 0; repetition < 2; ++repetition)
            {
                for (int offsetMs : { 1300, 800, 1600, 0 })
                {
                    require(engine->Stop(id), "stop before restart");
                    require(!engine->Resume(id), "Resume must not resume a pending stop");
                    if (repetition == 0)
                    {
                        require(mix(3) < 0.0001, "stopped sound must be silent");
                        require(engine->GetPlaybackState(id) == piSoundEngine::PlaybackState::Stopped, "logical stopped state");
                    }
                    require(!engine->Resume(id), "Resume must not resume a stopped sound");
                    require(engine->Play(id, static_cast<uint64_t>(offsetMs) * 1000), "restart play");
                    check(offsetMs == 1300 ? 0.2 : offsetMs == 800 ? 0.3 : offsetMs == 1600 ? 0.4 : 0.1,
                        "repeated restart target");
                    require(engine->GetPlaybackState(id) == piSoundEngine::PlaybackState::Playing, "playing after restart");
                }
            }
            require(engine->Pause(id), "regular pause");
            require(mix(3) < 0.0001, "paused sound must be silent");
            require(engine->GetPlaybackState(id) == piSoundEngine::PlaybackState::Paused, "regular paused state");
            require(engine->Resume(id), "resume acknowledged pause");
            check(0.1, "resume retains source marker", 2);
            require(engine->Pause(id), "rapid pause request");
            if (!engine->Resume(id))
            {
                // A reversal during an asynchronous pause must become possible
                // once that pause is acknowledged; it must not need Play/seek.
                for (int attempt = 0; attempt < 10 && engine->GetPlaybackState(id) != piSoundEngine::PlaybackState::Paused; ++attempt)
                    mix(1);
                require(engine->Resume(id), "resume after pending pause completes");
            }
            check(0.1, "pause-resume reversal retains source marker", 2);
            require(engine->GetPlaybackState(id) == piSoundEngine::PlaybackState::Playing, "playing after pause-resume reversal");
            require(engine->Pause(id), "pause after reversal");
            require(mix(3) < 0.0001, "pause after reversal is silent");
            require(engine->GetPlaybackState(id) == piSoundEngine::PlaybackState::Paused, "paused after reversal");
            require(engine->Stop(id), "stop an already paused sound");
            mix(3);
            require(engine->GetPlaybackState(id) == piSoundEngine::PlaybackState::Stopped, "stop from paused state");
            require(engine->Play(id, 800000), "restart from paused then stopped");
            check(0.3, "paused-stop restart target");
            if (!looping)
            {
                require(engine->Play(id, 2800000), "expire a currently playing sound");
                require(mix(6) < 0.0001, "expiring playback stops current audio");
                require(engine->GetPlaybackState(id) == piSoundEngine::PlaybackState::Stopped, "expired active sound reports stopped");
                require(engine->Play(id, 800000), "valid play after expiring active audio");
                check(0.3, "restart after expiring active audio");
            }
            require(engine->Stop(id), "stop before out-of-range offset");
            mix(3);
            require(engine->Play(id, 2800000), "play offset beyond asset duration");
            if (looping)
                check(0.3, "looping offset wraps to asset duration");
            else
            {
                require(mix(6) < 0.0001, "expired non-looping sound stays silent");
                require(engine->GetPlaybackState(id) == piSoundEngine::PlaybackState::Stopped, "expired non-looping sound reports stopped");
                require(!engine->Resume(id), "expired sound cannot resume");
                require(engine->Play(id, 800000), "valid play after expired offset");
                check(0.3, "valid restart after expired offset");
            }
            engine->DelSound(id);
        }
        require(errors == 0, "backend logged an audio error");
        backend->Deinit();
        piSoundEngineAudioSDKBackend::Destroy(backend);
        std::puts("IMM_AUDIO360_BACKEND PASS: repeated nonzero/zero restarts, silence and stopped/paused states");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "IMM_AUDIO360_BACKEND FAIL: %s\n", error.what());
        if (backend)
        {
            backend->Deinit();
            piSoundEngineAudioSDKBackend::Destroy(backend);
        }
        return 1;
    }
}
