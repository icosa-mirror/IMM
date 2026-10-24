// Deterministic request/callback ordering against the helper used by Audio360.
#include <cstdio>
#include <cstdlib>
#include "libImmCore/src/libSound/piSoundPlaybackState.h"

namespace
{
    using ImmCore::SoundPlaybackState;
    using Phase = ImmCore::piSoundEngine::PlaybackState;
    using Snapshot = SoundPlaybackState::Snapshot;

    void Require(bool condition, const char * name)
    {
        if (!condition)
        {
            std::fprintf(stderr, "[IMM_SOUND_PLAYBACK_STATE_TEST] failed: %s\n", name);
            std::exit(1);
        }
        std::printf("[IMM_SOUND_PLAYBACK_STATE_CASE] pass=1 case=%s\n", name);
    }

    void Playing(SoundPlaybackState & state)
    {
        state.Reset();
        Snapshot previous = 0;
        state.Request(Phase::PlayingStarting, false, previous);
        state.OnPlaybackChanged(true);
    }
}

int main()
{
    SoundPlaybackState state;
    Snapshot previous = 0;

    Playing(state);
    state.Request(Phase::PausingStarted, true, previous);
    Require(SoundPlaybackState::State(state.Capture()) == Phase::StoppingStarted &&
        SoundPlaybackState::State(state.Capture()) != Phase::Paused,
        "pending-logical-stop-is-not-pause-ready");
    state.OnPlaybackChanged(false); // A physical Pause implements logical Stop.
    Require(SoundPlaybackState::LogicallyStopped(state.Capture()) &&
        SoundPlaybackState::State(state.Capture()) == Phase::Stopped, "prior-logical-stop");
    Snapshot requested = state.Request(Phase::PlayingStarting, false, previous);
    state.OnPlaybackChanged(false); // Queued Pause acknowledgement during rejected Play.
    Require(SoundPlaybackState::PhysicalState(state.Capture()) == Phase::Paused &&
        SoundPlaybackState::State(state.Capture()) == Phase::PlayingStarting,
        "queued-pause-does-not-acknowledge-new-play");
    state.Rollback(requested, previous);
    Require(SoundPlaybackState::LogicallyStopped(state.Capture()) &&
        SoundPlaybackState::State(state.Capture()) == Phase::Stopped, "failed-play-restores-prior-logical-stop");

    Playing(state);
    state.Request(Phase::PausingStarted, true, previous);
    requested = state.Request(Phase::PlayingStarting, false, previous);
    state.OnPlaybackChanged(false);
    state.Rollback(requested, previous);
    Require(SoundPlaybackState::State(state.Capture()) == Phase::Stopped &&
        SoundPlaybackState::LogicallyStopped(state.Capture()), "failed-play-restores-acknowledged-prior-pending-stop");

    Playing(state);
    state.Request(Phase::PausingStarted, false, previous);
    state.OnPlaybackChanged(true);
    Require(SoundPlaybackState::State(state.Capture()) == Phase::PausingStarted,
        "queued-playing-does-not-acknowledge-new-pause");
    state.OnPlaybackChanged(false);
    Require(SoundPlaybackState::State(state.Capture()) == Phase::Paused, "pause-target-acknowledged");

    Playing(state);
    requested = state.Request(Phase::PlayingStarting, false, previous);
    state.OnStopped();
    state.Rollback(requested, previous);
    Require(SoundPlaybackState::State(state.Capture()) == Phase::Stopped, "failed-play-preserves-observed-eos");

    Playing(state);
    requested = state.Request(Phase::PausingStarted, false, previous);
    state.OnPlaybackChanged(false);
    state.Rollback(requested, previous);
    Require(!SoundPlaybackState::LogicallyStopped(state.Capture()) &&
        SoundPlaybackState::State(state.Capture()) == Phase::Paused, "failed-pause-preserves-observed-paused");

    state.Reset();
    state.Request(Phase::PausingStarted, false, previous);
    state.OnPlaybackChanged(false);
    requested = state.Request(Phase::PlayingStarting, false, previous);
    state.OnPlaybackChanged(true);
    state.Rollback(requested, previous);
    Require(!SoundPlaybackState::LogicallyStopped(state.Capture()) &&
        SoundPlaybackState::State(state.Capture()) == Phase::Playing, "failed-resume-preserves-observed-playing");

    Playing(state);
    const Snapshot requestA = state.Request(Phase::PausingStarted, false, previous);
    const Snapshot beforeA = previous;
    Snapshot beforeB = 0;
    const Snapshot requestB = state.Request(Phase::PausingStarted, true, beforeB);
    state.Rollback(requestA, beforeA);
    Require(state.Capture() == requestB && SoundPlaybackState::LogicallyStopped(state.Capture()) &&
        SoundPlaybackState::State(state.Capture()) == Phase::StoppingStarted,
        "rollback-a-cannot-rewind-same-phase-superseding-stop-b");

    Playing(state);
    requested = state.Request(Phase::PausingStarted, true, previous);
    state.OnPlaybackChanged(false);
    state.Rollback(requested, previous);
    Require(!SoundPlaybackState::LogicallyStopped(state.Capture()) &&
        SoundPlaybackState::State(state.Capture()) == Phase::Paused, "failed-stop-restores-intent-keeps-physical-pause");

    Playing(state);
    requested = state.Request(Phase::PausingStarted, true, previous);
    state.OnStopped();
    state.Rollback(requested, previous);
    Require(SoundPlaybackState::State(state.Capture()) == Phase::Stopped, "failed-stop-preserves-observed-eos");

    std::puts("[IMM_SOUND_PLAYBACK_STATE_TEST] pass=1 callbackRollback=1 logicalStop=1 generationSupersession=1 noSdkOrAudibleClaim=1");
    return 0;
}
