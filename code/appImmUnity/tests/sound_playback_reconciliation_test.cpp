// CPU-only deterministic engine acknowledgements through the production renderer.
#include <cstdio>
#include <cstdlib>
#include <map>
#include <chrono>
#include <thread>
#include "libImmCore/src/libSound/piSoundEngine.h"
#include "libImmImporter/src/document/layerSound.h"
#include "libImmImporter/src/document/sequence.h"
#include "libImmPlayer/src/layerRenderers/layerRendererSound/layerRendererSound.h"
#include "libImmPlayer/src/document.h"
#include "libImmImporter/src/fromImmersive/fromImmersive.h"

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
        bool deferStops = false;
        struct Voice
        {
            PlaybackState state = PlaybackState::Stopped;
            uint64_t offset = 0;
            int plays = 0, stops = 0, pauses = 0, resumes = 0, deletions = 0;
        };
        std::map<int, Voice> voices;
        int nextVoice = 17;
        int failAddAfter = -1;

        PlaybackState & StateFor(int id) { return id == 17 ? state : voices[id].state; }
        int AddVoice()
        {
            if (failAddAfter == 0) return -1;
            if (failAddAfter > 0) --failAddAfter;
            const int id = nextVoice++; voices[id] = Voice{}; return id;
        }

        bool Play(int id, uint64_t offset) override
        {
            ++playCalls; ++voices[id].plays; voices[id].offset = offset;
            // Audio360 seeks first but rejects Play on a running/starting voice.
            if (StateFor(id) == PlaybackState::Playing || StateFor(id) == PlaybackState::PlayingStarting) return false;
            StateFor(id) = PlaybackState::PlayingStarting; return true;
        }
        bool Stop(int id) override
        { ++stopCalls; ++voices[id].stops; StateFor(id) = deferStops ? PlaybackState::StoppingStarted : PlaybackState::Stopped; return true; }
        bool Pause(int id) override
        {
            ++pauseCalls; ++voices[id].pauses;
            if (StateFor(id) != PlaybackState::Playing) return false;
            if (failPauses > 0) { --failPauses; return false; }
            ++acceptedPauses; StateFor(id) = PlaybackState::PausingStarted; return true;
        }
        bool Resume(int id) override
        {
            ++resumeCalls; ++voices[id].resumes;
            if (StateFor(id) != PlaybackState::Paused) return false;
            if (failResumes > 0) { --failResumes; return false; }
            ++acceptedResumes; StateFor(id) = PlaybackState::PlayingStarting; return true;
        }
        PlaybackState GetPlaybackState(int id) override { return StateFor(id); }
        void PauseAllSounds() override {}
        void ResumeAllSounds() override {}
        void SetListener(const trans3d &) override {}
        int AddSound(void (*)(float *, size_t, size_t, void *), void *) override { return AddVoice(); }
        int AddSound(const wchar_t *, bool) override { return AddVoice(); }
        int AddSound(int, int, int, uint64_t, const void *, bool) override { return AddVoice(); }
        int AddSound(int, uint64_t, const void *, bool) override { return AddVoice(); }
        void DelSound(int id) override { ++voices[id].deletions; StateFor(id) = PlaybackState::Stopped; }
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

    bool CheckIndependentPreviewVoice(piLog & log)
    {
        SoundFixture fixture(log);
        fixture.StartPlaying();
        const auto mainSoundState = fixture.sound->CaptureRuntimePlaybackState();
        const int originalGpuId = fixture.sound->GetGpuId();
        const auto originalType = fixture.sound->GetType();
        fixture.sound->SetPlaying(true, 123456);
        fixture.sound->SetPaused(true);
        fixture.sound->SetForceRestart(true);
        auto * preview = fixture.renderer.CreatePreviewVoice(&fixture.engine, &log, fixture.layer);
        RequireSound(preview != nullptr && fixture.engine.nextVoice == 19, "preview creates a separate engine voice");
        RequireSound(fixture.renderer.UpdatePreviewVoice(&fixture.engine, &log, fixture.layer, preview, 1.0f, true, false),
            "paused preview update");
        RequireSound(fixture.engine.voices[18].plays == 0 && fixture.engine.voices[17].plays == 1 &&
            fixture.sound->GetGpuId() == originalGpuId && fixture.sound->GetType() == originalType &&
            fixture.sound->GetPlaying() && fixture.sound->GetPaused() && fixture.sound->GetOffset() == 123456 &&
            fixture.sound->GetForceRestart() && fixture.sound->GetVolume() == 1.0f,
            "paused creation preserves original handle, type and sound runtime without playing either voice");

        RequireSound(!fixture.renderer.PauseMainVoice(&fixture.engine, fixture.layer), "main pause awaits acknowledgment");
        fixture.engine.state = piSoundEngine::PlaybackState::StoppingStarted;
        RequireSound(!fixture.renderer.PauseMainVoice(&fixture.engine, fixture.layer), "pending stop is not main pause readiness");
        fixture.engine.state = piSoundEngine::PlaybackState::Paused;
        RequireSound(fixture.renderer.PauseMainVoice(&fixture.engine, fixture.layer), "main voice physically paused");

        fixture.sound->SetForceRestart(false);
        fixture.sound->SetPaused(false);
        RequireSound(fixture.renderer.UpdatePreviewVoice(&fixture.engine, &log, fixture.layer, preview, 1.0f, true, false),
            "start duplicate voice");
        RequireSound(fixture.engine.voices[18].plays == 1 && fixture.engine.voices[18].offset == 123456,
            "preview starts at its own source offset");
        fixture.engine.StateFor(18) = piSoundEngine::PlaybackState::Playing;
        RequireSound(fixture.renderer.UpdatePreviewVoice(&fixture.engine, &log, fixture.layer, preview, 1.0f, false, false),
            "pause inaudible duplicate");
        RequireSound(fixture.engine.StateFor(18) == piSoundEngine::PlaybackState::PausingStarted, "duplicate pause pending");
        fixture.engine.StateFor(18) = piSoundEngine::PlaybackState::Paused;
        RequireSound(fixture.renderer.UpdatePreviewVoice(&fixture.engine, &log, fixture.layer, preview, 1.0f, true, false),
            "resume duplicate cursor");
        RequireSound(fixture.engine.voices[18].resumes == 1 && fixture.engine.voices[18].plays == 1 &&
            fixture.engine.voices[18].offset == 123456, "resume preserves duplicate cursor without replay");
        fixture.engine.StateFor(18) = piSoundEngine::PlaybackState::Playing;

        fixture.engine.deferStops = true;
        fixture.sound->SetPlaying(true, 333000);
        RequireSound(fixture.renderer.UpdatePreviewVoice(&fixture.engine, &log, fixture.layer, preview, 1.0f, true, true) &&
            fixture.renderer.UpdatePreviewVoice(&fixture.engine, &log, fixture.layer, preview, 1.0f, true, false),
            "active seek waits for asynchronous Stop");
        RequireSound(fixture.engine.StateFor(18) == piSoundEngine::PlaybackState::StoppingStarted &&
            fixture.engine.voices[18].stops == 1 && fixture.engine.voices[18].plays == 1 &&
            fixture.engine.voices[18].offset == 123456, "pending restart cannot repeatedly seek or Play a running voice");
        fixture.engine.StateFor(18) = piSoundEngine::PlaybackState::Stopped;
        RequireSound(fixture.renderer.UpdatePreviewVoice(&fixture.engine, &log, fixture.layer, preview, 1.0f, true, false) &&
            fixture.engine.voices[18].plays == 2 && fixture.engine.voices[18].offset == 333000,
            "active seek plays only after acknowledged Stop");
        fixture.engine.deferStops = false;
        fixture.engine.StateFor(18) = piSoundEngine::PlaybackState::Playing;

        fixture.sound->SetPlaying(true, 654321);
        fixture.sound->SetPaused(true);
        RequireSound(fixture.renderer.UpdatePreviewVoice(&fixture.engine, &log, fixture.layer, preview, 1.0f, true, true),
            "seek restart latched while paused");
        RequireSound(fixture.engine.voices[18].plays == 2, "paused seek must not emit audio");
        fixture.engine.StateFor(18) = piSoundEngine::PlaybackState::Paused;
        fixture.sound->SetPaused(false);
        RequireSound(fixture.renderer.UpdatePreviewVoice(&fixture.engine, &log, fixture.layer, preview, 1.0f, true, false),
            "seek restart consumed on audible update");
        RequireSound(fixture.engine.voices[18].plays == 3 && fixture.engine.voices[18].offset == 654321,
            "seek restarts duplicate at latest offset");
        fixture.engine.StateFor(18) = piSoundEngine::PlaybackState::Stopped;
        RequireSound(fixture.renderer.UpdatePreviewVoice(&fixture.engine, &log, fixture.layer, preview, 1.0f, true, false) &&
            fixture.renderer.UpdatePreviewVoice(&fixture.engine, &log, fixture.layer, preview, 1.0f, true, false),
            "completed preview remains healthy");
        RequireSound(fixture.engine.voices[18].plays == 3, "EOF cannot auto-replay unchanged preview");

        fixture.renderer.DestroyPreviewVoice(&fixture.engine, preview);
        RequireSound(fixture.engine.voices[18].deletions == 1 && fixture.engine.voices[17].deletions == 0 &&
            fixture.engine.voices[17].plays == 1 && fixture.engine.voices[17].stops == 0 &&
            fixture.engine.state == piSoundEngine::PlaybackState::Paused && fixture.sound->GetGpuId() == originalGpuId,
            "preview teardown deletes only duplicate and preserves paused main cursor");
        fixture.sound->RestoreRuntimePlaybackState(mainSoundState);
        RequireSound(fixture.sound->GetPlaying() && !fixture.sound->GetPaused() &&
            fixture.sound->GetOffset() == 0 && !fixture.sound->GetForceRestart() && fixture.sound->GetVolume() == 1.0f,
            "main Sound runtime capsule restores playback offset, pause, restart and volume");
        fixture.Tick();
        RequireSound(fixture.engine.voices[17].resumes == 1 && fixture.engine.voices[17].plays == 1 &&
            fixture.engine.voices[17].stops == 0, "main cursor resumes without Play or Stop");
        std::puts("[IMM_SOUND_PREVIEW_VOICE_TEST] pass=1 separateIds=1 pausedCreation=1 independentTransport=1 latchedSeek=1 noEofReplay=1 duplicateOnlyTeardown=1 mainCursorPreserved=1 noAudibleClaim=1");
        return true;
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
    const bool independentPreview = CheckIndependentPreviewVoice(log);
    log.End();
    RequireSound(starting && pendingPause && pauseRetry && resumeRetry && pendingResume && unownedPause && independentPreview,
        "real renderer must reconcile desired pause/resume against acknowledged engine state");
    std::puts("[IMM_SOUND_RECONCILE_TEST] pass=1 startingPause=1 pendingPauseResume=1 pauseRetry=1 resumeRetry=1 pendingResumePause=1 unownedPausePreserved=1 noAudibleClaim=1");
}

void CheckDocumentPreviewAudio(const wchar_t * fixturePath)
{
    piLog log;
    RequireSound(log.Init(L"artifacts/document-preview-audio.log", PILOG_TXT), "document audio log");
    Document doc;
    ControlledSoundEngine engine;
    LayerRendererSound renderer;
    RequireSound(doc.Init(L"preview-audio", 0) && renderer.Init(nullptr, &log, Drawing::ColorSpace::Linear, false),
        "document audio fixture initialization");
    piTick now(1000000);
    const auto cpu = [&](const Document::Command * command = nullptr) {
        doc.UpdateStateCPU(&renderer, nullptr, nullptr, nullptr, Drawing::ColorSpace::Linear,
            Drawing::PaintRenderingTechnique::Static, &engine, &log, now, command);
        doc.UpdateStateGPU(nullptr, nullptr, nullptr, nullptr, &log, Drawing::ColorSpace::Linear);
    };
    Document::Command load{};
    load.mType = Document::Command::Type::Load; load.mFileType = Document::ImportType::IMM_disk;
    RequireSound(load.mStrArg.Init(1024) && load.mStrArg.CopyW(fixturePath), "document audio fixture path");
    cpu(&load);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while ((doc.GetLoadingState() != Document::LoadingState::Loaded || IsLoadingAsync(doc.GetSequence())) &&
        std::chrono::steady_clock::now() < deadline)
    { std::this_thread::sleep_for(std::chrono::milliseconds(1)); cpu(); }
    RequireSound(doc.GetLoadingState() == Document::LoadingState::Loaded && !IsLoadingAsync(doc.GetSequence()),
        "document audio fixture loaded");
    Sequence * sequence = doc.GetSequence();
    Layer * root = sequence->GetRoot();
    Layer * boundary = sequence->CreateLayer(root);
    RequireSound(boundary && boundary->Init(Layer::Type::Group, L"AudioPreview", true, trans3d::identity(),
        trans3d::identity(), 1.0f, true, piTick(1000), 0, UINT32_MAX, &log), "document audio boundary");
    const auto addTimeline = [&](const wchar_t * name, int duration, int stop) {
        Layer * timeline = sequence->CreateLayer(boundary);
        RequireSound(timeline && timeline->Init(Layer::Type::Group, name, true, trans3d::identity(),
            trans3d::identity(), 1.0f, true, piTick(duration), 1, UINT32_MAX, &log), "nested audio timeline");
        Layer::AnimValue key{}; key.mBool = true;
        RequireSound(timeline->AddKey(piTick(0), Layer::AnimProperty::Visibility, key, Layer::InterpolationType::None),
            "nested audio timeline visibility");
        if (stop > 0)
        {
            key = Layer::AnimValue{}; key.mInt = static_cast<uint32_t>(Layer::AnimAction::Stop);
            RequireSound(timeline->AddKey(piTick(stop), Layer::AnimProperty::Action, key, Layer::InterpolationType::None),
                "nested audio Stop");
        }
        return timeline;
    };
    Layer * finite = addTimeline(L"FiniteAudio", 80, 0);
    Layer * stopped = addTimeline(L"StoppedAudio", 0, 60);
    const auto addSound = [&](Layer * parent, const wchar_t * name, int offset) {
        Layer * layer = sequence->CreateLayer(parent);
        RequireSound(layer && layer->Init(Layer::Type::Sound, name, true, trans3d::identity(), trans3d::identity(),
            1.0f, false, piTick(0), 1, UINT32_MAX, &log), "document PCM layer");
        auto * sound = new LayerSound();
        LayerSound::AttenuationParameters attenuation{}; LayerSound::ModifierParameters modifier{};
        RequireSound(sound->Init(false, false, 1.0f, attenuation, modifier, LayerSound::Type::Flat, 1.0f, &log) &&
            sound->GetSound()->Init(48000, 1, 16, 48), "document tiny PCM payload");
        layer->SetImplementation(sound); layer->SetLoaded(true); sound->SetVolume(1.0f);
        if (offset >= 0)
        {
            Layer::AnimValue key{}; key.mBool = true;
            RequireSound(layer->AddKey(piTick(0), Layer::AnimProperty::Visibility, key, Layer::InterpolationType::None), "PCM visibility0");
            key.mInt = offset;
            RequireSound(layer->AddKey(piTick(0), Layer::AnimProperty::Offset, key, Layer::InterpolationType::None), "PCM offset0");
        }
        RequireSound(renderer.LoadInCPU(&log, layer) && renderer.LoadInSPU(&engine, &log, layer), "document PCM original voice");
        return layer;
    };
    Layer * mainLayer = addSound(root, L"MainAudio", -1);
    Layer * first = addSound(finite, L"LocalAudioA", 100);
    Layer * second = addSound(stopped, L"LocalAudioB", 200);
    auto * mainSound = static_cast<LayerSound *>(mainLayer->GetImplementation());
    auto * firstSound = static_cast<LayerSound *>(first->GetImplementation());
    const int originalGpu = firstSound->GetGpuId();
    mainSound->SetPlaying(true);
    renderer.GlobalWork(nullptr, &engine, &log, mainLayer, 1.0f);
    engine.state = piSoundEngine::PlaybackState::Playing;
    const auto originalMain = mainSound->CaptureRuntimePlaybackState();
    const auto originalFirst = firstSound->CaptureRuntimePlaybackState();
    RequireSound(doc.AttachEditing(), "attach document audio fixture");
    const auto control = [&](int action, int tick = 0) {
        return doc.ControlTimelinePreview(boundary, action, piTick(tick), now, &log, &renderer, &engine);
    };
    const auto audio = [&]() { doc.GlobalWorkTimelinePreviewAudio(&renderer, &engine, &log, 1.0f, now); };
    RequireSound(control(0, 20) == 0, "begin paused document audio preview");
    engine.failAddAfter = 1;
    const int rollbackId = engine.nextVoice;
    RequireSound(control(5) == -5 && !doc.GetTimelinePreviewAudioEnabled() && !doc.GetTimelinePreviewAudioPending() &&
        engine.voices[rollbackId].deletions == 1 && engine.voices[17].deletions == 0,
        "partial audio enable failure rolls back duplicate only");
    engine.failAddAfter = -1;
    const int cloneA = engine.nextVoice, cloneB = cloneA + 1;
    RequireSound(control(5) == 0 && doc.GetTimelinePreviewAudioEnabled(), "enable paused document preview audio");
    RequireSound(firstSound->GetPlaying() && firstSound->GetOffset() == piTick::ToMicroSeconds(piTick(120)),
        "canonical-visible sound visibility0 starts first scoped span with active offset");
    audio();
    RequireSound(engine.voices[cloneA].plays == 0 && engine.voices[cloneB].plays == 0 && doc.GetTimelinePreviewAudioPending(),
        "paused enable cannot Play and main pause remains pending");
    RequireSound(control(2) == 0, "play pending audio preview");
    audio();
    now = now + piTick(100); cpu(); audio();
    RequireSound(doc.GetTimelinePreviewContext()->mBoundaryElapsed == piTick(20),
        "pending original pause cannot consume preview clock time");
    RequireSound(engine.voices[cloneA].plays == 0 && engine.voices[17].plays == 1 && engine.voices[17].stops == 0,
        "duplicate playback waits for actual original pause");
    engine.state = piSoundEngine::PlaybackState::StoppingStarted; audio();
    RequireSound(doc.GetTimelinePreviewAudioPending() && engine.voices[cloneA].plays == 0, "logical stopping is not pause acknowledgment");
    engine.state = piSoundEngine::PlaybackState::Paused; audio();
    RequireSound(!doc.GetTimelinePreviewAudioPending() && engine.voices[cloneA].plays == 1 && engine.voices[cloneB].plays == 1 &&
        engine.voices[cloneA].offset == piTick::ToMicroSeconds(piTick(120)) &&
        doc.GetTimelinePreviewContext()->mBoundaryElapsed == piTick(20), "acknowledged pause reanchors without a preview time jump");
    engine.StateFor(cloneA) = engine.StateFor(cloneB) = piSoundEngine::PlaybackState::Playing;
    now = now + piTick(1); cpu(); audio();
    RequireSound(firstSound->GetOffset() == piTick::ToMicroSeconds(piTick(120)) && engine.voices[cloneA].plays == 1,
        "next scoped tick does not restart first visibility span");
    RequireSound(control(3) == 0, "pause document preview audio"); audio();
    engine.StateFor(cloneA) = engine.StateFor(cloneB) = piSoundEngine::PlaybackState::Paused;
    RequireSound(control(1, 50) == 0, "seek paused audio preview"); audio();
    RequireSound(engine.voices[cloneA].plays == 1 && firstSound->GetOffset() == piTick::ToMicroSeconds(piTick(150)),
        "paused audio seek updates intent without Play");
    RequireSound(control(2) == 0, "resume sought preview audio"); audio();
    RequireSound(engine.voices[cloneA].plays == 2 && engine.voices[cloneA].offset == piTick::ToMicroSeconds(piTick(150)),
        "sought preview starts duplicate at new offset");
    RequireSound(control(6) == 0 && !doc.GetTimelinePreviewAudioEnabled() &&
        engine.voices[cloneA].deletions == 1 && engine.voices[cloneB].deletions == 1, "mute deletes all duplicate voices");
    const int reenabledA = engine.nextVoice;
    RequireSound(control(5) == 0, "reenable document audio"); audio();
    RequireSound(engine.voices[reenabledA].plays == 1 && firstSound->GetGpuId() == originalGpu,
        "reenable creates new voices without replacing source handles");
    engine.StateFor(reenabledA) = engine.StateFor(reenabledA + 1) = piSoundEngine::PlaybackState::Playing;
    now = now + piTick(40); cpu(); audio(); // Natural local50 -> local90 crosses Stop60 and finite end80.
    const auto * context = doc.GetTimelinePreviewContext();
    RequireSound(context->mBoundaryElapsed == piTick(90) && finite->GetTimelineElapsed(now, context) == piTick(90) &&
        stopped->IsStoppedByAction() && stopped->GetTimelineElapsed(now, context) == piTick(59),
        "natural advance retains independent finite and authored Stop clocks");
    RequireSound(engine.voices[reenabledA].plays == 1 && engine.voices[reenabledA + 1].plays == 1 &&
        engine.StateFor(reenabledA) == piSoundEngine::PlaybackState::PausingStarted &&
        engine.StateFor(reenabledA + 1) == piSoundEngine::PlaybackState::PausingStarted,
        "finite end and nested Stop pause duplicate voices without replay");
    RequireSound(control(4) == 0 && !doc.GetTimelinePreviewBoundary() &&
        engine.voices[reenabledA].deletions == 1 && engine.voices[reenabledA + 1].deletions == 1,
        "preview exit destroys duplicates");
    const auto restoredMain = mainSound->CaptureRuntimePlaybackState();
    const auto restoredFirst = firstSound->CaptureRuntimePlaybackState();
    RequireSound(restoredMain.playing == originalMain.playing && restoredMain.paused == originalMain.paused &&
        restoredMain.offset == originalMain.offset && restoredMain.forceRestart == originalMain.forceRestart && restoredMain.volume == originalMain.volume &&
        restoredFirst.playing == originalFirst.playing && restoredFirst.paused == originalFirst.paused &&
        restoredFirst.offset == originalFirst.offset && restoredFirst.forceRestart == originalFirst.forceRestart && restoredFirst.volume == originalFirst.volume,
        "document exit restores complete main sound runtime");
    renderer.GlobalWork(nullptr, &engine, &log, mainLayer, 1.0f);
    RequireSound(engine.voices[17].plays == 1 && engine.voices[17].stops == 0 && engine.voices[17].resumes == 1,
        "document exit resumes original cursor without replay or stop");
    engine.state = piSoundEngine::PlaybackState::Playing;
    RequireSound(control(0) == 0 && control(5) == 0, "begin audio session before unload");
    const int unloadClone = engine.nextVoice - 2;
    doc.UnloadSync(&renderer, nullptr, nullptr, nullptr, Drawing::ColorSpace::Linear,
        Drawing::PaintRenderingTechnique::Static, &engine, nullptr, &log, now);
    RequireSound(doc.GetLoadingState() == Document::LoadingState::UnloadingCompleted &&
        engine.voices[unloadClone].deletions == 1 && engine.voices[unloadClone + 1].deletions == 1 &&
        engine.voices[17].deletions == 1 && engine.voices[18].deletions == 1 && engine.voices[19].deletions == 1,
        "document unload releases duplicates and source voices exactly once");
    doc.End(); renderer.Deinit(nullptr, &log); load.mStrArg.End(); log.End();
    std::puts("[IMM_DOCUMENT_PREVIEW_AUDIO_TEST] pass=1 firstSpan0=1 pauseBarrier=1 pendingClockRetention=1 pausedSeek=1 finiteEnd=1 nestedStop=1 enableRollback=1 muteReenable=1 exitRestoration=1 unloadCleanup=1 noAudibleClaim=1");
}
