// CPU-only deterministic engine acknowledgements through the production renderer.
#include <cstdio>
#include <cstdlib>
#include "libImmCore/src/libSound/piSoundEngine.h"
#include "libImmImporter/src/document/layerSound.h"
#include "libImmImporter/src/document/sequence.h"
#include "libImmPlayer/src/layerRenderers/layerRendererSound/layerRendererSound.h"

namespace
{
    using namespace ImmCore;
    using namespace ImmImporter;
    using namespace ImmPlayer;

    void RequireSound(bool condition, const char * message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "[IMM_SOUND_RECONCILE_TEST] %s\n", message);
            std::exit(1);
        }
    }

    class ControlledSoundEngine final : public piSoundEngine
    {
    public:
        PlaybackState state = PlaybackState::Stopped;
        int playCalls = 0, stopCalls = 0, pauseCalls = 0, resumeCalls = 0;
        int acceptedPauses = 0, acceptedResumes = 0;
        int failPauses = 0, failResumes = 0;
        float volume = 0;
        bool looping = false;

        bool Play(int, uint64_t) override
        { ++playCalls; state = PlaybackState::PlayingStarting; return true; }
        bool Stop(int) override
        { ++stopCalls; state = PlaybackState::Stopped; return true; }
        bool Pause(int) override
        {
            ++pauseCalls;
            if (state != PlaybackState::Playing) return false;
            if (failPauses > 0) { --failPauses; return false; }
            ++acceptedPauses; state = PlaybackState::PausingStarted; return true;
        }
        bool Resume(int) override
        {
            ++resumeCalls;
            if (state != PlaybackState::Paused) return false;
            if (failResumes > 0) { --failResumes; return false; }
            ++acceptedResumes; state = PlaybackState::PlayingStarting; return true;
        }
        PlaybackState GetPlaybackState(int) override { return state; }
        void PauseAllSounds() override {}
        void ResumeAllSounds() override {}
        void SetListener(const trans3d &) override {}
        int AddSound(void (*)(float *, size_t, size_t, void *), void *) override { return 17; }
        int AddSound(const wchar_t *, bool) override { return 17; }
        int AddSound(int, int, int, uint64_t, const void *, bool) override { return 17; }
        int AddSound(int, uint64_t, const void *, bool) override { return 17; }
        void DelSound(int) override { state = PlaybackState::Stopped; }
        float GetVolume(int) const override { return volume; }
        bool SetVolume(int, float value) override { volume = value; return true; }
        bool GetLooping(int) const override { return looping; }
        void SetLooping(int, bool value) override { looping = value; }
        SoundType GetType(int) const override { return SoundType::Flat; }
        bool ConvertType(int, SoundType) override { return false; }
        bool CanConvertType(int, SoundType) const override { return false; }
        void SetAttenMode(int, AttenuationType) override {}
        void SetAttenMinMax(int, double, double) override {}
        void SetPosition(int, const double *) override {}
        void SetOrientation(int, const double *, const double *) override {}
        void SetPositionOrientation(int, const double *, const double *, const double *) override {}
        void SetModifierType(int, ModifierType) override {}
        void SetModifierCone(int, double, double, double) override {}
        void SetModifierFrustum(int, double, double, double, double) override {}
        float ComputeModifiers(int) override { return 1; }
    };

    struct SoundFixture
    {
        piLog & log;
        Sequence sequence;
        Layer * layer = nullptr;
        LayerSound * sound = nullptr;
        ControlledSoundEngine engine;
        LayerRendererSound renderer;

        explicit SoundFixture(piLog & logger) : log(logger)
        {
            RequireSound(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "sound sequence");
            Layer * root = sequence.GetRoot();
            RequireSound(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
                1.0f, true, piTick(0), 0, UINT32_MAX, &log), "sound root");
            layer = sequence.CreateLayer(root);
            RequireSound(layer && layer->Init(Layer::Type::Sound, L"Reconciliation", true,
                trans3d::identity(), trans3d::identity(), 1.0f, false, piTick(0), 1, UINT32_MAX, &log), "sound layer");
            sound = new LayerSound();
            LayerSound::AttenuationParameters attenuation{};
            LayerSound::ModifierParameters modifier{};
            RequireSound(sound->Init(false, false, 1.0f, attenuation, modifier, LayerSound::Type::Flat, 1.0f, &log), "sound implementation");
            layer->SetImplementation(sound); layer->SetLoaded(true); sound->SetVolume(1.0f);
            RequireSound(sound->GetSound()->Init(48000, 1, 16, 48), "small PCM resource");
            RequireSound(renderer.Init(nullptr, &log, Drawing::ColorSpace::Linear, false) &&
                renderer.LoadInCPU(&log, layer) && renderer.LoadInSPU(&engine, &log, layer), "production renderer load");
        }
        ~SoundFixture()
        {
            renderer.UnloadInSPU(&engine, layer);
            renderer.Deinit(nullptr, &log);
            sequence.Deinit(&log);
        }
        void Tick() { renderer.GlobalWork(nullptr, &engine, &log, layer, 1.0f); }
        void StartPlaying()
        {
            sound->SetPlaying(true); Tick();
            RequireSound(engine.state == piSoundEngine::PlaybackState::PlayingStarting, "initial Play pending");
            engine.state = piSoundEngine::PlaybackState::Playing;
            Tick();
        }
        bool Result(const char * name, bool passed)
        {
            passed &= engine.playCalls == 1 && engine.stopCalls == 0;
            std::printf("[IMM_SOUND_RECONCILE_CASE] case=%s pass=%d actual=%d pauseCalls=%d acceptedPauses=%d resumeCalls=%d acceptedResumes=%d playCalls=%d stopCalls=%d\n",
                name, passed ? 1 : 0, static_cast<int>(engine.state), engine.pauseCalls, engine.acceptedPauses,
                engine.resumeCalls, engine.acceptedResumes, engine.playCalls, engine.stopCalls);
            return passed;
        }
    };

    bool CheckPauseDuringStart(piLog & log)
    {
        SoundFixture fixture(log);
        fixture.sound->SetPlaying(true); fixture.sound->SetPaused(true); fixture.Tick();
        const bool waited = fixture.engine.pauseCalls == 0;
        fixture.engine.state = piSoundEngine::PlaybackState::Playing;
        fixture.Tick();
        const bool accepted = fixture.engine.acceptedPauses == 1 && fixture.engine.state == piSoundEngine::PlaybackState::PausingStarted;
        if (accepted) { fixture.engine.state = piSoundEngine::PlaybackState::Paused; fixture.Tick(); }
        return fixture.Result("pause-during-start", waited && accepted && fixture.engine.pauseCalls == 1);
    }

    bool CheckResumeDuringPausePending(piLog & log)
    {
        SoundFixture fixture(log); fixture.StartPlaying();
        fixture.sound->SetPaused(true); fixture.Tick();
        RequireSound(fixture.engine.acceptedPauses == 1, "initial pause accepted");
        fixture.sound->SetPaused(false); fixture.Tick();
        const bool waited = fixture.engine.resumeCalls == 0;
        fixture.engine.state = piSoundEngine::PlaybackState::Paused;
        fixture.Tick();
        return fixture.Result("resume-during-pause-pending", waited && fixture.engine.acceptedResumes == 1 &&
            fixture.engine.resumeCalls == 1 && fixture.engine.state == piSoundEngine::PlaybackState::PlayingStarting);
    }

    bool CheckFailedPauseRetry(piLog & log)
    {
        SoundFixture fixture(log); fixture.StartPlaying();
        fixture.engine.failPauses = 1;
        fixture.sound->SetPaused(true); fixture.Tick();
        const bool failedWithoutStateChange = fixture.engine.pauseCalls == 1 && fixture.engine.state == piSoundEngine::PlaybackState::Playing;
        fixture.Tick();
        const bool retried = fixture.engine.pauseCalls == 2 && fixture.engine.acceptedPauses == 1;
        if (retried) { fixture.engine.state = piSoundEngine::PlaybackState::Paused; fixture.Tick(); }
        return fixture.Result("failed-pause-retry", failedWithoutStateChange && retried && fixture.engine.pauseCalls == 2);
    }

    bool CheckFailedResumeRetry(piLog & log)
    {
        SoundFixture fixture(log); fixture.StartPlaying();
        fixture.sound->SetPaused(true); fixture.Tick();
        fixture.engine.state = piSoundEngine::PlaybackState::Paused; fixture.Tick();
        fixture.engine.failResumes = 1;
        fixture.sound->SetPaused(false); fixture.Tick();
        const bool failedWithoutStateChange = fixture.engine.resumeCalls == 1 && fixture.engine.state == piSoundEngine::PlaybackState::Paused;
        fixture.Tick();
        const bool retried = fixture.engine.resumeCalls == 2 && fixture.engine.acceptedResumes == 1 &&
            fixture.engine.state == piSoundEngine::PlaybackState::PlayingStarting;
        if (retried) { fixture.engine.state = piSoundEngine::PlaybackState::Playing; fixture.Tick(); }
        return fixture.Result("failed-resume-retry", failedWithoutStateChange && retried && fixture.engine.resumeCalls == 2);
    }

    bool CheckPauseDuringResumePending(piLog & log)
    {
        SoundFixture fixture(log); fixture.StartPlaying();
        fixture.sound->SetPaused(true); fixture.Tick();
        fixture.engine.state = piSoundEngine::PlaybackState::Paused; fixture.Tick();
        fixture.sound->SetPaused(false); fixture.Tick();
        RequireSound(fixture.engine.acceptedResumes == 1, "initial resume accepted");
        fixture.sound->SetPaused(true); fixture.Tick();
        const bool waited = fixture.engine.pauseCalls == 1;
        fixture.engine.state = piSoundEngine::PlaybackState::Playing;
        fixture.Tick();
        return fixture.Result("pause-during-resume-pending", waited && fixture.engine.pauseCalls == 2 &&
            fixture.engine.acceptedPauses == 2 && fixture.engine.state == piSoundEngine::PlaybackState::PausingStarted);
    }

    bool CheckUnownedExternalPause(piLog & log)
    {
        SoundFixture fixture(log); fixture.StartPlaying();
        // Focus/PauseAll can pause a voice without this layer requesting it.
        // Its unchanged desired=false state does not authorize stealing that hold.
        fixture.engine.state = piSoundEngine::PlaybackState::Paused;
        fixture.Tick(); fixture.Tick();
        return fixture.Result("unowned-external-pause", !fixture.sound->GetPaused() &&
            fixture.engine.pauseCalls == 0 && fixture.engine.acceptedPauses == 0 &&
            fixture.engine.resumeCalls == 0 && fixture.engine.acceptedResumes == 0 &&
            fixture.engine.state == piSoundEngine::PlaybackState::Paused);
    }
}

void CheckSoundPlaybackReconciliation()
{
    ImmCore::piLog log;
    RequireSound(log.Init(L"artifacts/sound-playback-reconciliation.log", PILOG_TXT), "sound reconciliation log");
    const bool starting = CheckPauseDuringStart(log);
    const bool pendingPause = CheckResumeDuringPausePending(log);
    const bool pauseRetry = CheckFailedPauseRetry(log);
    const bool resumeRetry = CheckFailedResumeRetry(log);
    const bool pendingResume = CheckPauseDuringResumePending(log);
    const bool unownedPause = CheckUnownedExternalPause(log);
    log.End();
    RequireSound(starting && pendingPause && pauseRetry && resumeRetry && pendingResume && unownedPause,
        "real renderer must reconcile desired pause/resume against acknowledged engine state");
    std::puts("[IMM_SOUND_RECONCILE_TEST] pass=1 startingPause=1 pendingPauseResume=1 pauseRetry=1 resumeRetry=1 pendingResumePause=1 unownedPausePreserved=1 noAudibleClaim=1");
}
