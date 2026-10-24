#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "libImmImporter/src/document/layer.h"
#include "libImmImporter/src/document/sequence.h"
#include "libImmImporter/src/document/layerSound.h"
#include "libImmPlayer/src/mngrPlayer.h"

namespace
{
    using namespace ImmCore;
    using namespace ImmImporter;

    void RequireTimeline(bool condition, const char * message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "[IMM_TIMELINE_REPEAT_TEST] %s\n", message);
            std::exit(1);
        }
    }

    void AddBool(Layer * layer, Layer::AnimProperty property, int64_t tick, bool boolean)
    {
        Layer::AnimValue value{};
        value.mBool = boolean;
        RequireTimeline(layer->AddKey(piTick(tick), property, value, Layer::InterpolationType::None), "add boolean key");
    }

    bool CheckNestedStopDomain(piLog * log, bool independent, bool laterCycle = false)
    {
        Sequence sequence;
        RequireTimeline(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "nested Stop sequence");
        sequence.SetInitialSpawnArea(nullptr);
        Layer * root = sequence.GetRoot();
        RequireTimeline(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
            1.0f, true, piTick(laterCycle ? 100 : 0), 0, UINT32_MAX, log), "nested Stop root");
        Layer * boundary = root;
        if (independent)
        {
            root->SetStartTime(piTick(0)); root->SetStopTime(piTick(2), true, true); root->SetPlaying(false);
            boundary = sequence.CreateLayer(root);
            RequireTimeline(boundary && boundary->Init(Layer::Type::Group, L"Boundary", true,
                trans3d::identity(), trans3d::identity(), 1.0f, true, piTick(laterCycle ? 100 : 0), 0, UINT32_MAX, log),
                "nested Stop independent boundary");
        }
        Layer * child = sequence.CreateLayer(boundary);
        RequireTimeline(child && child->Init(Layer::Type::Group, L"Nested", true, trans3d::identity(),
            trans3d::identity(), 1.0f, true, piTick(100), 0, UINT32_MAX, log), "nested Stop timeline");
        AddBool(child, Layer::AnimProperty::Visibility, 20, true);
        Layer::AnimValue value;
        value.init(); value.mInt = 10;
        RequireTimeline(child->AddKey(piTick(0), Layer::AnimProperty::Offset, value,
            Layer::InterpolationType::None), "nested Stop offset");
        value.mInt = static_cast<uint32_t>(Layer::AnimAction::Stop);
        RequireTimeline(child->AddKey(piTick(50), Layer::AnimProperty::Action, value,
            Layer::InterpolationType::None), "nested Stop parent-domain action");
        const piTick epoch(1000000);
        ImmPlayer::MngrPlayer player;
        Layer::EvaluationContext context;
        context.mBoundary = boundary; context.mBoundaryStartTime = epoch;
        context.mBoundaryDuration = piTick(laterCycle ? 100 : 0); context.mBoundaryRepeatCount = 0;
        if (!independent)
        {
            RequireTimeline(player.Init(log), "nested Stop manager");
            player.Enter(&sequence, epoch);
        }
        const auto update = [&](int64_t tick) {
            if (independent)
            {
                context.mBoundaryElapsed = piTick(tick);
                child->SetStateAt(epoch + piTick(tick), log, &context);
            }
            else player.Update(epoch + piTick(tick));
        };
        const int64_t phase = laterCycle ? 200 : 0;
        update(phase + 20); update(phase + 49);
        // The independent boundary is an explicit clock snapshot. Advance that
        // snapshot for the pre-action query without evaluating the Stop yet.
        if (independent) context.mBoundaryElapsed = piTick(phase + 50);
        const piTick before = child->GetTimelineElapsed(epoch + piTick(phase + 50), independent ? &context : nullptr);
        update(phase + 50);
        const piTick held = child->GetTimelineElapsed(epoch + piTick(phase + 50), independent ? &context : nullptr);
        const piTick absoluteStop = child->GetStopTime();
        update(phase + 80);
        const piTick later = child->GetTimelineElapsed(epoch + piTick(phase + 80), independent ? &context : nullptr);
        const bool consumed = child->GetAnimKey(Layer::AnimProperty::Action, 0)->mValue.mBool;
        const bool correct = before == piTick(40) && held == piTick(39) && later == piTick(39) &&
            absoluteStop == epoch + piTick(phase + 49) && child->IsStoppedByAction() &&
            (independent ? !consumed && context.mConsumedActions.size() == 1 : consumed);
        std::printf("[IMM_NESTED_STOP_DOMAIN_TEST] independent=%d laterCycle=%d before=%lld held=%lld later=%lld stopRelative=%lld expectedHeld=39 expectedStop=%lld sourceConsumed=%d sessionConsumed=%zu\n",
            independent ? 1 : 0, laterCycle ? 1 : 0, piTick::CastInt(before), piTick::CastInt(held), piTick::CastInt(later),
            piTick::CastInt(absoluteStop - epoch), static_cast<long long>(phase + 49), consumed ? 1 : 0, context.mConsumedActions.size());
        if (!independent) { player.Exit(); player.Deinit(); }
        sequence.Deinit(log);
        return correct;
    }

    void ObserveRootControls(piLog * log, bool sequential, bool zeroDuration = false)
    {
        Sequence sequence;
        RequireTimeline(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "root observation sequence");
        sequence.SetInitialSpawnArea(nullptr);
        Layer * root = sequence.GetRoot();
        RequireTimeline(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
            1.0f, true, piTick(zeroDuration ? 0 : 100), 1, UINT32_MAX, log), "root observation root");
        AddBool(root, Layer::AnimProperty::Loop, 0, false);
        AddBool(root, Layer::AnimProperty::Loop, 50, true);
        AddBool(root, Layer::AnimProperty::Loop, 150, false);
        Layer::AnimValue opacity{};
        opacity.mFloat = 0.1f;
        RequireTimeline(root->AddKey(piTick(0), Layer::AnimProperty::Opacity, opacity,
            Layer::InterpolationType::None), "root observation initial opacity");
        opacity.mFloat = 0.9f;
        RequireTimeline(root->AddKey(piTick(50), Layer::AnimProperty::Opacity, opacity,
            Layer::InterpolationType::None), "root observation late opacity");
        ImmPlayer::MngrPlayer player;
        RequireTimeline(player.Init(log), "root observation player");
        player.Enter(&sequence, piTick(1000000));
        if (sequential)
            for (int64_t tick : { 0, 49, 50, 99 })
                player.Update(piTick(1000000 + tick));
        player.Update(piTick(1000100));
        RequireTimeline(root->GetMaxRepeatCount() == 1 && root->GetEvaluatedMaxRepeatCount() == 0 &&
            std::fabs(root->GetOpacity() - (zeroDuration ? 0.9f : 0.1f)) < 0.000001f,
            "root unwrapped Loop control is deterministic at repeat boundary, with zero duration remaining unwrapped");
        player.SetTime(piTick(1000200), piTick(150), piTick(0));
        player.Update(piTick(1000200));
        RequireTimeline(root->GetEvaluatedMaxRepeatCount() == 1 && std::fabs(root->GetOpacity() - 0.9f) < 0.000001f,
            "root Loop control beyond content duration disables wrapping without being reselected by its own clamp");
        player.SetTime(piTick(1000300), piTick(25), piTick(0));
        player.Update(piTick(1000300));
        RequireTimeline(root->GetEvaluatedMaxRepeatCount() == 1 && std::fabs(root->GetOpacity() - 0.1f) < 0.000001f,
            "root backward seek resolves earlier Loop control");
        std::printf("[IMM_TIMELINE_REPEAT_TEST] root mode=%s zeroDuration=%d boundary/backseek/unwrapped-control passed nativeContract=1\n",
            sequential ? "sequential" : "cold", zeroDuration ? 1 : 0);
        player.Exit();
        player.Deinit();
        sequence.Deinit(log);
    }

    bool CheckCanonicalTimelineRepeat(piLog * log, bool nested)
    {
        Sequence sequence;
        RequireTimeline(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "canonical sequence");
        sequence.SetInitialSpawnArea(nullptr);
        Layer * root = sequence.GetRoot();
        RequireTimeline(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
            1.0f, true, piTick(1000), nested ? 0 : 2, UINT32_MAX, log), "canonical root");
        Layer * timeline = root;
        if (nested)
        {
            timeline = sequence.CreateLayer(root);
            RequireTimeline(timeline != nullptr && timeline->Init(Layer::Type::Group, L"Nested", true,
                trans3d::identity(), trans3d::identity(), 1.0f, true, piTick(100), 2, UINT32_MAX, log), "canonical nested timeline");
            AddBool(timeline, Layer::AnimProperty::Visibility, 0, true);
        }
        AddBool(timeline, Layer::AnimProperty::Loop, 10, true);
        ImmPlayer::MngrPlayer player;
        RequireTimeline(player.Init(log), "canonical player");
        player.Enter(&sequence, piTick(1000000));
        player.Update(piTick(1000020));
        const uint32_t active = timeline->GetMaxRepeatCount();
        const uint32_t activeEffective = timeline->GetEvaluatedMaxRepeatCount();
        player.SetTime(piTick(1000030), piTick(5), piTick(0));
        player.Update(piTick(1000030));
        const uint32_t beforeFirst = timeline->GetMaxRepeatCount();
        const uint32_t beforeFirstEffective = timeline->GetEvaluatedMaxRepeatCount();
        player.SetTime(piTick(1000040), piTick(20), piTick(0));
        player.Update(piTick(1000040));
        timeline->ReplaceAnimKeys(Layer::AnimProperty::Loop, {});
        player.Update(piTick(1000040));
        const uint32_t removed = timeline->GetMaxRepeatCount();
        const uint32_t removedEffective = timeline->GetEvaluatedMaxRepeatCount();
        std::printf("[IMM_TIMELINE_REPEAT_TEST] nested=%d canonical=2 active=%u beforeFirst=%u removed=%u\n",
            nested ? 1 : 0, active, beforeFirst, removed);
        player.Exit();
        player.Deinit();
        sequence.Deinit(log);
        return active == 2 && beforeFirst == 2 && removed == 2 &&
            activeEffective == 0 && beforeFirstEffective == 2 && removedEffective == 2;
    }

    void CheckFiniteBoundaries(piLog * log, uint32_t repeats, bool nested, bool keyed = false)
    {
        Sequence sequence;
        RequireTimeline(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "finite repeat sequence");
        sequence.SetInitialSpawnArea(nullptr);
        Layer * root = sequence.GetRoot();
        RequireTimeline(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
            1.0f, true, piTick(nested ? 1000 : 100), nested ? (keyed ? 0 : 1) : repeats, UINT32_MAX, log), "finite repeat root");
        Layer * timeline = root;
        if (nested)
        {
            timeline = sequence.CreateLayer(root);
            RequireTimeline(timeline != nullptr && timeline->Init(Layer::Type::Group, L"Nested", true,
                trans3d::identity(), trans3d::identity(), 1.0f, true, piTick(100), repeats, UINT32_MAX, log), "finite nested timeline");
            AddBool(timeline, Layer::AnimProperty::Visibility, 0, true);
        }
        if (keyed)
        {
            AddBool(timeline, Layer::AnimProperty::Loop, 50, false);
            AddBool(timeline, Layer::AnimProperty::Loop, 150, true);
            AddBool(timeline, Layer::AnimProperty::Loop, 250, false);
        }
        Layer * child = sequence.CreateLayer(timeline);
        RequireTimeline(child != nullptr && child->Init(Layer::Type::Group, L"Child", true,
            trans3d::identity(), trans3d::identity(), 1.0f, false, piTick(0), 1, UINT32_MAX, log), "finite child");
        Layer::AnimValue value{};
        value.mFloat = 0.1f;
        RequireTimeline(child->AddKey(piTick(0), Layer::AnimProperty::Opacity, value, Layer::InterpolationType::None), "finite first opacity");
        value.mFloat = 0.9f;
        RequireTimeline(child->AddKey(piTick(50), Layer::AnimProperty::Opacity, value, Layer::InterpolationType::None), "finite last opacity");
        ImmPlayer::MngrPlayer player;
        RequireTimeline(player.Init(log), "finite player");
        player.Enter(&sequence, piTick(1000000));
        for (int64_t tick : { 0, 49, 50, 99, 100, 101, 149, 150, 199, 200, 201, 249, 250, 300, 1000, 1050, 1200 })
        {
            player.Update(piTick(1000000 + tick));
            // Nested controls use parent time, including its later root wrap.
            const int64_t parentTime = keyed ? tick % 1000 : tick;
            const uint32_t effective = !keyed ? repeats : parentTime < 50 ? repeats :
                parentTime < 150 ? 1 : parentTime < 250 ? 0 : 1;
            // An infinite child still stops receiving time when its finite root
            // reaches the end of its full window. Root diagnostics stay raw.
            const int64_t effectiveTime = nested && !keyed ? std::min(parentTime, int64_t(1000)) : parentTime;
            const int64_t localTime = effective == 0 || effectiveTime / 100 < effective ? effectiveTime % 100 : 100;
            const float expected = localTime < 50 ? 0.1f : 0.9f;
            if (timeline->GetMaxRepeatCount() != repeats || timeline->GetEvaluatedMaxRepeatCount() != effective ||
                std::fabs(child->GetOpacity() - expected) >= 0.000001f)
                std::fprintf(stderr, "[IMM_FINITE_CLOCK_DIAGNOSTIC] repeats=%u nested=%d keyed=%d parent=%lld effective=%u expectedEffective=%u opacity=%f expected=%f\n",
                    repeats, nested, keyed, static_cast<long long>(parentTime), timeline->GetEvaluatedMaxRepeatCount(), effective,
                    child->GetOpacity(), expected);
            RequireTimeline(timeline->GetMaxRepeatCount() == repeats && timeline->GetEvaluatedMaxRepeatCount() == effective &&
                std::fabs(child->GetOpacity() - expected) < 0.000001f, "authored finite/infinite boundaries retain effective child evaluation");
        }
        if (keyed)
        {
            player.SetTime(piTick(1002000), piTick(25), piTick(0));
            player.Update(piTick(1002000));
            RequireTimeline(timeline->GetEvaluatedMaxRepeatCount() == repeats &&
                std::fabs(child->GetOpacity() - 0.1f) < 0.000001f, "nested backseek restores canonical child selection before first Loop key");
            player.SetTime(piTick(1003000), piTick(200), piTick(0));
            player.Update(piTick(1003000));
            RequireTimeline(timeline->GetEvaluatedMaxRepeatCount() == 0 && std::fabs(child->GetOpacity() - 0.1f) < 0.000001f,
                "nested keyed Loop controls descendant wrapping on direct seek");
            timeline->ReplaceAnimKeys(Layer::AnimProperty::Loop, {});
            player.Update(piTick(1003000));
            RequireTimeline(timeline->GetEvaluatedMaxRepeatCount() == repeats && std::fabs(child->GetOpacity() - 0.9f) < 0.000001f,
                "nested Loop removal restores finite descendant selection");
        }
        player.Exit();
        player.Deinit();
        sequence.Deinit(log);
    }

    void CheckRootActionMetadataRefresh(piLog* log)
    {
        Sequence sequence;
        RequireTimeline(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "metadata sequence");
        sequence.SetInitialSpawnArea(nullptr);
        Layer* root = sequence.GetRoot();
        RequireTimeline(root->Init(Layer::Type::Group, L"MetadataRoot", true, trans3d::identity(),
            trans3d::identity(), 1.0f, true, piTick(0), 0, UINT32_MAX, log), "metadata root");
        const auto addAction = [&](int64_t time, Layer::AnimAction action) {
            Layer::AnimValue value{};
            value.mInt = static_cast<uint32_t>(action);
            RequireTimeline(root->AddKey(piTick(time), Layer::AnimProperty::Action, value,
                Layer::InterpolationType::None), "metadata action");
        };
        addAction(20, Layer::AnimAction::Play);
        addAction(40, Layer::AnimAction::Play);
        addAction(60, Layer::AnimAction::Stop);
        addAction(90, Layer::AnimAction::Loop);
        Layer* soundLayer = sequence.CreateLayer(root);
        RequireTimeline(soundLayer && soundLayer->Init(Layer::Type::Sound, L"StorySound", true,
            trans3d::identity(), trans3d::identity(), 1.0f, false, piTick(0), 1, UINT32_MAX, log), "story sound layer");
        auto* sound = new LayerSound();
        LayerSound::AttenuationParameters attenuation{};
        LayerSound::ModifierParameters modifier{};
        RequireTimeline(sound->Init(false, false, 1.0f, attenuation, modifier, LayerSound::Type::Flat, 10.0f, log),
            "story sound implementation");
        soundLayer->SetImplementation(sound);
        soundLayer->SetLoaded(true);
        AddBool(soundLayer, Layer::AnimProperty::Visibility, 0, true);
        ImmPlayer::MngrPlayer player;
        RequireTimeline(player.Init(log), "metadata player");
        const piTick epoch(1000000);
        player.Enter(&sequence, epoch);
        player.Update(epoch + piTick(40));
        player.Pause(epoch + piTick(40));

        const auto refreshUnchanged = [&](piTick now) {
            const piTick start = root->GetStartTime(), stop = root->GetStopTime();
            const piTick elapsed = root->GetTimelineElapsed(now);
            const bool playing = root->GetPlaying(), paused = player.GetIsPaused();
            const bool waiting = player.GetIsWaiting(), finished = player.GetIsFinished();
            const Layer* initial = sequence.GetInitialSpawnArea();
            const int spawn = player.GetSpawnArea();
            std::vector<Layer::AnimKey> before;
            RequireTimeline(root->CopyAnimKeys(Layer::AnimProperty::Action, before), "metadata snapshot");
            player.RefreshRootActionMetadata(now);
            RequireTimeline(root->GetStartTime() == start && root->GetStopTime() == stop &&
                root->GetTimelineElapsed(now) == elapsed && root->GetPlaying() == playing &&
                player.GetIsPaused() == paused && player.GetIsWaiting() == waiting &&
                player.GetIsFinished() == finished && sequence.GetInitialSpawnArea() == initial &&
                player.GetSpawnArea() == spawn, "metadata refresh preserves clocks, holds and spawn selection");
            for (unsigned int i = 0; i < before.size(); ++i)
                RequireTimeline(root->GetAnimKey(Layer::AnimProperty::Action, i)->mValue.mBool == before[i].mValue.mBool,
                    "metadata refresh preserves action consumption");
        };
        refreshUnchanged(epoch + piTick(1000));
        RequireTimeline(player.GetHasPlays() && player.GetChapterCount() == 3 && player.GetCurrentChapter() == 2,
            "paused exact Play boundary belongs to new chapter, using held rather than wall clock");

        root->ReplaceAnimKeys(Layer::AnimProperty::Action, {});
        addAction(20, Layer::AnimAction::Stop);
        addAction(40, Layer::AnimAction::Stop);
        addAction(60, Layer::AnimAction::Stop);
        refreshUnchanged(epoch + piTick(1000));
        RequireTimeline(!player.GetHasPlays() && player.GetChapterCount() == 4 && player.GetCurrentChapter() == 1,
            "removing last Play selects Stop-only chapters, whose exact boundary remains prior chapter");

        root->ReplaceAnimKeys(Layer::AnimProperty::Action, {});
        addAction(200, Layer::AnimAction::Stop);
        refreshUnchanged(epoch + piTick(1000));
        player.Resume(epoch + piTick(1000));
        const piTick resumedStart = root->GetStartTime();
        player.Update(epoch + piTick(1060));
        RequireTimeline(root->GetStartTime() == resumedStart && root->GetPlaying() &&
            root->GetTimelineElapsed(epoch + piTick(1060)) == piTick(100) && !player.GetIsWaiting(),
            "removed last Loop does not rewind on next ordinary tick beyond its cached marker");

        player.Update(epoch + piTick(1160));
        RequireTimeline(player.GetIsWaiting() && root->IsStoppedByAction() &&
            root->GetTimelineElapsed(epoch + piTick(1160)) == piTick(199), "metadata authored Stop hold");
        RequireTimeline(sound->GetPlaying() && sound->GetPaused(), "authored root Stop pauses autonomous sound intent");
        refreshUnchanged(epoch + piTick(2000));
        RequireTimeline(player.GetCurrentChapter() == 0, "held Stop pose stays in prior Stop chapter");
        root->ReplaceAnimKeys(Layer::AnimProperty::Action, {});
        addAction(199, Layer::AnimAction::Play);
        addAction(200, Layer::AnimAction::Play);
        refreshUnchanged(epoch + piTick(2000));
        RequireTimeline(player.GetHasPlays() && player.GetChapterCount() == 3 && player.GetCurrentChapter() == 1,
            "action-held exact Play boundary refresh preserves waiting intent");
        const uint64_t heldOffset = sound->GetOffset();
        const bool heldRestart = sound->GetForceRestart();
        player.Continue(epoch + piTick(2000));
        RequireTimeline(!player.GetIsWaiting() && !player.GetIsPaused() && !sound->GetPaused() &&
            sound->GetPlaying() && sound->GetOffset() == heldOffset && sound->GetForceRestart() == heldRestart &&
            root->GetTimelineElapsed(epoch + piTick(2000)) == piTick(199),
            "Continue releases story sound pause without seek or changing held pose");
        addAction(250, Layer::AnimAction::Stop);
        player.RefreshRootActionMetadata(epoch + piTick(2000));
        player.Update(epoch + piTick(2051));
        RequireTimeline(player.GetIsWaiting() && sound->GetPaused() &&
            root->GetTimelineElapsed(epoch + piTick(2051)) == piTick(249), "second story hold pauses sound");
        player.Pause(epoch + piTick(2100));
        RequireTimeline(player.GetIsPaused() && sound->GetPaused(), "user pause retains story sound pause");
        player.Continue(epoch + piTick(3000));
        RequireTimeline(!player.GetIsPaused() && !player.GetIsWaiting() && !sound->GetPaused() && sound->GetPlaying() &&
            root->GetTimelineElapsed(epoch + piTick(3000)) == piTick(249),
            "Continue after user pause clears media pause while reanchoring authored hold");
        std::puts("[IMM_STORY_SOUND_PAUSE_TEST] stopPaused=1 continued=1 continuedAfterUserPause=1 stableOffset=1");
        player.Exit();
        player.Deinit();
        sequence.Deinit(log);
        std::puts("[IMM_ROOT_ACTION_METADATA_TEST] pausedBoundary=1 chapterMode=1 removedLoop=1 actionHold=1 preservedState=1");
    }

    void CheckHeldRootControl(piLog * log)
    {
        Sequence sequence;
        RequireTimeline(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "held repeat sequence");
        sequence.SetInitialSpawnArea(nullptr);
        Layer * root = sequence.GetRoot();
        RequireTimeline(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
            1.0f, true, piTick(100), 2, UINT32_MAX, log), "held repeat root");
        AddBool(root, Layer::AnimProperty::Loop, 0, false);
        AddBool(root, Layer::AnimProperty::Loop, 50, true);
        AddBool(root, Layer::AnimProperty::Loop, 150, false);
        Layer * nested = sequence.CreateLayer(root);
        RequireTimeline(nested != nullptr && nested->Init(Layer::Type::Group, L"Nested", true,
            trans3d::identity(), trans3d::identity(), 1.0f, true, piTick(40), 0, UINT32_MAX, log), "held nested timeline");
        AddBool(nested, Layer::AnimProperty::Visibility, 0, true);
        Layer * child = sequence.CreateLayer(nested);
        RequireTimeline(child != nullptr && child->Init(Layer::Type::Group, L"Child", true,
            trans3d::identity(), trans3d::identity(), 1.0f, false, piTick(0), 1, UINT32_MAX, log), "held nested child");
        Layer::AnimValue opacity{};
        opacity.mFloat = 0.1f;
        RequireTimeline(child->AddKey(piTick(0), Layer::AnimProperty::Opacity, opacity,
            Layer::InterpolationType::None), "held nested first opacity");
        opacity.mFloat = 0.9f;
        RequireTimeline(child->AddKey(piTick(20), Layer::AnimProperty::Opacity, opacity,
            Layer::InterpolationType::None), "held nested late opacity");
        Layer::AnimValue action{};
        action.mInt = static_cast<uint32_t>(Layer::AnimAction::Stop);
        RequireTimeline(root->AddKey(piTick(75), Layer::AnimProperty::Action, action, Layer::InterpolationType::None), "held Stop");
        ImmPlayer::MngrPlayer player;
        RequireTimeline(player.Init(log), "held player");
        const piTick epoch(1000000);
        player.Enter(&sequence, epoch);
        player.Update(epoch + piTick(50));
        player.Update(epoch + piTick(75));
        player.Update(epoch + piTick(1000));
        RequireTimeline(root->IsStoppedByAction() && root->GetTimelineElapsed(epoch + piTick(1000)) == piTick(74) &&
            root->GetEvaluatedMaxRepeatCount() == 0 && root->GetMaxRepeatCount() == 2 &&
            std::fabs(child->GetOpacity() - 0.9f) < 0.000001f,
            "root Loop control observes held chapter clock rather than advancing wall time");
        player.Continue(epoch + piTick(1000));
        player.Update(epoch + piTick(1050));
        RequireTimeline(!root->HasStopTime() && root->GetEvaluatedMaxRepeatCount() == 0 &&
            std::fabs(child->GetOpacity() - 0.9f) < 0.000001f,
            "Continue preserves active Loop control before later disable key");
        player.Update(epoch + piTick(1075));
        std::printf("[IMM_TIMELINE_REPEAT_TEST] continuedElapsed=%lld effective=%u canonical=%u\n",
            piTick::CastInt(root->GetTimelineElapsed(epoch + piTick(1075))),
            root->GetEvaluatedMaxRepeatCount(), root->GetMaxRepeatCount());
        RequireTimeline(root->GetTimelineElapsed(epoch + piTick(1075)) == piTick(149) &&
            root->GetEvaluatedMaxRepeatCount() == 0 && root->GetMaxRepeatCount() == 2,
            "Continue retains enabled Loop at tick149 immediately before disable key");
        player.Update(epoch + piTick(1076));
        std::printf("[IMM_TIMELINE_REPEAT_TEST] continuedElapsed=%lld effective=%u canonical=%u\n",
            piTick::CastInt(root->GetTimelineElapsed(epoch + piTick(1076))),
            root->GetEvaluatedMaxRepeatCount(), root->GetMaxRepeatCount());
        RequireTimeline(root->GetTimelineElapsed(epoch + piTick(1076)) == piTick(150) &&
            root->GetEvaluatedMaxRepeatCount() == 1 && root->GetMaxRepeatCount() == 2,
            "Continue reaches later unwrapped Loop control without changing authored repeat count");
        player.Exit();
        player.Deinit();
        sequence.Deinit(log);
    }
    void CheckHeldPlaybackControlRefresh(piLog * log, bool independent, bool actionHold)
    {
        Sequence sequence;
        RequireTimeline(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "refresh sequence");
        sequence.SetInitialSpawnArea(nullptr);
        Layer * root = sequence.GetRoot();
        RequireTimeline(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
            1.0f, true, piTick(1000), 0, UINT32_MAX, log), "refresh root");
        Layer * timeline = sequence.CreateLayer(root);
        RequireTimeline(timeline && timeline->Init(Layer::Type::Group, L"Timeline", true,
            trans3d::identity(), trans3d::identity(), 1.0f, true, piTick(100), 0, UINT32_MAX, log), "refresh timeline");
        Layer * leaf = sequence.CreateLayer(timeline);
        RequireTimeline(leaf && leaf->Init(Layer::Type::Group, L"Pose", true, trans3d::identity(),
            trans3d::identity(), 1.0f, false, piTick(0), 1, UINT32_MAX, log), "refresh pose");
        for (Layer * layer : { root, timeline, leaf }) AddBool(layer, Layer::AnimProperty::Visibility, 0, true);
        Layer::AnimValue value; value.init();
        RequireTimeline(leaf->AddKey(piTick(0), Layer::AnimProperty::Transform, value,
            Layer::InterpolationType::Linear), "refresh first pose");
        value.mTransform.mTranslation.x = 5.0;
        RequireTimeline(leaf->AddKey(piTick(50), Layer::AnimProperty::Transform, value,
            Layer::InterpolationType::Linear), "refresh last pose");
        if (actionHold)
        {
            value.init(); value.mInt = static_cast<uint32_t>(Layer::AnimAction::Stop);
            RequireTimeline(timeline->AddKey(piTick(20), Layer::AnimProperty::Action, value,
                Layer::InterpolationType::None), "refresh Stop");
        }
        const piTick epoch(1000000), initial = epoch + piTick(1135), now = epoch + piTick(1200);
        root->SetStartTime(epoch); root->SetPlaying(true);
        Layer::EvaluationContext context;
        context.mBoundary = root; context.mBoundaryStartTime = epoch; context.mBoundaryElapsed = piTick(1135);
        context.mBoundaryDuration = piTick(1000); context.mBoundaryRepeatCount = 0; context.mHeld = true;
        Layer::EvaluationContext * scope = independent ? &context : nullptr;
        if (independent) { root->SetStopTime(epoch + piTick(2), true, true); root->SetPlaying(false); }
        else root->SetStateAt(initial, log);
        timeline->SetStateAt(initial, log, scope); leaf->SetStateAt(initial, log, scope);
        const double expected = actionHold ? 1.9 : 3.5;
        RequireTimeline(std::fabs(leaf->CapturePlaybackState().mTransform.mTranslation.x - expected) < 1e-9, "initial held pose");
        if (!independent)
        {
            root->SetStopTime(initial); root->SetPlaying(false);
            timeline->SetStopTime(timeline->GetStartTime() + timeline->GetTimelineElapsed(initial), true, timeline->IsStoppedByAction());
            timeline->SetPlaying(false);
        }
        const piTick mainElapsed = root->GetTimelineElapsed(now);
        const size_t consumed = context.mConsumedActions.size();
        const auto refresh = [&]() {
            timeline->ReevaluatePlaybackAt(now, log, !independent, scope);
            leaf->ReevaluatePlaybackAt(now, log, !independent, scope);
            RequireTimeline(root->GetTimelineElapsed(now) == mainElapsed, "refresh preserves held main clock");
            RequireTimeline(context.mConsumedActions.size() == consumed, "refresh does not consume Actions");
        };
        value.init(); value.mInt = 20;
        RequireTimeline(timeline->AddKey(piTick(0), Layer::AnimProperty::Offset, value,
            Layer::InterpolationType::None), "refresh Offset");
        refresh();
        RequireTimeline(std::fabs(leaf->CapturePlaybackState().mTransform.mTranslation.x - (actionHold ? 3.9 : 5.0)) < 1e-9, "Offset refresh retimes held children");
        timeline->ReplaceAnimKeys(Layer::AnimProperty::Offset, {}); refresh();
        RequireTimeline(std::fabs(leaf->CapturePlaybackState().mTransform.mTranslation.x - expected) < 1e-9, "Offset removal restores child pose");
        AddBool(timeline, Layer::AnimProperty::Loop, 0, false); refresh();
        RequireTimeline(std::fabs(leaf->CapturePlaybackState().mTransform.mTranslation.x - (actionHold ? 1.9 : 5.0)) < 1e-9, "Loop refresh clamps held children");
        timeline->ReplaceAnimKeys(Layer::AnimProperty::Loop, {}); refresh();
        RequireTimeline(std::fabs(leaf->CapturePlaybackState().mTransform.mTranslation.x - expected) < 1e-9, "Loop removal restores authored repeat");
        if (actionHold)
        {
            timeline->ReplaceAnimKeys(Layer::AnimProperty::Visibility, {});
            AddBool(timeline, Layer::AnimProperty::Visibility, 10, true); refresh();
            RequireTimeline(std::fabs(leaf->CapturePlaybackState().mTransform.mTranslation.x - 0.9) < 1e-9,
                "visibility resampling preserves Stop in its parent time domain");
            timeline->ReplaceAnimKeys(Layer::AnimProperty::Visibility, {});
            AddBool(timeline, Layer::AnimProperty::Visibility, 0, true); refresh();
        }
        if (!independent)
        {
            AddBool(root, Layer::AnimProperty::Loop, 0, false);
            root->ReevaluatePlaybackAt(now, log, true); refresh();
            RequireTimeline(std::fabs(leaf->CapturePlaybackState().mTransform.mTranslation.x - (actionHold ? 1.9 : 0.0)) < 1e-9, "Root Loop refresh realigns the last legal cycle");
            root->ReplaceAnimKeys(Layer::AnimProperty::Loop, {});
            root->ReevaluatePlaybackAt(now, log, true); refresh();
            RequireTimeline(std::fabs(leaf->CapturePlaybackState().mTransform.mTranslation.x - expected) < 1e-9, "Root Loop removal restores the held pose");
        }
        sequence.Deinit(log);
    }
}

void CheckTimelineRepeatKeys()
{
    ImmCore::piLog log;
    RequireTimeline(log.Init(L"artifacts/timeline-repeat-keys.log", PILOG_TXT), "timeline repeat log");
    for (bool independent : { false, true })
        for (bool actionHold : { false, true }) CheckHeldPlaybackControlRefresh(&log, independent, actionHold);
    ObserveRootControls(&log, false);
    ObserveRootControls(&log, true);
    ObserveRootControls(&log, false, true);
    ObserveRootControls(&log, true, true);
    const bool root = CheckCanonicalTimelineRepeat(&log, false);
    const bool nested = CheckCanonicalTimelineRepeat(&log, true);
    for (uint32_t repeats : { 0u, 1u, 2u })
    {
        CheckFiniteBoundaries(&log, repeats, false);
        CheckFiniteBoundaries(&log, repeats, true);
    }
    CheckFiniteBoundaries(&log, 2, true, true);
    CheckHeldRootControl(&log);
    CheckRootActionMetadataRefresh(&log);
    const bool nestedStopMain = CheckNestedStopDomain(&log, false);
    const bool nestedStopIndependent = CheckNestedStopDomain(&log, true);
    const bool laterStopMain = CheckNestedStopDomain(&log, false, true);
    const bool laterStopIndependent = CheckNestedStopDomain(&log, true, true);
    log.End();
    RequireTimeline(nestedStopMain && nestedStopIndependent && laterStopMain && laterStopIndependent,
        "nested Stop key must hold one parent tick before the action, accounting for visibility start/Offset");
    RequireTimeline(root && nested, "Loop evaluation must preserve authored root/group repeat counts before keys and after removal");
    std::puts("[IMM_TIMELINE_REPEAT_TEST] canonical root/group repeat metadata preserved");
}
