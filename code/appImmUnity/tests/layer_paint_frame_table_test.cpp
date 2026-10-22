#include <cstdio>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include "libImmImporter/src/document/layerPaintStatic.h"
#include "libImmImporter/src/document/layer.h"
#include "libImmImporter/src/document/sequence.h"

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
    std::puts("[IMM_PAINT_FRAME_TABLE_TEST] detached publication, held selection, zero frames, repeat/offset, append and compaction passed");
}
