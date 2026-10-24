// Real CPU Document publication versus actions executed after preparation.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include "libImmImporter/src/fromImmersive/fromImmersive.h"
#include "libImmImporter/src/document/layerSpawnArea.h"
#include "libImmPlayer/src/document.h"

using namespace ImmCore;
using namespace ImmImporter;
using namespace ImmPlayer;

namespace
{
    void RequireDefaultRace(bool condition, const char * message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "[IMM_DEFAULT_ACTION_RACE] %s\n", message);
            std::exit(1);
        }
    }
}

void CheckDocumentDefaultActionRace(const wchar_t * fixturePath)
{
    piLog log;
    RequireDefaultRace(log.Init(L"artifacts/document-default-action-race.log", PILOG_TXT), "race log");
    bool allPassed = true;
    for (bool preview : { false, true })
    {
        Document doc;
        RequireDefaultRace(doc.Init(L"default-action-race", 0), "race document");
        piTick now(1000000);
        const auto cpu = [&](const Document::Command * command = nullptr) {
            doc.UpdateStateCPU(nullptr, nullptr, nullptr, nullptr, Drawing::ColorSpace::Linear,
                Drawing::PaintRenderingTechnique::Static, nullptr, &log, now, command);
        };
        const auto gpu = [&]() {
            doc.UpdateStateGPU(nullptr, nullptr, nullptr, nullptr, &log, Drawing::ColorSpace::Linear);
        };
        Document::Command load{};
        load.mType = Document::Command::Type::Load; load.mFileType = Document::ImportType::IMM_disk;
        RequireDefaultRace(load.mStrArg.Init(1024) && load.mStrArg.CopyW(fixturePath), "race fixture path");
        cpu(&load); gpu();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while ((doc.GetLoadingState() != Document::LoadingState::Loaded || IsLoadingAsync(doc.GetSequence())) &&
            std::chrono::steady_clock::now() < deadline)
        { std::this_thread::sleep_for(std::chrono::milliseconds(1)); cpu(); gpu(); }
        RequireDefaultRace(doc.GetLoadingState() == Document::LoadingState::Loaded && !IsLoadingAsync(doc.GetSequence()),
            "race fixture loaded");
        Sequence * sequence = doc.GetSequence();
        // Remove the fixture's unrelated late MakeDefault so the old one-slot
        // preflight cannot accidentally cover both generations of this track.
        sequence->Recurse([](Layer * layer, int, int, bool) {
            layer->ReplaceAnimKeys(Layer::AnimProperty::Action, {}); return true;
        }, false, false, false, false);
        sequence->SetInitialSpawnArea(nullptr);
        Layer * scope = sequence->CreateLayer(sequence->GetRoot());
        RequireDefaultRace(scope && scope->Init(Layer::Type::Group, L"RaceScope", true, trans3d::identity(),
            trans3d::identity(), 1.0f, true, piTick(1000), 1, UINT32_MAX, &log), "race scope");
        Layer * spawn = sequence->CreateLayer(scope);
        RequireDefaultRace(spawn && spawn->Init(Layer::Type::SpawnArea, L"RaceDefault", true, trans3d::identity(),
            trans3d::identity(), 1.0f, false, piTick(0), 1, UINT32_MAX, &log), "race spawn");
        auto * implementation = new LayerSpawnArea();
        LayerSpawnArea::Volume volume{}; volume.mType = LayerSpawnArea::Volume::Type::Sphere;
        volume.mShape.mSphere = vec4(0, 0, 0, 1);
        RequireDefaultRace(implementation->Init(volume, LayerSpawnArea::TrackingLevel::Floor), "race spawn implementation");
        spawn->SetImplementation(implementation); spawn->SetLoaded(true);
        Layer::AnimValue visible{}; visible.mBool = true;
        RequireDefaultRace(scope->AddKey(piTick(0), Layer::AnimProperty::Visibility, visible, Layer::InterpolationType::None) &&
            spawn->AddKey(piTick(0), Layer::AnimProperty::Visibility, visible, Layer::InterpolationType::None), "race visibility");
        Layer::AnimValue action{}; action.mInt = static_cast<uint32_t>(Layer::AnimAction::MakeDefault);
        RequireDefaultRace(spawn->AddKey(piTick(5), Layer::AnimProperty::Action, action, Layer::InterpolationType::None) &&
            sequence->PrepareDefaultActionCapacity(1) && doc.AttachEditing(), "race initial action capacity and attachment");
        now = piTick(1000004); cpu();
        if (preview)
            RequireDefaultRace(doc.ControlTimelinePreview(scope, 0, piTick(4), now, &log) == 0 &&
                doc.ControlTimelinePreview(scope, 2, piTick(0), now, &log) == 0, "race independent playback");
        RequireDefaultRace(doc.QueueAnimationKeyRemoval(spawn->GetID(), Layer::AnimProperty::Action, piTick(5)) == 0 &&
            doc.QueueAnimationKey(spawn->GetID(), Layer::AnimProperty::Action, piTick(10), action, Layer::InterpolationType::None) == 0,
            "queue retimed default identity");
        int32_t result = -99;
        const uint64_t revision = doc.CommitEdits(&result);
        RequireDefaultRace(revision != 0 && result == 0, "seal retimed default");
        Document::Command commit{}; commit.mType = Document::Command::Type::AuthoringCommit;
        cpu(&commit); // Deliberately omit GPU publication while the old key executes.
        Document::AuthoringCommitStatus status{};
        RequireDefaultRace(doc.GetAuthoringCommitStatus(revision, &status) && status.mState == Document::AuthoringCommitState::Prepared,
            "default retime CPU prepared");
        now = piTick(1000005); cpu();
        const bool oldExecuted = preview ? doc.GetTimelinePreviewContext()->mDefaultSpawnAreaLayerId == spawn->GetID() :
            sequence->GetInitialSpawnArea() == spawn;
        RequireDefaultRace(oldExecuted, "old default executes between prepare and publication");
        gpu();
        RequireDefaultRace(doc.GetAuthoringCommitStatus(revision, &status) && status.mState == Document::AuthoringCommitState::Presented,
            "retimed default published");
        const bool oldInvalidated = preview ? doc.GetTimelinePreviewContext()->mDefaultSpawnAreaLayerId == UINT32_MAX :
            sequence->GetInitialSpawnArea() == nullptr;
        RequireDefaultRace(oldInvalidated, "removed executed identity becomes inactive");
        now = piTick(1000010); cpu();
        const auto * current = spawn->GetAnimKeyAt(Layer::AnimProperty::Action, piTick(10));
        bool correct = current != nullptr;
        if (preview)
        {
            const auto * context = doc.GetTimelinePreviewContext();
            correct &= context && context->mDefaultSpawnAreaLayerId == spawn->GetID() &&
                context->mDefaultActionHistory.size() == 2 && context->mConsumedActions.size() == 1 &&
                !context->CanConsumeAction(spawn, piTick(10)) && current && !current->mValue.mBool &&
                sequence->GetInitialSpawnArea() == nullptr;
        }
        else correct &= sequence->GetInitialSpawnArea() == spawn && current && current->mValue.mBool;
        std::printf("[IMM_DEFAULT_ACTION_RACE] preview=%d oldExecuted=%d oldInvalidated=%d newExecuted=%d\n",
            preview ? 1 : 0, oldExecuted ? 1 : 0, oldInvalidated ? 1 : 0, correct ? 1 : 0);
        allPassed &= correct;
        doc.UnloadSync(nullptr, nullptr, nullptr, nullptr, Drawing::ColorSpace::Linear,
            Drawing::PaintRenderingTechnique::Static, nullptr, nullptr, &log, now);
        doc.End(); load.mStrArg.End();
    }
    log.End();
    RequireDefaultRace(allPassed, "capacity must cover old executions after preparation and new identities after publication");
}
