#include <cstdio>
#include <cstdlib>
#include <limits>

#include "libImmImporter/src/document/sequence.h"
#include "libImmImporter/src/document/layerSpawnArea.h"

namespace
{
    using namespace ImmCore;
    using namespace ImmImporter;

    void RequireDefault(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "[IMM_DEFAULT_ACTION_TEST] %s\n", message);
        std::exit(1);
    }

    std::vector<Layer::AnimKey> DefaultKey(int64_t tick)
    {
        Layer::AnimKey key{};
        key.mTime = piTick(tick);
        key.mValue.mInt = static_cast<uint32_t>(Layer::AnimAction::MakeDefault);
        key.mInterpolation = Layer::InterpolationType::None;
        return { key };
    }
}

void CheckDefaultActionHistory()
{
    piLog log;
    RequireDefault(log.Init(L"artifacts/layer-action-default.log", PILOG_TXT), "log initialization");
    Sequence sequence;
    RequireDefault(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "sequence initialization");
    Layer* root = sequence.GetRoot();
    RequireDefault(root->Init(Layer::Type::Group, L"Root", true, trans3d::identity(), trans3d::identity(),
        1.0f, true, piTick(0), 0, UINT32_MAX, &log), "root initialization");
    const auto spawn = [&](const wchar_t* name) {
        Layer* layer = sequence.CreateLayer(root);
        RequireDefault(layer && layer->Init(Layer::Type::SpawnArea, name, true, trans3d::identity(),
            trans3d::identity(), 1.0f, false, piTick(0), 1, UINT32_MAX, &log), "spawn initialization");
        auto* implementation = new LayerSpawnArea();
        LayerSpawnArea::Volume volume{};
        volume.mType = LayerSpawnArea::Volume::Type::Sphere;
        RequireDefault(implementation->Init(volume, LayerSpawnArea::TrackingLevel::Floor), "spawn implementation");
        layer->SetImplementation(implementation);
        layer->SetLoaded(true);
        return layer;
    };
    Layer* a = spawn(L"A"), * b = spawn(L"B"), * baseline = spawn(L"Baseline");
    sequence.SetInitialSpawnArea(baseline);
    sequence.SetSpawnAreaNeedsUpdate(false);
    RequireDefault(!sequence.RecordDefaultActionExecution(a, piTick(900)) &&
        sequence.GetInitialSpawnArea() == baseline && !sequence.GetSpawnAreaNeedsUpdate(),
        "unprepared execution fails before changing the effective default");
    RequireDefault(!sequence.PrepareDefaultActionCapacity(std::numeric_limits<size_t>::max()) &&
        sequence.GetInitialSpawnAreaBaseline() == baseline && !sequence.GetSpawnAreaNeedsUpdate(),
        "overflow reservation preserves baseline and dirty state");
    RequireDefault(sequence.PrepareDefaultActionCapacity(2), "prepare two identities");
    RequireDefault(sequence.RecordDefaultActionExecution(a, piTick(900)) &&
        sequence.RecordDefaultActionExecution(b, piTick(10)) && sequence.GetInitialSpawnArea() == b,
        "actual execution order wins over key times across layers");
    RequireDefault(sequence.RecordDefaultActionExecution(a, piTick(900)) && sequence.GetInitialSpawnArea() == a,
        "re-executed identity moves to the latest execution position");
    sequence.SetSpawnAreaNeedsUpdate(false);
    RequireDefault(sequence.RecordDefaultActionExecution(a, piTick(900)) && !sequence.GetSpawnAreaNeedsUpdate(),
        "unchanged effective default does not set dirty");
    sequence.ReconcileDefaultActionKeys(a, {});
    RequireDefault(sequence.GetInitialSpawnArea() == b, "remove latest identity reveals prior execution");
    sequence.ReconcileDefaultActionKeys(a, DefaultKey(900));
    RequireDefault(sequence.GetInitialSpawnArea() == a, "Undo reactivates retained identity without executing it");
    a->ReplaceAnimKeys(Layer::AnimProperty::Action, DefaultKey(900));
    b->ReplaceAnimKeys(Layer::AnimProperty::Action, DefaultKey(10));
    sequence.SetSpawnAreaNeedsUpdate(false);
    sequence.ReconcileDefaultActionKeys();
    RequireDefault(sequence.GetInitialSpawnArea() == a && !sequence.GetSpawnAreaNeedsUpdate(),
        "whole-live reconciliation preserves an unchanged winner without intermediate dirty state");
    a->ReplaceAnimKeys(Layer::AnimProperty::Action, {});
    sequence.ReconcileDefaultActionKeys();
    RequireDefault(sequence.GetInitialSpawnArea() == b, "whole-live reconciliation observes removed keys");
    a->ReplaceAnimKeys(Layer::AnimProperty::Action, DefaultKey(900));
    sequence.ReconcileDefaultActionKeys();
    RequireDefault(sequence.GetInitialSpawnArea() == a, "whole-live reconciliation restores Undo identity order");
    sequence.SetInitialSpawnArea(baseline);
    sequence.ReconcileDefaultActionKeys(a, DefaultKey(900));
    RequireDefault(sequence.GetInitialSpawnArea() == baseline, "explicit baseline supersedes old execution effects");
    RequireDefault(sequence.RecordDefaultActionExecution(a, piTick(900)) &&
        sequence.RecordDefaultActionExecution(b, piTick(10)), "new executions after explicit choice");
    Layer* surviving[] = { a, baseline };
    sequence.ReconcileDefaultActionTargets(surviving, 2);
    RequireDefault(sequence.GetInitialSpawnArea() == a, "deleting latest target falls back to surviving execution");
    sequence.ReconcileDefaultActionTargets(surviving, 1);
    RequireDefault(sequence.GetInitialSpawnAreaBaseline() == nullptr && sequence.GetInitialSpawnArea() == a,
        "deleting shadowed baseline does not discard surviving execution");
    sequence.ReconcileDefaultActionKeys(a, {});
    RequireDefault(sequence.GetInitialSpawnArea() == nullptr, "no surviving baseline or active execution yields no default");
    sequence.ReconcileDefaultActionKeys(a, DefaultKey(900));
    sequence.SetSpawnAreaNeedsUpdate(false);
    sequence.ResetDefaultActionHistory();
    sequence.ReconcileDefaultActionKeys(a, {});
    RequireDefault(sequence.GetInitialSpawnArea() == a && sequence.GetInitialSpawnAreaBaseline() == a &&
        !sequence.GetSpawnAreaNeedsUpdate(), "history reset preserves effective choice as a fresh baseline");
    sequence.Deinit(&log);
    RequireDefault(sequence.GetInitialSpawnArea() == nullptr && sequence.GetInitialSpawnAreaBaseline() == nullptr,
        "teardown clears provenance pointers");
    RequireDefault(sequence.Init(Sequence::Type::Animated, 0, vec3(0.0f)), "sequence reinitialization");
    root = sequence.GetRoot();
    RequireDefault(root->Init(Layer::Type::Group, L"FreshRoot", true, trans3d::identity(), trans3d::identity(),
        1.0f, true, piTick(0), 0, UINT32_MAX, &log), "fresh root initialization");
    Layer* fresh = spawn(L"FreshSpawn");
    RequireDefault(!sequence.RecordDefaultActionExecution(fresh, piTick(0)) && sequence.GetInitialSpawnArea() == nullptr,
        "teardown releases ledger capacity so reuse requires fresh preparation");
    sequence.Deinit(&log);
    log.End();
    std::puts("[IMM_DEFAULT_ACTION_TEST] executionOrder=1 undo=1 explicitChoice=1 deletion=1 capacity=1 reset=1");
}
