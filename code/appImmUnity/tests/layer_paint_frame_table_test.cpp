#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "libImmImporter/src/document/layerPaintStatic.h"
#include "libImmImporter/src/document/layerPaintPretessellated.h"
#include "libImmImporter/src/document/layer.h"
#include "libImmImporter/src/document/sequence.h"
#include "libImmImporter/src/document/layerSound.h"
#include "libImmImporter/src/document/layerSpawnArea.h"
#include "libImmPlayer/src/mngrPlayer.h"

namespace
{
    void requireFrames(bool condition, const char * message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "[IMM_PAINT_FRAME_TABLE_TEST] %s\n", message);
            std::exit(1);
        }
    }
}

template<typename Paint>
static void CheckLayerDrivenPlaybackKeys()
{
    using namespace ImmCore;
    using namespace ImmImporter;
    piLog log;
    requireFrames(log.Init(L"artifacts/paint-playback-keys.log", PILOG_TXT), "playback key test log");
    Sequence sequence;
    requireFrames(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "playback key sequence");
    Layer * root = sequence.GetRoot();
    requireFrames(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
        1.0f, true, piTick(0), 1, UINT32_MAX, &log), "playback key root");
    root->SetStartTime(piTick(0));
    root->SetPlaying(true);
    Layer * layer = sequence.CreateLayer(root);
    requireFrames(layer != nullptr && layer->Init(Layer::Type::Paint, L"Paint", true,
        trans3d::identity(), trans3d::identity(), 1.0f, false, piTick(0), 1, UINT32_MAX, &log), "playback key paint");
    auto * paint = new Paint();
    requireFrames(paint->Init(3, 3, 0, 30, 1), "playback key implementation");
    for (int index = 0; index < 3; ++index)
    {
        requireFrames(paint->GetDrawing(index)->Init(0), "playback key drawing");
        paint->GetFrameBuffer()[index] = index;
    }
    layer->SetImplementation(paint);
    layer->SetLoaded(true);
    Layer::AnimValue value{};
    value.mBool = true;
    requireFrames(layer->AddKey(piTick(0), Layer::AnimProperty::Visibility, value,
        Layer::InterpolationType::None), "playback visibility key");
    value.mBool = false;
    requireFrames(layer->AddKey(piTick::FromFrames(4, 30), Layer::AnimProperty::Loop, value,
        Layer::InterpolationType::None), "playback loop key");
    layer->SetStateAt(piTick::FromFrames(7, 30), &log);
    requireFrames(paint->GetCurrentDrawing() == paint->GetDrawing(2), "active finite Loop clamps paint");
    requireFrames(paint->GetMaxRepeatCount() == 0, "Loop evaluation preserves canonical repeat metadata");
    layer->SetStateAt(piTick::FromFrames(3, 30), &log);
    requireFrames(paint->GetCurrentDrawing() == paint->GetDrawing(0), "seek before Loop restores canonical repeat");
    layer->SetStateAt(piTick::FromFrames(7, 30), &log);
    layer->ReplaceAnimKeys(Layer::AnimProperty::Loop, {});
    layer->SetStateAt(piTick::FromFrames(7, 30), &log);
    requireFrames(paint->GetCurrentDrawing() == paint->GetDrawing(1), "removing Loop restores canonical repeat");

    paint->SetMaxRepeatCount(2);
    value.mBool = true;
    requireFrames(layer->AddKey(piTick::FromFrames(8, 30), Layer::AnimProperty::Loop, value,
        Layer::InterpolationType::None), "finite canonical repeat loop key");
    layer->SetStateAt(piTick::FromFrames(10, 30), &log);
    requireFrames(paint->GetMaxRepeatCount() == 2 && paint->GetCurrentDrawing() == paint->GetDrawing(1),
        "Loop true overrides finite repeat without changing its canonical count");
    layer->SetStateAt(piTick::FromFrames(7, 30), &log);
    requireFrames(paint->GetMaxRepeatCount() == 2 && paint->GetCurrentDrawing() == paint->GetDrawing(2),
        "seek before Loop restores the authored finite repeat clamp");
    layer->SetStateAt(piTick::FromFrames(10, 30), &log);
    layer->ReplaceAnimKeys(Layer::AnimProperty::Loop, {});
    layer->SetStateAt(piTick::FromFrames(10, 30), &log);
    requireFrames(paint->GetMaxRepeatCount() == 2 && paint->GetCurrentDrawing() == paint->GetDrawing(2),
        "removing Loop restores the authored finite repeat clamp");
    paint->SetMaxRepeatCount(0);

    value.mInt = static_cast<uint32_t>(piTick::CastInt(piTick::FromFrames(1, 30)));
    requireFrames(layer->AddKey(piTick::FromFrames(4, 30), Layer::AnimProperty::Offset, value,
        Layer::InterpolationType::None), "playback offset key");
    layer->SetStateAt(piTick::FromFrames(5, 30), &log);
    requireFrames(paint->GetCurrentDrawing() == paint->GetDrawing(0), "active Offset applies on its first evaluation");
    layer->SetStateAt(piTick::FromFrames(3, 30), &log);
    requireFrames(paint->GetCurrentDrawing() == paint->GetDrawing(0), "seek before Offset resets local-time offset");
    layer->SetStateAt(piTick::FromFrames(5, 30), &log);
    layer->ReplaceAnimKeys(Layer::AnimProperty::Offset, {});
    layer->SetStateAt(piTick::FromFrames(5, 30), &log);
    requireFrames(paint->GetCurrentDrawing() == paint->GetDrawing(2), "removing Offset resets local-time offset");
    sequence.Deinit(&log);
    log.End();
}

static void CheckLayerDrivenSoundOffsets()
{
    using namespace ImmCore;
    using namespace ImmImporter;
    piLog log;
    requireFrames(log.Init(L"artifacts/sound-offset-keys.log", PILOG_TXT), "sound offset test log");
    Sequence sequence;
    requireFrames(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "sound offset sequence");
    sequence.SetInitialSpawnArea(nullptr);
    Layer * root = sequence.GetRoot();
    requireFrames(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
        1.0f, true, piTick(1000), 0, UINT32_MAX, &log), "sound offset looping root");
    Layer * layer = sequence.CreateLayer(root);
    requireFrames(layer != nullptr && layer->Init(Layer::Type::Sound, L"Sound", true,
        trans3d::identity(), trans3d::identity(), 1.0f, false, piTick(0), 1, UINT32_MAX, &log), "sound offset layer");
    auto * sound = new LayerSound();
    LayerSound::AttenuationParameters attenuation{};
    LayerSound::ModifierParameters modifier{};
    requireFrames(sound->Init(false, false, 1.0f, attenuation, modifier, LayerSound::Type::Flat, 10.0f, &log),
        "sound offset implementation");
    layer->SetImplementation(sound);
    Layer::AnimValue value{};
    value.mBool = true;
    requireFrames(layer->AddKey(piTick(0), Layer::AnimProperty::Visibility, value,
        Layer::InterpolationType::None), "sound offset visibility");
    value.mInt = 100;
    requireFrames(layer->AddKey(piTick(10), Layer::AnimProperty::Offset, value,
        Layer::InterpolationType::None), "sound active offset");
    ImmPlayer::MngrPlayer player;
    requireFrames(player.Init(&log), "sound offset player");
    const piTick epoch(1000000);
    player.Enter(&sequence, epoch);
    player.Update(epoch + piTick(20));
    requireFrames(sound->GetPlaying() && sound->GetOffset() == piTick::ToMicroSeconds(piTick(120)),
        "cold sound span uses currently active Offset immediately");
    const auto seek = [&](int64_t tick) {
        player.SetTime(epoch, piTick(tick), piTick(0));
        player.Update(epoch);
        requireFrames(sound->GetPlaying() && sound->GetForceRestart(), "sound seek preserves explicit restart request");
        sound->SetForceRestart(false); // Simulate audio renderer consuming the explicit seek request.
    };
    seek(5);
    requireFrames(sound->GetOffset() == piTick::ToMicroSeconds(piTick(5)), "sound seek before first Offset restores zero");
    seek(20);
    requireFrames(sound->GetOffset() == piTick::ToMicroSeconds(piTick(120)), "sound seek restores active Offset");
    layer->ReplaceAnimKeys(Layer::AnimProperty::Offset, {});
    seek(20);
    requireFrames(sound->GetOffset() == piTick::ToMicroSeconds(piTick(20)), "sound removed Offset restores zero");
    std::puts("[IMM_SOUND_OFFSET_TEST] cold/backseek/removal passed; metadata only, no audio engine");

    value.mBool = true;
    requireFrames(layer->AddKey(piTick(20), Layer::AnimProperty::Visibility, value,
        Layer::InterpolationType::None), "sound second visibility span");
    value.mInt = 100;
    requireFrames(layer->AddKey(piTick(10), Layer::AnimProperty::Offset, value,
        Layer::InterpolationType::None), "sound initial offset");
    value.mInt = 0;
    requireFrames(layer->AddKey(piTick(30), Layer::AnimProperty::Offset, value,
        Layer::InterpolationType::None), "sound decreasing midspan offset");
    seek(0);
    player.Update(epoch + piTick(10));
    player.Update(epoch + piTick(20));
    const piTick originalStart = layer->GetStartTime();
    const uint64_t originalOffset = sound->GetOffset();
    player.Update(epoch + piTick(29));
    requireFrames(layer->GetStartTime() == originalStart && sound->GetOffset() == originalOffset,
        "sound remains in its initial visibility span before Offset boundary");
    player.Update(epoch + piTick(30));
    const bool stableAtBoundary = layer->GetStartTime() == originalStart && sound->GetOffset() == originalOffset;
    player.Update(epoch + piTick(31));
    const bool stableAfterBoundary = layer->GetStartTime() == originalStart && sound->GetOffset() == originalOffset;
    const bool noForceRestart = !sound->GetForceRestart();
    player.Update(epoch + piTick(1000));
    const bool actualWrap = layer->GetStartTime() == epoch + piTick(1000) &&
        sound->GetOffset() == 0 && !sound->GetForceRestart();
    std::printf("[IMM_SOUND_OFFSET_TEST] midspanBoundaryStable=%d midspanNextTickStable=%d forceRestartUnchanged=%d actualWrapRetrigger=%d\n",
        stableAtBoundary ? 1 : 0, stableAfterBoundary ? 1 : 0, noForceRestart ? 1 : 0, actualWrap ? 1 : 0);
    player.Exit();
    player.Deinit();
    sequence.Deinit(&log);
    log.End();
    requireFrames(actualWrap, "actual looping ancestor wrap retriggers sound with active Offset");
    requireFrames(stableAtBoundary && stableAfterBoundary && noForceRestart,
        "decreasing midspan Offset alone must not masquerade as an ancestor wrap");
}

static void CheckHiddenLayerRenderEligibility()
{
    using namespace ImmCore;
    using namespace ImmImporter;
    piLog log;
    requireFrames(log.Init(L"artifacts/hidden-render-eligibility.log", PILOG_TXT), "visibility eligibility log");
    Sequence sequence;
    requireFrames(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "visibility eligibility sequence");
    Layer * root = sequence.GetRoot();
    requireFrames(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
        1, true, piTick(100), 0, UINT32_MAX, &log), "visibility eligibility root");
    root->SetPlaying(true); root->SetStartTime(piTick(0));
    Layer * hidden = sequence.CreateLayer(root);
    requireFrames(hidden && hidden->Init(Layer::Type::Group, L"InitiallyHidden", false,
        trans3d::identity(), trans3d::identity(), 1, false, piTick(0), 1, UINT32_MAX, &log), "initial hidden group");
    const auto saved = hidden->CapturePlaybackState();
    Layer::AnimValue visible; visible.init(); visible.mBool = true;
    std::vector<Layer::AnimKey> keys;
    requireFrames(Layer::SetAnimKey(keys, piTick(0), visible, Layer::InterpolationType::None), "revealing visibility key");
    hidden->ReplaceAnimKeys(Layer::AnimProperty::Visibility, std::move(keys));
    hidden->ReevaluateVisibilityAt(piTick(50), &log);
    requireFrames(hidden->GetWorldVisible() && hidden->GetPotentiallyVisible(),
        "accepted visibility key must pass both renderer visibility gates");
    hidden->RestorePlaybackState(saved);
    hidden->ReevaluateVisibilityAt(piTick(50), &log);
    requireFrames(hidden->GetWorldVisible() && hidden->GetPotentiallyVisible(),
        "restored runtime capsule must not suppress a retained revealing key after reevaluation");
    hidden->ReplaceAnimKeys(Layer::AnimProperty::Visibility, {});
    hidden->SetPotentiallyVisible(false); hidden->SetCanonicalVisible(true);
    requireFrames(hidden->GetVisible() && hidden->GetPotentiallyVisible(), "base visibility reveal is render eligible");
    hidden->SetVisible(false); hidden->SetPotentiallyVisible(false); hidden->SetVisibilityOverride(true, true);
    requireFrames(hidden->GetVisible() && hidden->GetPotentiallyVisible(), "runtime override reveal is render eligible");
    sequence.Deinit(&log); log.End();
    std::puts("[IMM_VISIBILITY_ELIGIBILITY_TEST] initialHiddenReveal=1 retainedKeysAfterRestore=1 baseAndOverride=1");
}

static void CheckIndependentEvaluationContext()
{
    using namespace ImmCore;
    using namespace ImmImporter;
    piLog log;
    requireFrames(log.Init(L"artifacts/paint-evaluation-context.log", PILOG_TXT), "context log");
    Sequence sequence;
    requireFrames(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "context sequence");
    sequence.SetInitialSpawnArea(nullptr);
    sequence.SetSpawnAreaNeedsUpdate(false);
    Layer * root = sequence.GetRoot();
    trans3d outside = trans3d::identity(); outside.mTranslation.x = 100;
    requireFrames(root->Init(Layer::Type::Group, L"Root", false, outside, trans3d::identity(),
        0.2f, true, piTick(100), 1, UINT32_MAX, &log), "context root");
    root->SetStartTime(piTick(0)); root->SetStopTime(piTick(2), true, true); root->SetPlaying(false);
    const auto make = [&](Layer::Type type, Layer * parent, const wchar_t * name, bool visible,
        float opacity, bool timeline) {
        Layer * layer = sequence.CreateLayer(parent);
        requireFrames(layer && layer->Init(type, name, visible, trans3d::identity(), trans3d::identity(),
            opacity, timeline, piTick::FromFrames(3, 30), 0, UINT32_MAX, &log), "context layer");
        return layer;
    };
    const auto add = [&](Layer * layer, Layer::AnimProperty property, piTick time, const Layer::AnimValue & value) {
        requireFrames(layer->AddKey(time, property, value, Layer::InterpolationType::None), "context key");
    };
    Layer * boundary = make(Layer::Type::Group, root, L"Boundary", false, 0.1f, true);
    trans3d placement = trans3d::identity(); placement.mTranslation.x = 50;
    boundary->SetCanonicalTransform(placement);
    Layer::AnimValue value; value.init(); value.mBool = false;
    add(boundary, Layer::AnimProperty::Visibility, piTick(0), value);
    value.mTransform.mTranslation.x = 777;
    add(boundary, Layer::AnimProperty::Transform, piTick(0), value);
    add(boundary, Layer::AnimProperty::Loop, piTick(0), value);
    value.mInt = static_cast<uint32_t>(Layer::AnimAction::Stop);
    add(boundary, Layer::AnimProperty::Action, piTick(0), value);
    Layer * paintLayer = make(Layer::Type::Paint, boundary, L"Paint", true, 0.5f, false);
    auto * paint = new LayerPaintStatic();
    requireFrames(paint->Init(3, 3, 0, 30, 1), "context paint");
    for (uint32_t index = 0; index < 3; ++index)
    {
        requireFrames(paint->GetDrawing(index)->Init(0), "context drawing");
        paint->GetFrameBuffer()[index] = index;
    }
    paintLayer->SetImplementation(paint);
    value.init(); value.mBool = true;
    add(paintLayer, Layer::AnimProperty::Visibility, piTick(0), value);
    value.mFloat = 0.6f;
    add(paintLayer, Layer::AnimProperty::Opacity, piTick::FromFrames(1, 30), value);
    value.mTransform.mTranslation.x = 2;
    add(paintLayer, Layer::AnimProperty::Transform, piTick::FromFrames(1, 30), value);
    Layer::EvaluationContext context;
    context.mBoundary = boundary; context.mBoundaryStartTime = piTick(1000000);
    context.mBoundaryElapsed = piTick::FromFrames(4, 30);
    context.mBoundaryDuration = boundary->GetDuration(); context.mBoundaryRepeatCount = boundary->GetMaxRepeatCount();
    context.mHeld = true;
    const piTick wallTime = context.mBoundaryStartTime + piTick(99999);
    boundary->SetStateAt(wallTime, &log, &context);
    boundary->ReevaluateTransformAt(wallTime, &log, &context);
    requireFrames(!boundary->GetVisible() && boundary->GetTransform().mTranslation.x == 50 &&
        !boundary->GetAnimKey(Layer::AnimProperty::Action, 0)->mValue.mBool && context.mConsumedActions.empty(),
        "boundary parent-domain keys are excluded");
    paintLayer->SetStateAt(wallTime, &log, &context);
    requireFrames(paint->GetCurrentDrawing() == paint->GetDrawing(1) && paintLayer->GetTimeline(&context) == boundary &&
        boundary->GetTimelineElapsed(wallTime, &context) == piTick::FromFrames(4, 30),
        "virtual held clock wraps independently of the original root hold");
    paintLayer->ReevaluateTransformAt(wallTime, &log, &context);
    paintLayer->ReevaluateOpacityAt(wallTime, &log, &context);
    requireFrames(!paintLayer->GetWorldVisible() && paintLayer->GetWorldVisible(&context) &&
        std::fabs(paintLayer->GetWorldOpacity(&context) - 0.6f) < 0.000001f &&
        paintLayer->GetTransformToWorld(&context).mTranslation.x == 2 &&
        paintLayer->GetTransformToWorld().mTranslation.x == 152,
        "scope excludes outside and boundary visibility, opacity and placement");
    Layer * inner = make(Layer::Type::Group, boundary, L"Inner", true, 1.0f, true);
    value.init(); value.mBool = true;
    add(inner, Layer::AnimProperty::Visibility, piTick(0), value);
    // Main playback has already consumed this action; preview must still fire
    // its own action and retain the source's original consumed flag.
    value.mInt = static_cast<uint32_t>(Layer::AnimAction::Stop);
    add(inner, Layer::AnimProperty::Action, piTick::FromFrames(1, 30), value);
    const bool sourceStopConsumed = inner->GetAnimKey(Layer::AnimProperty::Action, 0)->mValue.mBool;
    inner->ReevaluateOpacityAt(wallTime, &log, &context);
    requireFrames(context.mConsumedActions.empty(), "property-only evaluation never consumes actions");
    inner->SetStateAt(wallTime, &log, &context);
    inner->SetStateAt(wallTime + piTick(100), &log, &context);
    requireFrames(inner->IsStoppedByAction() && context.mConsumedActions.size() == 1 &&
        inner->GetAnimKey(Layer::AnimProperty::Action, 0)->mValue.mBool == sourceStopConsumed &&
        inner->GetTimelineElapsed(wallTime, &context) == piTick::FromFrames(1, 30) - piTick(1),
        "descendant Stop holds use session-local action consumption");
    Layer * spawn = make(Layer::Type::SpawnArea, boundary, L"Spawn", true, 1.0f, false);
    auto * spawnImpl = new LayerSpawnArea();
    LayerSpawnArea::Volume volume{}; volume.mType = LayerSpawnArea::Volume::Type::Sphere;
    volume.mShape.mSphere = vec4(0, 0, 0, 1);
    requireFrames(spawnImpl->Init(volume, LayerSpawnArea::TrackingLevel::Floor), "context spawn");
    spawn->SetImplementation(spawnImpl);
    value.init(); value.mInt = static_cast<uint32_t>(Layer::AnimAction::MakeDefault);
    add(spawn, Layer::AnimProperty::Action, piTick(0), value);
    spawn->SetStateAt(wallTime, &log, &context);
    requireFrames(context.mDefaultSpawnAreaLayerId == spawn->GetID() && sequence.GetInitialSpawnArea() == nullptr &&
        !sequence.GetSpawnAreaNeedsUpdate() && !spawn->GetAnimKey(Layer::AnimProperty::Action, 0)->mValue.mBool,
        "preview default-spawn action never changes source navigation or flags");
    Layer * soundLayer = make(Layer::Type::Sound, boundary, L"Sound", true, 1.0f, false);
    auto * sound = new LayerSound();
    LayerSound::AttenuationParameters attenuation{}; LayerSound::ModifierParameters modifier{};
    requireFrames(sound->Init(false, false, 1.0f, attenuation, modifier, LayerSound::Type::Flat, 10.0f, &log), "context sound");
    soundLayer->SetImplementation(sound); sound->SetPlaying(false); sound->SetPaused(true); sound->SetVolume(0.25f);
    value.init(); value.mBool = true;
    add(soundLayer, Layer::AnimProperty::Visibility, piTick(0), value);
    soundLayer->SetStateAt(wallTime, &log, &context);
    soundLayer->ReevaluateOpacityAt(wallTime, &log, &context);
    requireFrames(!sound->GetPlaying() && sound->GetPaused() && sound->GetVolume() == 0.25f,
        "context defaults cannot start, resume or change shared sound voices");
    root->ReevaluateVisibilityAt(wallTime, &log, &context);
    requireFrames(!root->GetVisible() && root->GetStopTime() == piTick(2) && root->IsStoppedByAction() &&
        !root->GetWorldVisible(&context), "outside state and main root hold stay unchanged");
    inner->SetVisibilityOverride(true, false);
    inner->SetOpacityOverride(true, 0.3f);
    inner->SetTransformOverride(true, placement);
    inner->ResetActionStatesAt(piTick::FromFrames(2, 30));
    const auto savedRoot = root->CapturePlaybackState();
    const auto savedInner = inner->CapturePlaybackState();
    paint->SetTime(piTick::FromFrames(4, 30)); // Same old frame index1, retained time4.
    paint->SetPlaying(false);
    const auto savedPaint = paint->CapturePlaybackState();
    root->ResetPlaybackStateForEvaluation();
    inner->ResetPlaybackStateForEvaluation();
    const auto resetInner = inner->CapturePlaybackState();
    requireFrames(!resetInner.mIsPlaying && !resetInner.mHasStopTime && !resetInner.mHasActiveSpanClockOrigin &&
        resetInner.mOpacity == inner->GetCanonicalOpacity() && resetInner.mVisibilityOverrideEnabled &&
        inner->GetAnimKey(Layer::AnimProperty::Action, 0)->mValue.mBool,
        "evaluation reset leaves overrides and consumed source actions intact");
    inner->SetCanonicalOpacity(0.8f); // A later owner must reconcile this canonical edit after restoration.
    inner->SetVisibilityOverride(false, true);
    inner->SetOpacityOverride(false, 1.0f);
    inner->SetTransformOverride(false, trans3d::identity());
    inner->SetPotentiallyVisible(false);
    const piTick wallShift(100);
    root->RestorePlaybackState(savedRoot, wallShift);
    inner->RestorePlaybackState(savedInner, wallShift);
    const auto restoredInner = inner->CapturePlaybackState();
    requireFrames(root->GetStartTime() == savedRoot.mStartTime + wallShift &&
        root->GetStopTime() == savedRoot.mStopTime + wallShift && !root->GetPlaying() && root->IsStoppedByAction() &&
        restoredInner.mStartTime == savedInner.mStartTime + wallShift &&
        restoredInner.mStopTime == savedInner.mStopTime + wallShift &&
        restoredInner.mLastActiveSpanClockOrigin == savedInner.mLastActiveSpanClockOrigin + wallShift &&
        restoredInner.mLastActiveSpanStart == savedInner.mLastActiveSpanStart &&
        restoredInner.mStartOffset == savedInner.mStartOffset && restoredInner.mIsPlaying == savedInner.mIsPlaying &&
        inner->GetPotentiallyVisible() && inner->GetVisibilityOverrideEnabled() &&
        inner->GetOpacityOverrideEnabled() && inner->GetTransformOverrideEnabled() && inner->GetCanonicalOpacity() == 0.8f,
        "capsule restores clocks/intent/overrides with shifted absolute time without reverting canonical edits");
    auto editedFrames = LayerPaintStatic::PrepareFrameTable({2, 0}, 3);
    requireFrames(editedFrames && paint->PublishFrameTable(*editedFrames), "edit canonical table while holding capsule");
    paint->SetMaxRepeatCount(2);
    paint->SetPlaying(true); paint->SetTime(piTick(0)); paint->SetOffset(1);
    paint->RestorePlaybackState(savedPaint);
    requireFrames(!paint->GetPlaying() && paint->GetNumFrames() == 2 && paint->GetFrameBuffer()[0] == 2 &&
        paint->GetFrameBuffer()[1] == 0 && paint->GetMaxRepeatCount() == 2 &&
        paint->GetCurrentDrawing() == paint->GetDrawing(2) &&
        paint->CapturePlaybackState().mTime == piTick::FromFrames(4, 30),
        "paint capsule reselects saved time against edited table without reverting canonical repeat or paused intent");
    sequence.Deinit(&log); log.End();
    std::puts("[IMM_LAYER_EVALUATION_CONTEXT_TEST] boundaryClock=1 scopedProperties=1 localActions=1 sharedAudioUntouched=1 foundationOnly=1");
    std::puts("[IMM_PLAYBACK_CAPSULE_TEST] restoredClocksAndIntent=1 currentTableReselected=1 canonicalEditsPreserved=1");
}

static void CheckLayerDrivenPaintFrameGrowth()
{
    using namespace ImmCore;
    using namespace ImmImporter;
    piLog log;
    requireFrames(log.Init(L"artifacts/paint-frame-layer-runtime.log", PILOG_TXT), "layer frame test log");
    Sequence sequence;
    requireFrames(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "layer frame test sequence");
    Layer * root = sequence.GetRoot();
    requireFrames(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
        1.0f, true, piTick(0), 1, UINT32_MAX, &log), "layer frame test root");
    root->SetStartTime(piTick(0));
    root->SetStopTime(piTick::FromFrames(2, 30));
    root->SetPlaying(true);
    const auto createPaint = [&](const wchar_t * name) {
        Layer * layer = sequence.CreateLayer(root);
        requireFrames(layer != nullptr && layer->Init(Layer::Type::Paint, name, true, trans3d::identity(),
            trans3d::identity(), 1.0f, false, piTick(0), 1, UINT32_MAX, &log), "layer frame test paint layer");
        std::unique_ptr<LayerPaintStatic> implementation(new LayerPaintStatic());
        requireFrames(implementation->Init(3, 1, 0, 30, 1), "layer frame test paint implementation");
        for (int index = 0; index < 3; ++index)
            requireFrames(implementation->GetDrawing(index)->Init(0), "layer frame test drawing");
        layer->SetImplementation(implementation.release());
        layer->SetLoaded(true);
        return layer;
    };
    Layer * keyed = createPaint(L"KeyedPaint");
    auto * keyedPaint = static_cast<LayerPaintStatic *>(keyed->GetImplementation());
    Layer::AnimValue visible{};
    visible.mBool = true;
    requireFrames(keyed->AddKey(piTick(0), Layer::AnimProperty::Visibility, visible,
        Layer::InterpolationType::None), "layer frame test visibility key");
    keyed->SetStateAt(piTick::FromFrames(9, 30), &log);
    requireFrames(keyedPaint->GetPlaying() && keyedPaint->GetNumFrames() == 1,
        "real visibility evaluation starts logical single-frame paint intent");
    auto grown = LayerPaintStatic::PrepareFrameTable({ 0, 1, 2, 0 }, 3);
    requireFrames(grown != nullptr && keyedPaint->PublishFrameTable(*grown) &&
        keyedPaint->GetCurrentDrawing() == keyedPaint->GetDrawing(2),
        "real layer evaluation records held root local time before single-frame growth");
    auto single = LayerPaintStatic::PrepareFrameTable({ 0 }, 3);
    requireFrames(single != nullptr && keyedPaint->PublishFrameTable(*single), "keyed shrink to single frame");
    visible.mBool = false;
    requireFrames(keyed->AddKey(piTick::FromFrames(1, 30), Layer::AnimProperty::Visibility, visible,
        Layer::InterpolationType::None), "layer frame test hidden key");
    keyed->SetStateAt(piTick::FromFrames(9, 30), &log);
    requireFrames(!keyedPaint->GetPlaying(), "false visibility key stops single-frame logical intent");

    Layer * keyless = createPaint(L"KeylessPaint");
    auto * keylessPaint = static_cast<LayerPaintStatic *>(keyless->GetImplementation());
    keyless->SetStateAt(piTick::FromFrames(9, 30), &log);
    auto keylessGrown = LayerPaintStatic::PrepareFrameTable({ 0, 1, 2, 0 }, 3);
    requireFrames(keylessGrown != nullptr && keylessPaint->PublishFrameTable(*keylessGrown) &&
        !keylessPaint->GetPlaying() && keylessPaint->GetCurrentDrawing() == keylessPaint->GetDrawing(0),
        "imported keyless paint remains stopped after actual layer evaluation and growth");
    sequence.Deinit(&log);
    log.End();
}

void CheckPaintFrameTables()
{
    using namespace ImmCore;
    using namespace ImmImporter;
    LayerPaintStatic paint;
    requireFrames(paint.Init(3, 1, 0, 30, 1), "paint initialization");
    for (int index = 0; index < 3; ++index)
        requireFrames(paint.GetDrawing(index)->Init(0), "empty drawing initialization");
    const Drawing * first = paint.GetDrawing(0);
    const Drawing * second = paint.GetDrawing(1);
    const Drawing * third = paint.GetDrawing(2);
    requireFrames(!paint.GetPlaying(), "keyless paint does not implicitly start");
    auto grown = LayerPaintStatic::PrepareFrameTable({ 0, 1, 1, 2 }, 3);
    requireFrames(grown != nullptr && paint.GetNumFrames() == 1 && paint.GetCurrentDrawing() == first,
        "prepared table leaves presented sequence unchanged");
    paint.SetPlaying(true);
    requireFrames(paint.GetPlaying(), "single-frame paint retains playing intent");
    paint.SetTime(piTick::FromFrames(2, 30));
    paint.SetPlaying(false);
    requireFrames(paint.PublishFrameTable(*grown) && paint.GetCurrentDrawing() == second,
        "growth reselects time recorded while single-frame, including held frame");
    paint.SetTime(piTick::FromFrames(3, 30));
    requireFrames(paint.GetCurrentDrawing() == second, "stopped paint ignores clock advancement");
    auto shorter = LayerPaintStatic::PrepareFrameTable({ 2, 0 }, 3);
    requireFrames(shorter != nullptr && paint.PublishFrameTable(*shorter) && paint.GetCurrentDrawing() == third,
        "paused shrink selects replacement table from held local time");
    requireFrames(paint.PublishFrameTable(*shorter) && paint.GetNumFrames() == 4 &&
        paint.GetCurrentDrawing() == second, "ownership swap supports rollback without rebuilding drawings");
    requireFrames(!LayerPaintStatic::PrepareFrameTable({ 3 }, 3), "invalid private drawing index rejects preparation");
    LayerPaintStatic::PreparedFrameTable invalid;
    invalid.mFrames = { UINT32_MAX };
    requireFrames(!paint.PublishFrameTable(invalid) && paint.GetNumFrames() == 4 &&
        paint.GetCurrentDrawing() == second, "publication revalidates indices without mutating old table");

    auto empty = LayerPaintStatic::PrepareFrameTable({}, 3);
    requireFrames(empty != nullptr && paint.PublishFrameTable(*empty) && paint.GetNumFrames() == 0 &&
        paint.GetFrameBuffer() == nullptr && paint.GetCurrentDrawing() == nullptr && !paint.HasBBox(),
        "zero frames have no current drawing, frame storage or bounds");
    const auto & emptyBox = paint.GetBBox();
    requireFrames(emptyBox.mMinX > emptyBox.mMaxX && emptyBox.mMinY > emptyBox.mMaxY &&
        emptyBox.mMinZ > emptyBox.mMaxZ, "empty table bounds use the empty-bound convention");
    paint.SetPlaying(true);
    paint.SetTime(piTick::FromFrames(5, 30));
    auto restored = LayerPaintStatic::PrepareFrameTable({ 0, 1, 2 }, 3);
    requireFrames(restored != nullptr && paint.PublishFrameTable(*restored) && paint.GetCurrentDrawing() == third,
        "empty-to-animated growth preserves playback time and intent");
    paint.SetMaxRepeatCount(1);
    paint.SetTime(piTick::FromFrames(5, 30));
    auto finite = LayerPaintStatic::PrepareFrameTable({ 0, 1 }, 3);
    requireFrames(finite != nullptr && paint.PublishFrameTable(*finite) && paint.GetCurrentDrawing() == second,
        "finite repeats clamp selection after shrink");
    paint.SetMaxRepeatCount(0);
    paint.SetOffset(1);
    paint.SetTime(piTick::FromFrames(5, 30));
    requireFrames(paint.GetCurrentDrawing() == first, "infinite repeat and frame offset select correctly");
    auto single = LayerPaintStatic::PrepareFrameTable({ 2 }, 3);
    requireFrames(single != nullptr && paint.PublishFrameTable(*single) && paint.GetCurrentDrawing() == third,
        "shrinking to one frame resets an obsolete current frame index");
    paint.SetTime(piTick::FromFrames(7, 30));
    auto again = LayerPaintStatic::PrepareFrameTable({ 0, 1, 2 }, 3);
    requireFrames(again != nullptr && paint.PublishFrameTable(*again) && paint.GetCurrentDrawing() == third,
        "single-frame clock continues to determine later growth");

    auto appended = LayerPaintStatic::PrepareFrameTable({ 3 }, 4);
    requireFrames(appended != nullptr && !paint.PublishFrameTable(*appended) && paint.GetNumFrames() == 3,
        "pending append index cannot publish before drawing exists");
    Drawing * added = paint.AddDrawing();
    requireFrames(added != nullptr && paint.PublishFrameTable(*appended) &&
        paint.GetCurrentDrawing() == added, "prepared append table publishes after drawing append");
    auto beforeDelete = LayerPaintStatic::PrepareFrameTable({ 1, 2 }, 4);
    requireFrames(beforeDelete != nullptr && paint.PublishFrameTable(*beforeDelete), "deletion replacement table");
    std::unique_ptr<Drawing> removed(paint.ExtractDrawing(0));
    requireFrames(removed.get() == first && paint.GetDrawing(0) == second && paint.GetDrawing(1) == third &&
        paint.GetFrameBuffer()[0] == 0 && paint.GetFrameBuffer()[1] == 1 && paint.GetCurrentDrawing() == second,
        "drawing compaction updates table indices once and preserves surviving object addresses");
    removed->Deinit();
    removed.reset();
    paint.Deinit();
    CheckLayerDrivenPaintFrameGrowth();
    CheckLayerDrivenPlaybackKeys<LayerPaintStatic>();
    CheckLayerDrivenPlaybackKeys<LayerPaintPretessellated>();
    CheckLayerDrivenSoundOffsets();
    CheckIndependentEvaluationContext();
    CheckHiddenLayerRenderEligibility();
    std::puts("[IMM_PAINT_FRAME_TABLE_TEST] detached publication, held selection, zero frames, repeat/offset, append and compaction passed");
}
