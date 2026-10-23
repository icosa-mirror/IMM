#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

#include "libImmImporter/src/document/layer.h"
#include "libImmImporter/src/document/layerPaintStatic.h"
#include "libImmImporter/src/document/sequence.h"

void CheckPaintPlaybackReselection()
{
    using namespace ImmCore;
    using namespace ImmImporter;
    const auto require = [](bool condition, const char * message) {
        if (!condition)
        {
            std::fprintf(stderr, "[IMM_PAINT_PLAYBACK_SELECTION_TEST] %s\n", message);
            std::exit(1);
        }
    };
    piLog log;
    require(log.Init(L"artifacts/paint-playback-selection.log", PILOG_TXT), "initialize log");
    Sequence sequence;
    require(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "initialize sequence");
    Layer * root = sequence.GetRoot();
    require(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
        1.0f, true, piTick(0), 1, UINT32_MAX, &log), "initialize root");
    root->SetStartTime(piTick(0));
    root->SetStopTime(piTick::FromFrames(7, 30));
    root->SetPlaying(false);
    Layer::AnimValue action{};
    action.mInt = static_cast<uint32_t>(Layer::AnimAction::Stop);
    require(root->AddKey(piTick(0), Layer::AnimProperty::Action, action, Layer::InterpolationType::None), "unconsumed root action");
    Layer * layer = sequence.CreateLayer(root);
    require(layer != nullptr && layer->Init(Layer::Type::Paint, L"Paint", true, trans3d::identity(),
        trans3d::identity(), 1.0f, false, piTick(0), 1, UINT32_MAX, &log), "initialize paint layer");
    auto * paint = new LayerPaintStatic();
    require(paint->Init(3, 3, 0, 30, 1), "initialize static paint");
    for (uint32_t index = 0; index < 3; ++index)
    {
        require(paint->GetDrawing(index)->Init(0), "initialize drawing");
        paint->GetFrameBuffer()[index] = index;
    }
    layer->SetImplementation(paint);
    layer->SetLoaded(true);
    Layer::AnimValue value{};
    value.mBool = true;
    require(layer->AddKey(piTick(0), Layer::AnimProperty::Visibility, value, Layer::InterpolationType::None), "visibility key");
    const auto select = [&](uint32_t expected, uint32_t canonicalRepeat = 0) {
        layer->ReevaluatePaintPlaybackAt(piTick::FromFrames(100, 30), &log);
        require(paint->GetCurrentDrawing() == paint->GetDrawing(expected), "held key reselection");
        require(!paint->GetPlaying() && !root->GetPlaying() && root->HasStopTime() &&
            root->GetStopTime() == piTick::FromFrames(7, 30) && root->GetStartTime() == piTick(0),
            "reselection preserves paused intent and parent clock");
        require(!root->GetAnimKey(Layer::AnimProperty::Action, 0)->mValue.mBool,
            "reselection does not consume root actions");
        require(paint->GetMaxRepeatCount() == canonicalRepeat, "reselection preserves canonical paint repeat");
    };
    const auto replace = [&](Layer::AnimProperty property, const Layer::AnimValue & replacement) {
        std::vector<Layer::AnimKey> keys;
        require(layer->CopyAnimKeys(property, keys), "copy keys for detached replacement");
        require(Layer::SetAnimKey(keys, piTick(0), replacement, Layer::InterpolationType::None),
            "replace detached key");
        layer->ReplaceAnimKeys(property, std::move(keys));
    };
    select(1); // Held local frame7 wraps to1 despite later wall time.
    value.mBool = false;
    require(layer->AddKey(piTick(0), Layer::AnimProperty::Loop, value, Layer::InterpolationType::None), "insert Loop");
    select(2);
    value.mBool = true;
    replace(Layer::AnimProperty::Loop, value);
    select(1);
    layer->ReplaceAnimKeys(Layer::AnimProperty::Loop, {});
    select(1);
    value.mInt = static_cast<uint32_t>(piTick::CastInt(piTick::FromFrames(1, 30)));
    require(layer->AddKey(piTick(0), Layer::AnimProperty::Offset, value, Layer::InterpolationType::None), "insert Offset");
    select(2);
    value.mInt = static_cast<uint32_t>(piTick::CastInt(piTick::FromFrames(2, 30)));
    replace(Layer::AnimProperty::Offset, value);
    select(0);
    layer->ReplaceAnimKeys(Layer::AnimProperty::Offset, {});
    select(1);
    paint->SetMaxRepeatCount(1);
    select(2, 1);
    paint->SetMaxRepeatCount(3);
    select(1, 3);
    paint->SetMaxRepeatCount(UINT32_MAX);
    select(1, UINT32_MAX);
    value.mBool = false;
    require(layer->AddKey(piTick(0), Layer::AnimProperty::Loop, value, Layer::InterpolationType::None), "Loop overrides canonical repeat");
    paint->SetMaxRepeatCount(0);
    select(2);
    value.mBool = true;
    replace(Layer::AnimProperty::Loop, value);
    paint->SetMaxRepeatCount(1);
    select(1, 1);
    layer->ReplaceAnimKeys(Layer::AnimProperty::Loop, {});
    select(2, 1);
    paint->SetMaxRepeatCount(0);
    select(1);
    root->SetCanonicalVisible(false);
    value.mBool = false;
    require(layer->AddKey(piTick(0), Layer::AnimProperty::Loop, value, Layer::InterpolationType::None), "hidden paint Loop");
    select(2);
    require(!layer->GetWorldVisible(), "latent reselection does not unhide paint");
    root->SetCanonicalVisible(true); // Same canonical setter used by live parent properties; no reevaluation.
    require(layer->GetWorldVisible() && paint->GetCurrentDrawing() == paint->GetDrawing(2),
        "canonical parent unhide reveals the new held selection without another evaluation");
    layer->ReplaceAnimKeys(Layer::AnimProperty::Loop, {});
    layer->ReplaceAnimKeys(Layer::AnimProperty::Visibility, {});
    value.mInt = static_cast<uint32_t>(piTick::CastInt(piTick::FromFrames(2, 30)));
    require(layer->AddKey(piTick(0), Layer::AnimProperty::Offset, value, Layer::InterpolationType::None), "keyless paint Offset");
    select(0); // Existing keyless semantics retain local0, without adding Offset.
    value.mBool = false;
    require(layer->AddKey(piTick(0), Layer::AnimProperty::Visibility, value, Layer::InterpolationType::None), "inactive paint visibility");
    select(0);
    require(!layer->GetWorldVisible(), "inactive visibility retains local0 without becoming visible");
    sequence.Deinit(&log);
    log.End();
    std::puts("[IMM_PAINT_PLAYBACK_SELECTION_TEST] held Loop/Offset add/replace/remove and keyless/hidden policy passed");
}
