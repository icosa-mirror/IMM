#pragma once

#include <atomic>
#include <cstdint>
#include "piSoundEngine.h"

namespace ImmCore
{
    // Internal voice transport state shared by SDK callbacks and its controller.
    // Physical pause and logical stop remain distinct through pending requests.
    class SoundPlaybackState
    {
    public:
        using Snapshot = uint64_t;
        using Phase = piSoundEngine::PlaybackState;

        void Reset(bool stopped = true) { mValue.store(Encode(Phase::Stopped, stopped)); }
        Snapshot Capture() const { return mValue.load(); }
        static bool LogicallyStopped(Snapshot value) { return (value & StopFlag) != 0; }
        static Phase PhysicalState(Snapshot value) { return static_cast<Phase>(value & 0xffu); }
        static Phase State(Snapshot value)
        {
            const auto phase = PhysicalState(value);
            const auto target = value & TargetMask;
            if (target == PlayingTarget) return Phase::PlayingStarting;
            if (target == PausedTarget) return LogicallyStopped(value) ? Phase::StoppingStarted : Phase::PausingStarted;
            if (!LogicallyStopped(value)) return phase;
            return phase == Phase::Paused || phase == Phase::Stopped ? Phase::Stopped : Phase::StoppingStarted;
        }
        static bool Pending(Snapshot value) { return (value & TargetMask) != 0; }

        Snapshot Request(Phase phase, bool stopped, Snapshot & previous)
        {
            previous = mValue.load();
            Snapshot request;
            do { request = Next(previous, phase, stopped); }
            while (!mValue.compare_exchange_weak(previous, request));
            return request;
        }

        bool RequestIf(Snapshot & previous, Phase phase, bool stopped, Snapshot & request)
        {
            request = Next(previous, phase, stopped);
            return mValue.compare_exchange_strong(previous, request);
        }

        void Acknowledge(Snapshot request, Phase phase, bool stopped)
        { mValue.compare_exchange_strong(request, WithPlayback(request, phase, stopped)); }

        void Rollback(Snapshot request, Snapshot previous)
        {
            Snapshot current = mValue.load();
            while ((current & GenerationMask) == (request & GenerationMask))
            {
                // Preserve a callback's physical observation, while rolling back
                // this failed request's intent. A real EOF/STOPPED is definitive.
                Snapshot restored;
                if (current == request)
                    restored = (current & GenerationMask) | (previous & ~GenerationMask);
                else
                {
                    const auto phase = PhysicalState(current);
                    const bool stopped = phase == Phase::Stopped || LogicallyStopped(previous);
                    Snapshot target = previous & TargetMask;
                    if (phase == Phase::Stopped || Matches(target, phase)) target = 0;
                    restored = WithPlayback(current, phase, stopped) | target;
                }
                if (mValue.compare_exchange_weak(current, restored)) return;
            }
        }

        void OnStopped() { Observe(Phase::Stopped, true, false); }
        void OnPlaybackChanged(bool playing) { Observe(playing ? Phase::Playing : Phase::Paused, false, true); }

    private:
        static constexpr Snapshot StopFlag = 0x100u;
        static constexpr Snapshot PlayingTarget = 0x200u;
        static constexpr Snapshot PausedTarget = 0x400u;
        static constexpr Snapshot TargetMask = PlayingTarget | PausedTarget;
        static constexpr Snapshot GenerationStep = 0x800u;
        static constexpr Snapshot GenerationMask = ~(GenerationStep - 1);
        std::atomic<Snapshot> mValue;

        static Snapshot Encode(Phase phase, bool stopped)
        { return static_cast<Snapshot>(phase) | (stopped ? StopFlag : 0); }
        static Snapshot WithPlayback(Snapshot value, Phase phase, bool stopped)
        { return (value & GenerationMask) | Encode(phase, stopped); }
        static Snapshot Next(Snapshot value, Phase phase, bool stopped)
        {
            const Snapshot target = phase == Phase::PlayingStarting ? PlayingTarget : PausedTarget;
            return ((value & GenerationMask) + GenerationStep) | Encode(PhysicalState(value), stopped) | target;
        }
        static bool Matches(Snapshot target, Phase phase)
        { return (target == PlayingTarget && phase == Phase::Playing) || (target == PausedTarget && phase == Phase::Paused); }
        void Observe(Phase phase, bool stopped, bool preserveIntent)
        {
            Snapshot previous = mValue.load(), next;
            do
            {
                Snapshot target = preserveIntent ? previous & TargetMask : 0;
                if (Matches(target, phase)) target = 0;
                next = WithPlayback(previous, phase, preserveIntent ? LogicallyStopped(previous) : stopped) | target;
            }
            while (!mValue.compare_exchange_weak(previous, next));
        }
    };
}
