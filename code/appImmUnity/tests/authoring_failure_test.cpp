// Real Document lifecycle/authoring test; groups/viewpoints only, no GPU resources.
#include <chrono>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <thread>
#include <type_traits>
#include "libImmImporter/src/fromImmersive/fromImmersive.h"
#include "libImmPlayer/src/document.h"
#include "libImmPlayer/src/mngrPlayer.h"
#include "libImmPlayer/src/renderPerformanceSnapshot.h"
#include "libImmImporter/src/document/layerSpawnArea.h"
using namespace ImmCore;
using namespace ImmImporter;
using namespace ImmPlayer;
static void require(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "[IMM_AUTHORING_FAILURE_TEST] %s\n", message); std::exit(1); }
}

static void checkEmptySpawnCleanup()
{
    // Heap contents must not determine whether an absent preview is freed.
    alignas(LayerSpawnArea) unsigned char storage[sizeof(LayerSpawnArea)];
    std::memset(storage, 0xa5, sizeof(storage));
    auto * spawn = new (storage) LayerSpawnArea;
    LayerSpawnArea::Volume volume{};
    volume.mType = LayerSpawnArea::Volume::Type::Sphere;
    volume.mShape.mSphere = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    require(spawn->Init(volume, LayerSpawnArea::TrackingLevel::Floor), "empty spawn initialization");
    require(spawn->GetScreenshot() == nullptr, "empty spawn has no preview");
    spawn->Deinit();
    spawn->~LayerSpawnArea();
    std::puts("[IMM_SPAWN_PREVIEW_LIFECYCLE_TEST] no-preview cleanup ignores prior storage contents");
}

static void checkRenderPerformanceSnapshots()
{
    static_assert(std::is_trivially_copyable<RenderPerformanceSnapshot>::value,
        "render performance publication must own no allocation");
    RenderPerformanceSnapshotStore store;
    require(store.Get().numPaintDrawCalls == 0 && store.Get().numTriangles == 0,
        "performance snapshot starts empty");
    std::atomic<bool> started(false), done(false), invalid(false);
    std::atomic<unsigned int> readersReady(0);
    std::atomic<uint64_t> observations(0);
    const auto read = [&]() {
        readersReady.fetch_add(1);
        while (!started.load())
            std::this_thread::yield();
        do
        {
            const auto snapshot = store.Get();
            // One publication's fields encode the same value. Any partial copy
            // or a reset interleaved with a read violates these relationships.
            const int value = snapshot.numPaintDrawCalls;
            if (snapshot.numDrawCalls != value * 2 || snapshot.numTriangles != value * 8 ||
                snapshot.validationTimeFrame != static_cast<uint64_t>(value))
                invalid.store(true);
            observations.fetch_add(1);
        } while (!done.load());
    };
    std::thread first(read), second(read);
    while (readersReady.load() != 2)
        std::this_thread::yield();
    started.store(true);
    for (int index = 0; index < 50000; index++)
    {
        RenderPerformanceSnapshot snapshot;
        const int value = index % 4096 + 1;
        snapshot.numPaintDrawCalls = value;
        snapshot.numDrawCalls = value * 2;
        snapshot.numTriangles = value * 8;
        snapshot.validationTimeFrame = static_cast<uint64_t>(value);
        store.Publish(snapshot);
        if (index % 31 == 0)
            store.Clear();
    }
    done.store(true);
    first.join();
    second.join();
    require(!invalid.load() && observations.load() >= 2, "concurrent completed snapshots stay coherent");
    store.SetEnabled(false);
    RenderPerformanceSnapshot nonzero;
    nonzero.numPaintDrawCalls = 9;
    nonzero.numTriangles = 72;
    store.Publish(nonzero);
    require(store.Get().numPaintDrawCalls == 0 && store.Get().numTriangles == 0,
        "disabled snapshot rejects an in-flight render publication");
    store.SetEnabled(true);
    require(store.Get().numPaintDrawCalls == 0, "reenabling cannot resurrect old render metrics");
    store.Publish(nonzero);
    require(store.Get().numPaintDrawCalls == 9 && store.Get().numTriangles == 72,
        "reenabled snapshot accepts a fresh completed render");
    store.Clear();
    require(store.Get().numPaintDrawCalls == 0 && store.Get().numTriangles == 0,
        "empty scene snapshot clears completed metrics");
    std::puts("[IMM_RENDER_PERFORMANCE_SNAPSHOT_TEST] coherent concurrent publication, empty reset and disabled publication gate passed");
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
static void checkSpawnCacheLifecycle(piLog * log)
{
    Sequence populated;
    Sequence empty;
    require(populated.Init(Sequence::Type::Still, 0, vec3(0.0f)) &&
        empty.Init(Sequence::Type::Still, 0, vec3(0.0f)), "spawn cache sequences");
    const auto initialize = [&](Layer * layer, Layer::Type type) {
        return layer->Init(type, L"spawn-cache", true, trans3d::identity(), trans3d::identity(),
            1.0f, false, piTick(0), 0, UINT32_MAX, log);
    };
    require(initialize(populated.GetRoot(), Layer::Type::Group) &&
        initialize(empty.GetRoot(), Layer::Type::Group), "spawn cache roots");
    Layer * spawn = populated.CreateLayer(populated.GetRoot());
    require(spawn != nullptr && initialize(spawn, Layer::Type::SpawnArea), "metadata-only spawn layer");
    populated.SetInitialSpawnArea(spawn);
    MngrPlayer manager;
    require(manager.Init(log), "spawn cache manager");
    manager.Enter(&populated, piTick(0));
    require(manager.GetSpawnAreaCount() == 1 && manager.GetSpawnArea() == 0 &&
        manager.GetInitialSpawnArea() == 0, "valid spawn selection preserved");
    require(manager.GetSpawnAreaLayer(-1) == nullptr && manager.GetSpawnAreaLayer(1) == nullptr,
        "invalid spawn cache indices rejected");
    manager.SetSpawnArea(-1);
    manager.SetSpawnArea(1);
    require(manager.GetSpawnArea() == 0, "invalid selection preserves active spawn");
    populated.SetInitialSpawnArea(nullptr);
    manager.Enter(&populated, piTick(1));
    require(manager.GetSpawnAreaCount() == 1 && manager.GetSpawnArea() == -1 &&
        manager.GetInitialSpawnArea() == -1, "same manager enter without default clears old ordinal");
    populated.SetInitialSpawnArea(spawn);
    manager.Enter(&populated, piTick(2));
    require(manager.GetSpawnArea() == 0, "authored default selected after reentry");
    manager.Exit();
    populated.Deinit(log);
    require(manager.GetSpawnAreaCount() == 0 && manager.GetSpawnArea() == -1 &&
        manager.GetInitialSpawnArea() == -1 && manager.GetSpawnAreaLayer(0) == nullptr,
        "exit clears pointers before sequence deletion");
    manager.Enter(&empty, piTick(3));
    require(manager.GetSpawnAreaCount() == 0 && manager.GetSpawnArea() == -1 &&
        manager.GetInitialSpawnArea() == -1, "empty sequence does not inherit active spawn");
    manager.Exit();
    manager.Deinit();
    empty.Deinit(log);
}

int wmain(int argc, wchar_t** argv)
{
    require(argc == 2, "fixture argument");
    piLog log;
    require(log.Init(L"artifacts/authoring-failure-runtime.log", PILOG_TXT), "log init");
    checkLayerInitialization(&log);
    checkSpawnCacheLifecycle(&log);
    checkEmptySpawnCleanup();
    checkRenderPerformanceSnapshots();
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
        const int spawnCount = doc.GetSpawnAreaCount();
        require(spawnCount == 3 && doc.GetSpawnArea() == doc.GetInitialSpawnArea(), "loaded fixture spawn defaults");
        Document::SpawnAreaInfo valid;
        doc.GetSpawnAreaInfo(valid, 0);
        require(valid.mName != nullptr && valid.mVersion > 0, "valid spawn info remains available");
        for (const int invalid : { -1, spawnCount, INT32_MAX })
        {
            Document::SpawnAreaInfo info = valid;
            doc.GetSpawnAreaInfo(info, invalid);
            require(info.mName == nullptr && info.mVersion == 0 &&
                doc.GetSpawnAreaScreenshot(invalid) == nullptr, "invalid spawn queries return empty defaults");
        }
        doc.SetSpawnArea(spawnCount - 1);
        doc.SetSpawnArea(-1);
        doc.SetSpawnArea(spawnCount);
        require(doc.GetSpawnArea() == spawnCount - 1, "invalid document selection preserves last valid ordinal");
    };
    auto unload = [&](uint64_t revision) {
        Document::Command command{}; command.mType = Document::Command::Type::Unload;
        cpu(&command); gpu();
        doc.UnloadSync(nullptr, nullptr, nullptr, nullptr, Drawing::ColorSpace::Linear,
            Drawing::PaintRenderingTechnique::Static, nullptr, nullptr, &log, piTick(1000001));
        require(doc.GetLoadingState() == Document::LoadingState::UnloadingCompleted && !doc.IsEditing(), "unload invalidates session");
        require(doc.GetSpawnAreaCount() == 0 && doc.GetSpawnArea() == -1 && doc.GetInitialSpawnArea() == -1 &&
            doc.GetInitialSpawnAreaLayerId() == -1, "unload clears spawn enumeration and selection");
        for (const int invalid : { -1, 0, 2, INT32_MAX })
        {
            Document::SpawnAreaInfo info{};
            doc.GetSpawnAreaInfo(info, invalid);
            require(info.mName == nullptr && info.mVersion == 0 &&
                doc.GetSpawnAreaScreenshot(invalid) == nullptr, "unloaded spawn queries are safe after layer destruction");
        }
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
    std::puts("[IMM_SPAWN_CACHE_TEST] unload invalid queries, safe bounds, active ordinal reset passed (CPU-only)");
    std::puts("[IMM_AUTHORING_FAILURE_TEST] queued/prepared group unload, invalid parents, root reparent, reload, deleted IDs passed (CPU-only; no GPU cancellation evidence)");
    return 0;
}

