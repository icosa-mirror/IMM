// Real Document lifecycle/authoring test; groups/viewpoints only, no GPU resources.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include "libImmImporter/src/fromImmersive/fromImmersive.h"
#include "libImmPlayer/src/document.h"
using namespace ImmCore;
using namespace ImmImporter;
using namespace ImmPlayer;
static void require(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "[IMM_AUTHORING_FAILURE_TEST] %s\n", message); std::exit(1); }
}

static void checkLayerInitialization(piLog * log)
{
    Sequence sequence;
    require(sequence.Init(Sequence::Type::Comic, 0, vec3(0.0f)), "layer lifecycle sequence");
    Layer * parent = sequence.GetRoot();
    const auto initialize = [&](Layer * layer) {
        return layer->Init(Layer::Type::Group, L"lifecycle", true, trans3d::identity(),
            trans3d::identity(), 1.0f, false, piTick(0), 0, UINT32_MAX, log);
    };
    require(initialize(parent), "root initialization");
    require(!initialize(parent), "active layer cannot be initialized twice");
    for (int depth = 1; depth < 64; ++depth)
    {
        Layer * child = sequence.CreateLayer(parent);
        require(child != nullptr && initialize(child), "supported full-name depth");
        parent = child;
    }
    const uint32_t before = parent->GetNumChildren();
    Layer * failed = sequence.CreateLayer(parent);
    require(failed != nullptr && !initialize(failed), "full-name stack limit rejects safely");
    require(parent->GetNumChildren() == before, "failed initialization never publishes child");
    failed->Deinit(log);
    failed->Deinit(log);
    sequence.Deinit(log);
    require(sequence.GetRoot() == nullptr && sequence.GetLayerStorageCount() == 0,
        "sequence teardown clears owned registry");
    sequence.Deinit(log);
    require(sequence.Init(Sequence::Type::Comic, 0, vec3(0.0f)), "sequence reinitialization");
    require(initialize(sequence.GetRoot()), "layer initialization after sequence teardown");
    sequence.Deinit(log);
    std::puts("[IMM_LAYER_LIFECYCLE_TEST] depth limit, unpublished failure, repeated teardown and reinitialization passed");
}
int wmain(int argc, wchar_t** argv)
{
    require(argc == 2, "fixture argument");
    piLog log;
    require(log.Init(L"artifacts/authoring-failure-runtime.log", PILOG_TXT), "log init");
    checkLayerInitialization(&log);
    Document doc;
    require(doc.Init(L"authoring-failures", 0), "document init");
    auto cpu = [&](const Document::Command* command = nullptr) {
        doc.UpdateStateCPU(nullptr, nullptr, nullptr, nullptr, Drawing::ColorSpace::Linear,
            Drawing::PaintRenderingTechnique::Static, nullptr, &log, piTick(1000000), command);
    };
    auto gpu = [&]() { doc.UpdateStateGPU(nullptr, nullptr, nullptr, nullptr, &log, Drawing::ColorSpace::Linear); };
    Document::Command load{};
    load.mType = Document::Command::Type::Load;
    load.mFileType = Document::ImportType::IMM_disk;
    require(load.mStrArg.Init(1024) && load.mStrArg.CopyW(argv[1]), "fixture path");
    auto loadFixture = [&]() {
        cpu(&load); gpu();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (doc.GetLoadingState() != Document::LoadingState::Loaded && std::chrono::steady_clock::now() < deadline)
        { std::this_thread::sleep_for(std::chrono::milliseconds(1)); cpu(); gpu(); }
        require(doc.GetLoadingState() == Document::LoadingState::Loaded, "fixture loaded");
        require(doc.AttachEditing(), "attach editing");
    };
    auto unload = [&](uint64_t revision) {
        Document::Command command{}; command.mType = Document::Command::Type::Unload;
        cpu(&command); gpu();
        doc.UnloadSync(nullptr, nullptr, nullptr, nullptr, Drawing::ColorSpace::Linear,
            Drawing::PaintRenderingTechnique::Static, nullptr, nullptr, &log, piTick(1000001));
        require(doc.GetLoadingState() == Document::LoadingState::UnloadingCompleted && !doc.IsEditing(), "unload invalidates session");
        Document::AuthoringCommitStatus status{}; Document::AuthoringRevisions revisions{};
        require(!doc.GetAuthoringCommitStatus(revision, &status) && !doc.GetAuthoringRevisions(&revisions), "old session queries rejected");
        require(doc.QueueLayerDeletion(0) == -4, "mutation rejected while detached");
    };
    Document::Command commit{}; commit.mType = Document::Command::Type::AuthoringCommit;
    for (int prepared = 0; prepared != 2; ++prepared)
    {
        loadFixture();
        Layer* root = doc.GetSequence()->GetRoot();
        const uint32_t count = root->GetNumChildren();
        const uint32_t rootId = root->GetID();
        uint32_t createdId = 0;
        require(doc.QueueGroupLayerCreation(UINT32_MAX, L"invalid", &createdId) == -1, "invalid parent rejected");
        require(doc.QueueLayerReparent(rootId, rootId, 0) == -3, "root reparent rejected");
        Layer* group = root->GetChild(0);
        require(group != nullptr && group->GetType() == Layer::Type::Group && group->GetNumChildren() != 0, "fixture group with viewpoint child");
        require(doc.QueueLayerReparent(group->GetID(), group->GetID(), 0) == -3, "ancestor cycle rejected");
        require(doc.QueueLayerDeletion(group->GetID()) == -3, "group containing non-group reference rejected");
        require(root->GetNumChildren() == count, "invalid operations preserve hierarchy");
        require(doc.QueueGroupLayerCreation(rootId, L"cancelled-group", &createdId) == 0, "queue group creation");
        int32_t result = -99;
        const uint64_t revision = doc.CommitEdits(&result);
        require(revision != 0 && result == 0, "commit group creation");
        Document::AuthoringCommitStatus status{};
        require(doc.GetAuthoringCommitStatus(revision, &status) && status.mState == Document::AuthoringCommitState::Queued, "queued state");
        require(!doc.DetachEditing(), "pending commit blocks detach");
        if (prepared)
        {
            cpu(&commit);
            require(doc.GetAuthoringCommitStatus(revision, &status) && status.mState == Document::AuthoringCommitState::Prepared, "CPU prepared creation");
        }
        require(root->GetNumChildren() == count, "unpublished creation leaves hierarchy unchanged");
        unload(revision);
        loadFixture();
        root = doc.GetSequence()->GetRoot();
        require(root->GetNumChildren() == count, "reload contains no cancelled creation");
        require(doc.QueueLayerDeletion(createdId) == -1, "cancelled layer ID is not a published layer");
        require(doc.QueueGroupLayerCreation(root->GetID(), L"valid-after-reload", &createdId) == 0, "new session accepts valid edit");
        const uint64_t freshRevision = doc.CommitEdits(&result);
        require(freshRevision != 0 && result == 0, "new commit accepted");
        cpu(&commit); gpu();
        require(doc.GetAuthoringCommitStatus(freshRevision, &status) && status.mState == Document::AuthoringCommitState::Presented, "new session publishes normally");
        require(root->GetNumChildren() == count + 1, "fresh creation published");
        require(doc.QueueLayerDeletion(createdId) == 0, "queue created group deletion");
        const uint64_t deletedRevision = doc.CommitEdits(&result);
        require(deletedRevision != 0 && result == 0, "commit group deletion");
        cpu(&commit); gpu();
        require(doc.GetAuthoringCommitStatus(deletedRevision, &status) && status.mState == Document::AuthoringCommitState::Presented, "deletion published");
        require(root->GetNumChildren() == count && doc.QueueLayerDeletion(createdId) == -1, "deleted layer ID rejected");
        unload(deletedRevision);
    }
    doc.End(); load.mStrArg.End(); log.End();
    std::puts("[IMM_AUTHORING_FAILURE_TEST] queued/prepared group unload, invalid parents, root reparent, reload, deleted IDs passed (CPU-only; no GPU cancellation evidence)");
    return 0;
}

