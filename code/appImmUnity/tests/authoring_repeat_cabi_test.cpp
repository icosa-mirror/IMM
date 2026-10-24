// Compile the production export definitions into this owned native host. The
// test initializes their real bridge with a borrowed WARP device, without Unity
// interfaces, camera targets, private-member access or production test hooks.
#include "../src/main.cpp"
#include <d3d11.h>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <limits>

static void requireRepeat(bool condition, const char * message)
{
    if (!condition)
    {
        std::fprintf(stderr, "[IMM_REPEAT_CABI_TEST] %s\n", message);
        std::exit(1);
    }
}

int wmain(int argc, wchar_t ** argv)
{
    requireRepeat(argc == 2, "fixture argument");
    ID3D11Device * device = nullptr;
    ID3D11DeviceContext * context = nullptr;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    requireRepeat(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
        0, &level, 1, D3D11_SDK_VERSION, &device, nullptr, &context)), "create WARP device");
    ImmShared::ImmEngineBridge::InitConfig config{};
    config.rendererApi = piRenderer::API::DX;
    config.graphicsDevice = device;
    config.antialiasing = 8;
    config.enableSound = false;
    config.logFileName = "native-runtime.log";
    config.tmpFolderName = ".";
    requireRepeat(gImmUnityPlugin.mBridge.Init(config), "initialize production bridge");
    auto & player = iPlayer();
    const auto requireEmptyTime = [&](int id) {
        int64_t elapsed = 99, stopped = 99;
        ::GetTime(id, &elapsed, &stopped);
        requireRepeat(elapsed == 0 && stopped == 0 && ::GetPlayTime(id) == 0,
            "unready/invalid playback time is defined and does not dereference root");
    };
    requireEmptyTime(-1);
    requireEmptyTime(INT32_MAX);
    const int doc = player.Load(argv[1]);
    requireRepeat(doc >= 0, "load fixture");
    requireEmptyTime(doc); // No GlobalWork has initialized the root/player manager yet.
    ImmAuthoringTimelineClock untouchedClock;
    std::memset(&untouchedClock, 0xa5, sizeof(untouchedClock));
    const auto originalClockBytes = untouchedClock;
    requireRepeat(ImmAuthoring_TimelineGetClock(doc, 0, &untouchedClock, sizeof(untouchedClock)) ==
        IMM_AUTHORING_INVALID_STATE && std::memcmp(&untouchedClock, &originalClockBytes, sizeof(untouchedClock)) == 0,
        "clock query before load preserves caller memory");
    const auto tick = [&]() {
        gImmUnityPlugin.mBridge.GlobalWork(true);
        player.MaintainGPU();
    };
    const auto wait = [&](const auto & ready) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        do
        {
            tick();
            if (ready()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    };
    requireRepeat(wait([&]() {
        Player::DocumentState state{};
        player.GetDocumentState(state, doc);
        return state.mLoadingState == Player::LoadingState::Loaded;
    }), "fixture reaches loaded state");
    requireRepeat(wait([&]() { return ImmAuthoring_Attach(doc) == IMM_AUTHORING_OK; }), "attach through C export");
    int target = -1;
    int sibling = -1;
    int root = -1;
    for (int index = 0; index < player.GetLayerCount(doc); ++index)
    {
        Player::LayerInfo info{};
        requireRepeat(player.GetLayerInfoByIndex(doc, index, info), "layer query");
        if (info.parentId < 0) root = info.id;
        if (std::wcscmp(info.name, L"KeepPaint") == 0) target = info.id;
        if (std::wcscmp(info.name, L"BlueSibling") == 0) sibling = info.id;
    }
    requireRepeat(target >= 0 && sibling >= 0, "paint fixture layers");
    int32_t isTimeline = -1;
    double duration = -1, offset = -1;
    uint32_t frameRate = 0, frameCount = 0, canonicalRepeat = 0;
    requireRepeat(ImmAuthoring_LayerGetTimeline(doc, target, &isTimeline, &duration, &offset,
        &frameRate, &frameCount, &canonicalRepeat) == IMM_AUTHORING_OK &&
        frameRate == 30 && frameCount == 2 && canonicalRepeat == 1 &&
        std::isfinite(duration) && std::isfinite(offset), "paint timeline through C export");
    uint32_t visibilityCount = 0;
    requireRepeat(ImmAuthoring_KeyGetCount(doc, target, IMM_AUTHORING_ANIM_PROPERTY_VISIBILITY,
        &visibilityCount) == IMM_AUTHORING_OK && visibilityCount == 1, "canonical visibility key count");
    ImmAuthoringKey readKey{};
    readKey.structSize = sizeof(readKey); readKey.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    requireRepeat(ImmAuthoring_KeyGet(doc, target, IMM_AUTHORING_ANIM_PROPERTY_VISIBILITY, 0, &readKey) ==
        IMM_AUTHORING_OK && readKey.boolValue == 1 && readKey.timeSeconds == 0, "read canonical key");
    requireRepeat(ImmAuthoring_KeyGet(doc, target, IMM_AUTHORING_ANIM_PROPERTY_VISIBILITY, visibilityCount, &readKey) ==
        IMM_AUTHORING_NOT_FOUND, "key query rejects out-of-range index");
    readKey.structSize = sizeof(readKey) - 1;
    requireRepeat(ImmAuthoring_KeyGet(doc, target, IMM_AUTHORING_ANIM_PROPERTY_VISIBILITY, 0, &readKey) ==
        IMM_AUTHORING_INVALID_ARGUMENT, "key query rejects undersized output");
    readKey.structSize = sizeof(readKey);
    readKey.structVersion = 99;
    requireRepeat(ImmAuthoring_KeyGet(doc, target, IMM_AUTHORING_ANIM_PROPERTY_VISIBILITY, 0, &readKey) ==
        IMM_AUTHORING_UNSUPPORTED, "key query rejects unknown output version");
    readKey.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    struct GuardedManagedDiagnostics
    {
        SerializedLayerDiagnostics value{};
        unsigned char guard[512];
    } managedDiagnostics;
    std::memset(managedDiagnostics.guard, 0xa5, sizeof(managedDiagnostics.guard));
    Player::LayerDiagnostics internalDiagnostics{};
    requireRepeat(player.GetLayerDiagnostics(doc, target, internalDiagnostics) &&
        ::GetLayerDiagnostics(doc, target, &managedDiagnostics.value), "managed diagnostics export");
    for (unsigned char byte : managedDiagnostics.guard)
        requireRepeat(byte == 0xa5, "managed diagnostics never overwrite the 44-byte caller buffer");
    requireRepeat(managedDiagnostics.value.hasOpacityKeys == internalDiagnostics.hasOpacityKeys &&
        managedDiagnostics.value.hasVisibilityKeys == internalDiagnostics.hasVisibilityKeys &&
        managedDiagnostics.value.isVisible == internalDiagnostics.isVisible &&
        managedDiagnostics.value.opacity == internalDiagnostics.opacity &&
        managedDiagnostics.value.isWorldVisible == internalDiagnostics.isWorldVisible &&
        managedDiagnostics.value.worldOpacity == internalDiagnostics.worldOpacity &&
        managedDiagnostics.value.parentId == internalDiagnostics.parentId &&
        managedDiagnostics.value.visibilityOverrideEnabled == internalDiagnostics.visibilityOverrideEnabled &&
        managedDiagnostics.value.visibilityOverrideValue == internalDiagnostics.visibilityOverrideValue &&
        managedDiagnostics.value.hasTransformKeys == internalDiagnostics.hasTransformKeys &&
        managedDiagnostics.value.transformOverrideEnabled == internalDiagnostics.transformOverrideEnabled,
        "managed diagnostics fields map to the public scalar layout");
    requireRepeat(!::GetLayerDiagnostics(-1, target, &managedDiagnostics.value) &&
        !::GetLayerDiagnostics(doc, target, nullptr), "managed diagnostics invalid arguments reject");
    std::printf("[IMM_MANAGED_DIAGNOSTICS_CABI_TEST] pass=1 exportedBytes=%zu internalBytes=%zu guarded=1\n",
        sizeof(SerializedLayerDiagnostics), sizeof(Player::LayerDiagnostics));
    uint32_t count = 99;
    requireRepeat(root >= 0 && ImmAuthoring_PaintGetMaxRepeatCount(doc, root, &count) == IMM_AUTHORING_UNSUPPORTED && count == 99,
        "non-paint getter preserves caller output");
    requireRepeat(ImmAuthoring_PaintSetMaxRepeatCount(doc, root, 0) == IMM_AUTHORING_UNSUPPORTED,
        "non-paint setter rejected");
    requireRepeat(ImmAuthoring_PaintGetMaxRepeatCount(doc, INT32_MAX, &count) == IMM_AUTHORING_NOT_FOUND && count == 99,
        "unknown layer getter preserves caller output");
    requireRepeat(ImmAuthoring_PaintSetMaxRepeatCount(doc, INT32_MAX, 0) == IMM_AUTHORING_NOT_FOUND,
        "unknown layer setter rejected");
    requireRepeat(ImmAuthoring_PaintGetMaxRepeatCount(doc, target, &count) == IMM_AUTHORING_OK && count == 1,
        "initial canonical count through C export");
    uint32_t previous = count;
    for (uint32_t requested : { 0u, 2u, UINT32_MAX })
    {
        requireRepeat(ImmAuthoring_PaintSetMaxRepeatCount(doc, target, requested) == IMM_AUTHORING_OK,
            "queue count through C export");
        requireRepeat(ImmAuthoring_PaintGetMaxRepeatCount(doc, target, &count) == IMM_AUTHORING_OK && count == previous,
            "queued count remains unpublished");
        uint64_t revision = 0;
        requireRepeat(ImmAuthoring_Commit(doc, &revision) == IMM_AUTHORING_OK && revision != 0,
            "commit through C export");
        requireRepeat(wait([&]() {
            ImmAuthoringCommitStatus status{};
            status.structSize = sizeof(status);
            status.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
            requireRepeat(ImmAuthoring_GetCommitStatus(doc, revision, &status) == IMM_AUTHORING_OK,
                "commit status through C export");
            requireRepeat(status.state != IMM_AUTHORING_COMMIT_REJECTED, "commit not rejected");
            return status.state == IMM_AUTHORING_COMMIT_PRESENTED;
        }), "C export revision presented");
        requireRepeat(ImmAuthoring_PaintGetMaxRepeatCount(doc, target, &count) == IMM_AUTHORING_OK && count == requested,
            "presented count through C export");
        requireRepeat(ImmAuthoring_PaintGetMaxRepeatCount(doc, sibling, &count) == IMM_AUTHORING_OK && count == 1,
            "sibling canonical count preserved");
        previous = requested;
    }
    requireRepeat(ImmAuthoring_PaintSetMaxRepeatCount(doc, target, 1) == IMM_AUTHORING_OK &&
        ImmAuthoring_DiscardPending(doc) == IMM_AUTHORING_OK, "discard through C export");
    requireRepeat(ImmAuthoring_PaintGetMaxRepeatCount(doc, target, &count) == IMM_AUTHORING_OK && count == UINT32_MAX,
        "discard preserves count");
    Player::LayerInfo invalid{};
    requireRepeat(player.GetLayerCount(-1) == 0 && player.GetLayerCount(INT32_MAX) == 0 &&
        !player.GetLayerInfoByIndex(-1, 0, invalid) && !player.GetLayerInfoByIndex(INT32_MAX, 0, invalid),
        "invalid enumeration document IDs reject safely");
    std::atomic<bool> querying{true};
    std::atomic<bool> validSnapshots{true};
    std::atomic<uint64_t> snapshots{0};
    std::thread reader([&]() {
        while (querying.load())
        {
            const int layers = player.GetLayerCount(doc);
            for (int index = 0; index < layers; ++index)
            {
                Player::LayerInfo info{};
                // Count and info are separate snapshots: a removed index may
                // disappear between calls, but a returned record must be owned.
                if (player.GetLayerInfoByIndex(doc, index, info))
                {
                    if (info.id < 0 || info.parentId < -1 || info.name[0] == 0 || info.fullName[0] == 0)
                        validSnapshots.store(false);
                    snapshots.fetch_add(1);
                }
            }
            std::this_thread::yield();
        }
    });
    const auto presentHierarchyEdit = [&]() {
        uint64_t revision = 0;
        const int32_t result = ImmAuthoring_Commit(doc, &revision);
        if (result != IMM_AUTHORING_OK || revision == 0)
            std::fprintf(stderr, "[IMM_REPEAT_CABI_TEST] commit result=%d revision=%llu\n",
                result, static_cast<unsigned long long>(revision));
        requireRepeat(result == IMM_AUTHORING_OK && revision != 0,
            "commit concurrent hierarchy edit");
        requireRepeat(wait([&]() {
            ImmAuthoringCommitStatus status{};
            status.structSize = sizeof(status);
            status.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
            requireRepeat(ImmAuthoring_GetCommitStatus(doc, revision, &status) == IMM_AUTHORING_OK &&
                status.state != IMM_AUTHORING_COMMIT_REJECTED, "concurrent hierarchy status");
            return status.state == IMM_AUTHORING_COMMIT_PRESENTED;
        }), "concurrent hierarchy edit presented");
    };
    for (int iteration = 0; iteration < 100; ++iteration)
    {
        int32_t created = -1;
        const char name[] = "Snapshot fixture";
        requireRepeat(ImmAuthoring_LayerCreate(doc, root, IMM_AUTHORING_LAYER_GROUP,
            name, sizeof(name) - 1, &created) == IMM_AUTHORING_OK, "queue concurrent group creation");
        presentHierarchyEdit();
        requireRepeat(ImmAuthoring_LayerDestroy(doc, created) == IMM_AUTHORING_OK, "queue concurrent group deletion");
        presentHierarchyEdit();
    }
    querying.store(false);
    reader.join();
    requireRepeat(validSnapshots.load() && snapshots.load() > 0, "owned layer snapshots survive concurrent publication");
    std::printf("[IMM_LAYER_QUERY_SNAPSHOT_TEST] pass=1 publications=200 snapshots=%llu perCallSnapshot=1\n",
        static_cast<unsigned long long>(snapshots.load()));
    const auto readName = [&](int layer) {
        uint32_t bytes = 0;
        requireRepeat(ImmAuthoring_LayerGetName(doc, layer, nullptr, 0, &bytes) == IMM_AUTHORING_OK && bytes > 0,
            "query name buffer size");
        std::vector<char> value(bytes);
        requireRepeat(ImmAuthoring_LayerGetName(doc, layer, value.data(), bytes, &bytes) == IMM_AUTHORING_OK,
            "copy owned UTF-8 name");
        return std::string(value.data());
    };
    const auto layerInfo = [&](int id) {
        Player::LayerInfo result{};
        for (int index = 0; index < player.GetLayerCount(doc); ++index)
            if (player.GetLayerInfoByIndex(doc, index, result) && result.id == id) return result;
        requireRepeat(false, "named layer info exists");
        return result;
    };
    requireRepeat(ImmAuthoring_LayerSetName(doc, root, "Root changed", 12) == IMM_AUTHORING_UNSUPPORTED,
        "root label stays read-only");
    const char badUtf8[] = { char(0xc0), char(0xaf) };
    requireRepeat(ImmAuthoring_LayerSetName(doc, target, badUtf8, sizeof(badUtf8)) == IMM_AUTHORING_INVALID_ARGUMENT &&
        ImmAuthoring_LayerSetName(doc, target, "", 0) == IMM_AUTHORING_INVALID_ARGUMENT,
        "malformed and empty names rejected");
    int32_t group = -1, spawn = -1;
    const char groupName[] = "Rename group";
    requireRepeat(ImmAuthoring_LayerCreate(doc, root, IMM_AUTHORING_LAYER_GROUP, groupName,
        sizeof(groupName) - 1, &group) == IMM_AUTHORING_OK, "create rename group");
    presentHierarchyEdit();
    requireRepeat(ImmAuthoring_LayerReparent(doc, target, group, 0) == IMM_AUTHORING_OK, "move paint into rename group");
    presentHierarchyEdit();
    const char spawnName[] = "Spawn before";
    requireRepeat(ImmAuthoring_LayerCreate(doc, group, IMM_AUTHORING_LAYER_SPAWN_AREA,
        spawnName, sizeof(spawnName) - 1, &spawn) == IMM_AUTHORING_OK, "create rename spawn");
    presentHierarchyEdit();
    const int oldActive = player.GetSpawnArea(doc), oldDefault = player.GetInitialSpawnArea(doc);
    uint64_t drawingBefore = 0, drawingAfter = 0;
    requireRepeat(player.GetDrawingHandle(doc, target, 0, drawingBefore), "drawing handle before rename");
    char paintName[] = "Paint \xc3\x85 \xf0\x9f\x9a\x80";
    const std::string expectedPaint = paintName;
    requireRepeat(ImmAuthoring_LayerSetName(doc, target, paintName, sizeof(paintName) - 1) == IMM_AUTHORING_OK,
        "queue UTF-8 paint rename");
    paintName[0] = 'X';
    requireRepeat(readName(target) == "KeepPaint", "queued name remains unpublished and input is copied");
    _putenv_s("IMM_LIVE_EDIT_HOLD_LAYER_RENAME", "1");
    uint64_t nameRevision = 0;
    requireRepeat(ImmAuthoring_Commit(doc, &nameRevision) == IMM_AUTHORING_OK, "commit held rename");
    requireRepeat(wait([&]() {
        ImmAuthoringCommitStatus status{};
        status.structSize = sizeof(status); status.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
        requireRepeat(ImmAuthoring_GetCommitStatus(doc, nameRevision, &status) == IMM_AUTHORING_OK,
            "held rename status");
        return status.state == IMM_AUTHORING_COMMIT_PREPARED;
    }), "rename prepared");
    requireRepeat(readName(target) == "KeepPaint", "prepared name remains unpublished");
    _putenv_s("IMM_LIVE_EDIT_HOLD_LAYER_RENAME", "");
    requireRepeat(wait([&]() { return readName(target) == expectedPaint; }), "UTF-8 paint name presented");
    char tooSmall[] = { '!', 0 };
    uint32_t required = 0;
    requireRepeat(ImmAuthoring_LayerGetName(doc, target, tooSmall, sizeof(tooSmall), &required) ==
        IMM_AUTHORING_INVALID_ARGUMENT && tooSmall[0] == '!' && required == expectedPaint.size() + 1,
        "insufficient buffer reports size without truncation");
    const char renamedGroup[] = "Group \xce\xa9";
    player.SetSpawnAreaNeedsUpdate(doc, false);
    requireRepeat(ImmAuthoring_LayerSetName(doc, group, renamedGroup, sizeof(renamedGroup) - 1) == IMM_AUTHORING_OK,
        "queue ancestor rename");
    presentHierarchyEdit();
    const auto paintInfo = layerInfo(target), spawnInfo = layerInfo(spawn);
    const std::wstring prefix = std::wstring(layerInfo(root).fullName) + L"/Group \u03a9/";
    requireRepeat(std::wstring(paintInfo.fullName) == prefix + L"Paint \u00c5 \U0001f680" &&
        std::wstring(spawnInfo.fullName) == prefix + L"Spawn before" &&
        readName(sibling) == "BlueSibling", "ancestor rename updates descendant paths and preserves sibling");
    requireRepeat(player.GetSpawnAreaNeedsUpdate(doc) && player.GetSpawnAreaCount(doc) == 1 &&
        player.GetSpawnArea(doc) == oldActive && player.GetInitialSpawnArea(doc) == oldDefault &&
        player.GetDrawingHandle(doc, target, 0, drawingAfter) && drawingAfter == drawingBefore,
        "rename notifies labels without changing ordinal selection or drawing handles");
    Document::SpawnAreaInfo savedSpawn{};
    std::wstring savedName;
    requireRepeat(player.GetSpawnAreaInfoSnapshot(savedSpawn, doc, 0, &savedName) && savedName == L"Spawn before",
        "owned spawn name snapshot");
    const char renamedSpawn[] = "Spawn \xe6\x97\xa5 \xf0\x9f\x9a\x80";
    requireRepeat(ImmAuthoring_LayerSetName(doc, spawn, renamedSpawn, sizeof(renamedSpawn) - 1) == IMM_AUTHORING_OK,
        "queue Unicode spawn rename");
    presentHierarchyEdit();
    SerializedSpawnArea serialized{};
    requireRepeat(::GetSpawnAreaInfo(doc, 0, serialized) && std::strcmp(serialized.mName, renamedSpawn) == 0 &&
        std::wstring(savedSpawn.mName) == L"Spawn before", "UTF-8 bridge and old owned snapshot survive rename");
    requireRepeat(ImmAuthoring_LayerSetName(doc, group, "Discarded", 9) == IMM_AUTHORING_OK &&
        ImmAuthoring_DiscardPending(doc) == IMM_AUTHORING_OK && readName(group) == renamedGroup,
        "discard preserves name");
    for (const char * failure : { "IMM_LIVE_EDIT_FAIL_PREPARE_CPU", "IMM_LIVE_EDIT_FAIL_PRESENT" })
    {
        requireRepeat(ImmAuthoring_LayerSetName(doc, group, "Rejected", 8) == IMM_AUTHORING_OK,
            "queue rejected rename");
        uint64_t revision = 0;
        requireRepeat(ImmAuthoring_Commit(doc, &revision) == IMM_AUTHORING_OK, "commit rejected rename");
        _putenv_s(failure, "1");
        ImmAuthoringCommitStatus status{};
        requireRepeat(wait([&]() {
            status.structSize = sizeof(status); status.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
            requireRepeat(ImmAuthoring_GetCommitStatus(doc, revision, &status) == IMM_AUTHORING_OK, "rename rejection status");
            return status.state == IMM_AUTHORING_COMMIT_REJECTED || status.state == IMM_AUTHORING_COMMIT_PRESENTED;
        }), "rename reaches terminal state");
        _putenv_s(failure, "");
        requireRepeat(status.state == IMM_AUTHORING_COMMIT_REJECTED && status.result == -10 &&
            readName(group) == renamedGroup && std::wstring(layerInfo(target).fullName) == paintInfo.fullName,
            "rename failure preserves canonical name and descendant paths");
    }
    ImmAuthoringLayerPivot pivot{};
    pivot.structSize = sizeof(pivot); pivot.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    pivot.qw = 1.0f; pivot.scale = 1.0f;
    ImmAuthoringLayerPivot readPivot = pivot;
    requireRepeat(ImmAuthoring_LayerSetPivot(doc, root, &pivot) == IMM_AUTHORING_UNSUPPORTED,
        "root pivot rejected");
    pivot.flip = 4;
    requireRepeat(ImmAuthoring_LayerSetPivot(doc, target, &pivot) == IMM_AUTHORING_INVALID_ARGUMENT,
        "invalid pivot reflection rejected");
    pivot.flip = 0;
    for (int layer : {target, group, spawn})
    {
        for (int flip = 0; flip <= 3; ++flip)
        {
            requireRepeat(ImmAuthoring_LayerGetPivot(doc, layer, &readPivot) == IMM_AUTHORING_OK,
                "read initial presented pivot");
            const auto oldPivot = readPivot;
            pivot.tx = static_cast<float>(flip + 1); pivot.flip = flip; pivot.scale = 1.5f;
            requireRepeat(ImmAuthoring_LayerSetPivot(doc, layer, &pivot) == IMM_AUTHORING_OK &&
                ImmAuthoring_LayerGetPivot(doc, layer, &readPivot) == IMM_AUTHORING_OK &&
                readPivot.tx == oldPivot.tx && readPivot.flip == oldPivot.flip,
                "queued pivot preserves presented state");
            presentHierarchyEdit();
            requireRepeat(ImmAuthoring_LayerGetPivot(doc, layer, &readPivot) == IMM_AUTHORING_OK &&
                readPivot.tx == pivot.tx && readPivot.scale == pivot.scale && readPivot.flip == flip,
                "pivot publishes through production C exports");
        }
    }
    pivot.tx = 99.0f;
    requireRepeat(ImmAuthoring_LayerSetPivot(doc, target, &pivot) == IMM_AUTHORING_OK &&
        ImmAuthoring_DiscardPending(doc) == IMM_AUTHORING_OK &&
        ImmAuthoring_LayerGetPivot(doc, target, &readPivot) == IMM_AUTHORING_OK && readPivot.tx == 4.0f,
        "discard preserves pivot");
    for (const char * failure : {"IMM_LIVE_EDIT_FAIL_PREPARE_CPU", "IMM_LIVE_EDIT_FAIL_PRESENT"})
    {
        requireRepeat(ImmAuthoring_LayerSetPivot(doc, target, &pivot) == IMM_AUTHORING_OK,
            "queue rejected pivot");
        uint64_t revision = 0;
        requireRepeat(ImmAuthoring_Commit(doc, &revision) == IMM_AUTHORING_OK, "commit rejected pivot");
        _putenv_s(failure, "1");
        ImmAuthoringCommitStatus status{};
        requireRepeat(wait([&]() {
            status.structSize = sizeof(status); status.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
            requireRepeat(ImmAuthoring_GetCommitStatus(doc, revision, &status) == IMM_AUTHORING_OK, "pivot status");
            return status.state == IMM_AUTHORING_COMMIT_REJECTED || status.state == IMM_AUTHORING_COMMIT_PRESENTED;
        }), "pivot reaches terminal state");
        _putenv_s(failure, "");
        requireRepeat(status.state == IMM_AUTHORING_COMMIT_REJECTED && status.result == -10 &&
            ImmAuthoring_LayerGetPivot(doc, target, &readPivot) == IMM_AUTHORING_OK && readPivot.tx == 4.0f,
            "pivot rejection preserves presented state");
    }
    uint64_t pivotDrawing = 0;
    requireRepeat(player.GetDrawingHandle(doc, target, 0, pivotDrawing) && pivotDrawing == drawingBefore &&
        player.GetSpawnArea(doc) == oldActive && player.GetInitialSpawnArea(doc) == oldDefault,
        "pivot preserves drawing handles and spawn selection");
    std::puts("[IMM_LAYER_PIVOT_CABI_TEST] pass=1 groupPaintSpawn=1 allFlips=1 queuedState=1 discardAndRejection=1");
    const std::string batchOldName = readName(target);
    ImmAuthoringLayerProperties batchProperties{};
    batchProperties.structSize = sizeof(batchProperties); batchProperties.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    batchProperties.updateMask = IMM_AUTHORING_LAYER_PROPERTY_OPACITY;
    batchProperties.opacity = 0.25f;
    requireRepeat(ImmAuthoring_LayerSetName(doc, target, "First", 5) == IMM_AUTHORING_OK &&
        ImmAuthoring_LayerSetProperties(doc, target, &batchProperties) == IMM_AUTHORING_OK &&
        ImmAuthoring_LayerSetPivot(doc, target, &pivot) == IMM_AUTHORING_OK &&
        ImmAuthoring_PaintSetMaxRepeatCount(doc, target, 7) == IMM_AUTHORING_OK &&
        ImmAuthoring_LayerSetName(doc, target, "Combined", 8) == IMM_AUTHORING_OK &&
        ImmAuthoring_PaintSetMaxRepeatCount(doc, target, 2) == IMM_AUTHORING_OK,
        "same-layer metadata setters accumulate with last value per field");
    requireRepeat(ImmAuthoring_LayerSetName(doc, sibling, "Other", 5) == IMM_AUTHORING_QUEUE_FULL &&
        ImmAuthoring_LayerSetName(doc, target, badUtf8, sizeof(badUtf8)) == IMM_AUTHORING_INVALID_ARGUMENT,
        "cross-layer and invalid subsequent commands preserve accepted metadata");
    requireRepeat(readName(target) == batchOldName &&
        ImmAuthoring_LayerGetPivot(doc, target, &readPivot) == IMM_AUTHORING_OK && readPivot.tx == 4.0f &&
        ImmAuthoring_PaintGetMaxRepeatCount(doc, target, &count) == IMM_AUTHORING_OK && count == UINT32_MAX,
        "combined metadata remains unpublished while queued");
    presentHierarchyEdit();
    ImmAuthoringLayerProperties batchRead{};
    batchRead.structSize = sizeof(batchRead); batchRead.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    requireRepeat(readName(target) == "Combined" &&
        ImmAuthoring_LayerGetProperties(doc, target, &batchRead) == IMM_AUTHORING_OK && batchRead.opacity == 0.25f &&
        ImmAuthoring_LayerGetPivot(doc, target, &readPivot) == IMM_AUTHORING_OK && readPivot.tx == 99.0f &&
        ImmAuthoring_PaintGetMaxRepeatCount(doc, target, &count) == IMM_AUTHORING_OK && count == 2,
        "one revision publishes name, opacity, pivot and final repeat count");
    requireRepeat(ImmAuthoring_LayerSetName(doc, target, "Discard", 7) == IMM_AUTHORING_OK &&
        ImmAuthoring_PaintSetMaxRepeatCount(doc, target, 9) == IMM_AUTHORING_OK &&
        ImmAuthoring_DiscardPending(doc) == IMM_AUTHORING_OK && readName(target) == "Combined" &&
        ImmAuthoring_PaintGetMaxRepeatCount(doc, target, &count) == IMM_AUTHORING_OK && count == 2,
        "discard drops all accumulated fields");
    std::puts("[IMM_METADATA_BATCH_CABI_TEST] pass=1 combinedPublication=1 lastValueWins=1 invalidPreservesQueue=1 discard=1");
    ImmAuthoringLayerProperties reflected{};
    reflected.structSize = sizeof(reflected); reflected.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    reflected.updateMask = IMM_AUTHORING_LAYER_PROPERTY_TRANSFORM;
    reflected.qw = 1.0f; reflected.scale = 1.0f;
    ImmAuthoringLayerProperties reflectionRead = reflected;
    for (int flip = 0; flip <= 3; ++flip)
    {
        requireRepeat(ImmAuthoring_LayerGetProperties(doc, target, &reflectionRead) == IMM_AUTHORING_OK,
            "read canonical reflection");
        const int oldFlip = reflectionRead.transformFlip;
        reflected.transformFlip = flip; reflected.tx = static_cast<float>(flip);
        requireRepeat(ImmAuthoring_LayerSetProperties(doc, target, &reflected) == IMM_AUTHORING_OK &&
            ImmAuthoring_LayerGetProperties(doc, target, &reflectionRead) == IMM_AUTHORING_OK &&
            reflectionRead.transformFlip == oldFlip, "queued reflection remains unpublished");
        presentHierarchyEdit();
        requireRepeat(ImmAuthoring_LayerGetProperties(doc, target, &reflectionRead) == IMM_AUTHORING_OK &&
            reflectionRead.transformFlip == flip && reflectionRead.tx == reflected.tx,
            "publishes all four transform reflection values");
    }
    reflected.transformFlip = 4;
    requireRepeat(ImmAuthoring_LayerSetProperties(doc, target, &reflected) == IMM_AUTHORING_INVALID_ARGUMENT,
        "invalid reflection rejects");
    reflected.transformFlip = 0; reflected.structSize = sizeof(ImmAuthoringLayerProperties) - 4;
    requireRepeat(ImmAuthoring_LayerSetProperties(doc, target, &reflected) == IMM_AUTHORING_INVALID_ARGUMENT &&
        ImmAuthoring_LayerGetProperties(doc, target, &reflected) == IMM_AUTHORING_INVALID_ARGUMENT,
        "truncated properties reject");
    reflected.structSize = sizeof(reflected);
    requireRepeat(ImmAuthoring_LayerSetProperties(doc, target, &reflected) == IMM_AUTHORING_OK,
        "explicitly clear transform reflection");
    presentHierarchyEdit();
    requireRepeat(ImmAuthoring_LayerGetProperties(doc, target, &reflectionRead) == IMM_AUTHORING_OK &&
        reflectionRead.transformFlip == 0, "explicit None publishes an unreflected transform");
    std::puts("[IMM_REFLECTION_CABI_TEST] pass=1 allFlips=1 queuedState=1 invalidAndTruncated=1 explicitClear=1");
    ImmAuthoringKey offsetKey{};
    offsetKey.structSize = sizeof(offsetKey); offsetKey.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    offsetKey.property = IMM_AUTHORING_ANIM_PROPERTY_OFFSET; offsetKey.uintValue = UINT32_MAX;
    requireRepeat(ImmAuthoring_KeySet(doc, target, &offsetKey) == IMM_AUTHORING_OK,
        "direct key layout submits full-width Offset ticks");
    presentHierarchyEdit();
    requireRepeat(ImmAuthoring_KeyGet(doc, target, IMM_AUTHORING_ANIM_PROPERTY_OFFSET, 0, &readKey) ==
        IMM_AUTHORING_OK && readKey.uintValue == UINT32_MAX && readKey.timeSeconds == 0,
        "key query preserves all unsigned Offset bits");
    requireRepeat(ImmAuthoring_KeyRemove(doc, target, IMM_AUTHORING_ANIM_PROPERTY_OFFSET, 0.0) == IMM_AUTHORING_OK,
        "remove published Offset key");
    presentHierarchyEdit();
    player.Pause(doc);
    requireRepeat(wait([&]() {
        Player::DocumentState state{};
        player.GetDocumentState(state, doc);
        return state.mPlaybackState == Player::PlaybackState::Paused;
    }), "hold playback for transform key edits");
    ImmAuthoringKey transformKey{};
    transformKey.structSize = sizeof(transformKey); transformKey.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    transformKey.property = IMM_AUTHORING_ANIM_PROPERTY_TRANSFORM;
    transformKey.qw = 1.0f; transformKey.scale = 1.0f; transformKey.tx = 17.0f;
    Player::LayerDiagnostics heldTransform{};
    for (int flip = 0; flip <= 3; ++flip)
    {
        transformKey.transformFlip = flip;
        requireRepeat(ImmAuthoring_KeySet(doc, target, &transformKey) == IMM_AUTHORING_OK, "queue held transform key");
        presentHierarchyEdit();
        requireRepeat(player.GetLayerDiagnostics(doc, target, heldTransform) && heldTransform.transform.mTranslation.x == 17.0 &&
            static_cast<int>(heldTransform.transform.mFlip) == flip,
            "presented transform key changes the held reflected pose");
        requireRepeat(ImmAuthoring_KeyGet(doc, target, IMM_AUTHORING_ANIM_PROPERTY_TRANSFORM, 0, &readKey) ==
            IMM_AUTHORING_OK && readKey.tx == 17.0f && readKey.transformFlip == flip,
            "key query preserves reflected transform fields");
        ImmAuthoringLayerPivot world{};
        world.structSize = sizeof(world); world.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
        ImmCore::trans3d expectedWorld;
        requireRepeat(player.GetLayerWorldTransform(doc, target, expectedWorld) == IMM_AUTHORING_OK &&
            ImmAuthoring_LayerGetWorldTransform(doc, target, &world) == IMM_AUTHORING_OK &&
            std::fabs(world.tx - expectedWorld.mTranslation.x) < 0.00001 &&
            std::fabs(world.ty - expectedWorld.mTranslation.y) < 0.00001 &&
            std::fabs(world.tz - expectedWorld.mTranslation.z) < 0.00001 &&
            world.flip == static_cast<int32_t>(expectedWorld.mFlip), "evaluated accumulated transform export");
    }
    requireRepeat(ImmAuthoring_KeyRemove(doc, target, IMM_AUTHORING_ANIM_PROPERTY_TRANSFORM, 0.0) == IMM_AUTHORING_OK,
        "remove held transform key");
    presentHierarchyEdit();
    requireRepeat(player.GetLayerDiagnostics(doc, target, heldTransform) &&
        heldTransform.transform.mTranslation.x == heldTransform.canonicalTransform.mTranslation.x,
        "last transform key removal restores canonical held pose");
    ImmAuthoringKey opacityKey{};
    opacityKey.structSize = sizeof(opacityKey); opacityKey.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    opacityKey.property = IMM_AUTHORING_ANIM_PROPERTY_OPACITY; opacityKey.floatValue = 0.9f;
    requireRepeat(ImmAuthoring_KeySet(doc, target, &opacityKey) == IMM_AUTHORING_OK, "queue held opacity key");
    presentHierarchyEdit();
    requireRepeat(player.GetLayerDiagnostics(doc, target, heldTransform) && heldTransform.opacity == 0.9f,
        "presented opacity key changes the held opacity");
    requireRepeat(ImmAuthoring_KeyRemove(doc, target, IMM_AUTHORING_ANIM_PROPERTY_OPACITY, 0.0) == IMM_AUTHORING_OK,
        "remove held opacity key");
    presentHierarchyEdit();
    requireRepeat(player.GetLayerDiagnostics(doc, target, heldTransform) && heldTransform.opacity == heldTransform.canonicalOpacity,
        "last opacity key removal restores canonical held opacity");
    ImmAuthoringKey visibilityKey{};
    visibilityKey.structSize = sizeof(visibilityKey); visibilityKey.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    visibilityKey.property = IMM_AUTHORING_ANIM_PROPERTY_VISIBILITY;
    requireRepeat(ImmAuthoring_KeySet(doc, target, &visibilityKey) == IMM_AUTHORING_OK, "queue held visibility key");
    presentHierarchyEdit();
    requireRepeat(player.GetLayerDiagnostics(doc, target, heldTransform) && heldTransform.isVisible == 0,
        "presented paint visibility key changes held visibility");
    requireRepeat(ImmAuthoring_KeyRemove(doc, target, IMM_AUTHORING_ANIM_PROPERTY_VISIBILITY, 0.0) == IMM_AUTHORING_OK,
        "remove held visibility key");
    presentHierarchyEdit();
    requireRepeat(player.GetLayerDiagnostics(doc, target, heldTransform) && heldTransform.isVisible == heldTransform.canonicalVisible,
        "last paint visibility key removal restores canonical held visibility");
    requireRepeat(ImmAuthoring_KeySet(doc, group, &visibilityKey) == IMM_AUTHORING_OK, "queue held group visibility key");
    presentHierarchyEdit();
    requireRepeat(player.GetLayerDiagnostics(doc, group, heldTransform) && heldTransform.isVisible == 0 &&
        player.GetLayerDiagnostics(doc, target, heldTransform) && heldTransform.isWorldVisible == 0,
        "presented group visibility refreshes its held subtree");
    requireRepeat(ImmAuthoring_KeyRemove(doc, group, IMM_AUTHORING_ANIM_PROPERTY_VISIBILITY, 0.0) == IMM_AUTHORING_OK,
        "remove held group visibility key");
    presentHierarchyEdit();
    requireRepeat(player.GetLayerDiagnostics(doc, group, heldTransform) &&
        heldTransform.isVisible == heldTransform.canonicalVisible &&
        player.GetLayerDiagnostics(doc, target, heldTransform) && heldTransform.isWorldVisible == 1,
        "group key removal restores canonical held subtree visibility");
    ImmAuthoringKey drawInKey{};
    drawInKey.structSize = sizeof(drawInKey); drawInKey.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    drawInKey.property = IMM_AUTHORING_ANIM_PROPERTY_DRAW_IN_TIME;
    drawInKey.doubleValue = 1234567890123456.0;
    requireRepeat(static_cast<double>(static_cast<float>(drawInKey.doubleValue)) != drawInKey.doubleValue,
        "DrawInTime precision fixture cannot fit in float");
    uint32_t drawInCount = 99;
    requireRepeat(ImmAuthoring_KeyGetCount(doc, target, drawInKey.property, &drawInCount) == IMM_AUTHORING_OK && drawInCount == 0,
        "DrawInTime fixture starts with no keys");
    int64_t drawInStart = 0, drawInStop = 0;
    ::GetTime(doc, &drawInStart, &drawInStop);
    requireRepeat(ImmAuthoring_KeySet(doc, target, &drawInKey) == IMM_AUTHORING_OK, "queue paused DrawInTime double key");
    presentHierarchyEdit();
    requireRepeat(ImmAuthoring_KeyGet(doc, target, drawInKey.property, 0, &readKey) == IMM_AUTHORING_OK &&
        readKey.doubleValue == drawInKey.doubleValue && readKey.timeSeconds == 0,
        "published DrawInTime preserves full double precision through C ABI");
    for (double invalid : {-1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
        drawInKey.doubleValue = invalid;
        requireRepeat(ImmAuthoring_KeySet(doc, target, &drawInKey) == IMM_AUTHORING_INVALID_ARGUMENT,
            "DrawInTime rejects negative and nonfinite values");
    }
    requireRepeat(ImmAuthoring_KeyGet(doc, target, drawInKey.property, 0, &readKey) == IMM_AUTHORING_OK &&
        readKey.doubleValue == 1234567890123456.0, "invalid DrawInTime requests preserve published key");
    requireRepeat(ImmAuthoring_KeyRemove(doc, target, drawInKey.property, 0) == IMM_AUTHORING_OK,
        "remove paused DrawInTime key");
    presentHierarchyEdit();
    int64_t drawInAfterStart = 0, drawInAfterStop = 0;
    ::GetTime(doc, &drawInAfterStart, &drawInAfterStop);
    Player::DocumentState drawInDocumentState{};
    player.GetDocumentState(drawInDocumentState, doc);
    uint64_t drawInDrawing = 0;
    requireRepeat(ImmAuthoring_KeyGetCount(doc, target, drawInKey.property, &drawInCount) == IMM_AUTHORING_OK && drawInCount == 0 &&
        drawInDocumentState.mPlaybackState == Player::PlaybackState::Paused &&
        drawInStart == drawInAfterStart && drawInStop > 0 && drawInAfterStop >= drawInStop &&
        player.GetDrawingHandle(doc, target, 0, drawInDrawing) && drawInDrawing == drawingBefore,
        "DrawInTime publication/removal preserves paused clock and drawing identity");
    std::puts("[IMM_DRAW_IN_TIME_CABI_TEST] pass=1 property5=1 doublePrecision=1 invalidRejected=1 pausedPublicationAndRemoval=1 stableDrawing=1");
    ImmAuthoringKey movedKey{};
    movedKey.structSize = sizeof(movedKey); movedKey.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    movedKey.property = IMM_AUTHORING_ANIM_PROPERTY_OPACITY;
    movedKey.timeSeconds = 0.125; movedKey.floatValue = 0.4f;
    requireRepeat(ImmAuthoring_KeySet(doc, target, &movedKey) == IMM_AUTHORING_OK, "seed movable key");
    presentHierarchyEdit();
    movedKey.timeSeconds = 0.25;
    requireRepeat(ImmAuthoring_KeyRemove(doc, target, movedKey.property, 0.125) == IMM_AUTHORING_OK &&
        ImmAuthoring_KeySet(doc, target, &movedKey) == IMM_AUTHORING_OK,
        "queue ordered same-track remove/set");
    requireRepeat(ImmAuthoring_KeyGet(doc, target, movedKey.property, 0, &readKey) == IMM_AUTHORING_OK &&
        readKey.timeSeconds == 0.125, "queued retime preserves complete old track");
    presentHierarchyEdit();
    requireRepeat(ImmAuthoring_KeyGetCount(doc, target, movedKey.property, &visibilityCount) == IMM_AUTHORING_OK &&
        visibilityCount == 1 && ImmAuthoring_KeyGet(doc, target, movedKey.property, 0, &readKey) == IMM_AUTHORING_OK &&
        readKey.timeSeconds == 0.25 && readKey.floatValue == 0.4f, "retime publishes complete replacement once");
    const auto requireRejectedTrackBatch = [&](int32_t expected) {
        uint64_t revision = 0;
        requireRepeat(ImmAuthoring_Commit(doc, &revision) == IMM_AUTHORING_OK && revision != 0, "seal rejected track batch");
        ImmAuthoringCommitStatus status{};
        requireRepeat(wait([&]() {
            status.structSize = sizeof(status); status.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
            requireRepeat(ImmAuthoring_GetCommitStatus(doc, revision, &status) == IMM_AUTHORING_OK, "track batch status");
            return status.state == IMM_AUTHORING_COMMIT_REJECTED || status.state == IMM_AUTHORING_COMMIT_PRESENTED;
        }), "track batch terminal status");
        requireRepeat(status.state == IMM_AUTHORING_COMMIT_REJECTED && status.result == expected &&
            ImmAuthoring_KeyGet(doc, target, movedKey.property, 0, &readKey) == IMM_AUTHORING_OK &&
            readKey.timeSeconds == 0.25, "rejected batch preserves entire original track");
    };
    requireRepeat(ImmAuthoring_KeyRemove(doc, target, movedKey.property, 0.25) == IMM_AUTHORING_OK &&
        ImmAuthoring_KeyRemove(doc, target, movedKey.property, 0.75) == IMM_AUTHORING_OK,
        "queue invalid second removal after valid removal");
    requireRejectedTrackBatch(IMM_AUTHORING_NOT_FOUND);
    requireRepeat(ImmAuthoring_KeyRemove(doc, target, movedKey.property, 0.25) == IMM_AUTHORING_OK &&
        ImmAuthoring_KeySet(doc, sibling, &movedKey) == IMM_AUTHORING_OK, "queue mixed layer track batch");
    requireRejectedTrackBatch(IMM_AUTHORING_UNSUPPORTED);
    requireRepeat(ImmAuthoring_KeyGetCount(doc, sibling, movedKey.property, &visibilityCount) == IMM_AUTHORING_OK &&
        visibilityCount == 0, "mixed layer batch preserves sibling track");
    requireRepeat(ImmAuthoring_KeyRemove(doc, target, movedKey.property, 0.25) == IMM_AUTHORING_OK &&
        ImmAuthoring_KeySet(doc, target, &offsetKey) == IMM_AUTHORING_OK, "queue mixed channel track batch");
    requireRejectedTrackBatch(IMM_AUTHORING_UNSUPPORTED);
    requireRepeat(ImmAuthoring_KeyGetCount(doc, target, IMM_AUTHORING_ANIM_PROPERTY_OFFSET, &visibilityCount) ==
        IMM_AUTHORING_OK && visibilityCount == 0, "mixed channel batch preserves Offset track");
    requireRepeat(ImmAuthoring_KeyRemove(doc, target, movedKey.property, 0.25) == IMM_AUTHORING_OK, "remove moved key");
    presentHierarchyEdit();
    // 24 fps frame 13 is exactly 6825 native ticks, but its seconds double
    // multiplies back to 6824.999999999999. Exercise the exported round trip.
    Layer::AnimValue exactTickValue;
    exactTickValue.init(); exactTickValue.mFloat = 0.7f;
    requireRepeat(player.QueueAnimationKey(doc, target, Layer::AnimProperty::Opacity,
        piTick(6825), exactTickValue, Layer::InterpolationType::None) == IMM_AUTHORING_OK,
        "seed exact native 24 fps key tick");
    presentHierarchyEdit();
    requireRepeat(ImmAuthoring_KeyGet(doc, target, movedKey.property, 0, &readKey) == IMM_AUTHORING_OK &&
        readKey.timeSeconds == 6825.0 / 12600.0, "read non-binary-exact native key time");
    requireRepeat(ImmAuthoring_KeySet(doc, target, &readKey) == IMM_AUTHORING_OK,
        "write queried native key without moving its tick");
    presentHierarchyEdit();
    Layer::AnimKey exactRead{};
    requireRepeat(ImmAuthoring_KeyGetCount(doc, target, movedKey.property, &visibilityCount) == IMM_AUTHORING_OK &&
        visibilityCount == 1 && player.GetAnimationKey(doc, target, Layer::AnimProperty::Opacity, 0, exactRead) ==
        IMM_AUTHORING_OK && exactRead.mTime == piTick(6825), "get/set round trip preserves exact tick and key count");
    requireRepeat(ImmAuthoring_KeyRemove(doc, target, movedKey.property, readKey.timeSeconds) == IMM_AUTHORING_OK,
        "remove exact native key through queried seconds");
    presentHierarchyEdit();
    requireRepeat(ImmAuthoring_KeyGetCount(doc, target, movedKey.property, &visibilityCount) == IMM_AUTHORING_OK &&
        visibilityCount == 0, "get/remove round trip removes the original native tick");
    for (double invalidTime : { -1.0, std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::max(),
        static_cast<double>(INT64_MAX) / static_cast<double>(piTick::FromOneSecond()) })
    {
        movedKey.timeSeconds = invalidTime;
        requireRepeat(ImmAuthoring_KeySet(doc, target, &movedKey) == IMM_AUTHORING_INVALID_ARGUMENT &&
            ImmAuthoring_KeyRemove(doc, target, movedKey.property, invalidTime) == IMM_AUTHORING_INVALID_ARGUMENT,
            "authoring key time rejects negative/nonfinite/overflow values before integer conversion");
    }
    requireRepeat(ImmAuthoring_KeyGetCount(doc, target, movedKey.property, &visibilityCount) == IMM_AUTHORING_OK &&
        visibilityCount == 0, "invalid time requests leave canonical key track unchanged");
    std::puts("[IMM_KEY_TIME_QUANTIZATION_TEST] pass=1 exact24fpsTick=6825 getSetRemove=1 invalidTimesRejected=1");
    std::puts("[IMM_TIMELINE_QUERY_RETIME_TEST] pass=1 safeUnreadyTime=1 keyQueries=1 exactOffset=1 reflectedWorldTransform=1 atomicMove=1 invalidRemovalRollback=1 mixedTracksRollback=1");
    requireRepeat(ImmAuthoring_LayerDestroy(doc, group) == IMM_AUTHORING_OK, "remove renamed subtree");
    presentHierarchyEdit();
    requireRepeat(std::wstring(savedSpawn.mName) == L"Spawn before" && player.GetSpawnAreaCount(doc) == 0,
        "owned spawn name survives deletion");
    std::puts("[IMM_LAYER_RENAME_CABI_TEST] pass=1 paintGroupSpawn=1 utf8=1 copiedInput=1 queuedPreparedPreserved=1 descendants=1 stableHandles=1 ownedSpawnName=1 discardAndRejection=1");
    requireRepeat(ImmAuthoring_Detach(doc) == IMM_AUTHORING_OK, "detach through C export");
    count = 123;
    requireRepeat(ImmAuthoring_PaintGetMaxRepeatCount(doc, target, &count) == IMM_AUTHORING_INVALID_STATE && count == 123,
        "detached getter preserves caller output");
    requireRepeat(ImmAuthoring_PaintSetMaxRepeatCount(doc, target, 0) == IMM_AUTHORING_INVALID_STATE,
        "detached setter rejected");
    // A separate group-only document exercises actual nested clocks without
    // touching the paint fixture's authoring session or introducing test hooks.
    void * clockSequence = ImmExporter_CreateSequence(1, 0, 0, 0, 0, 30, 0, 0, 0, 0);
    requireRepeat(clockSequence != nullptr, "create timeline clock fixture");
    ImmExporterTransformC identity{};
    identity.qw = 1.0f; identity.scale = 1.0f;
    requireRepeat(ImmExporter_SetRootLayerProperties(clockSequence, 1, 1.0f, &identity, &identity,
        0, 0, 1, 1000, 0), "clock fixture root timing");
    void * clockRoot = ImmExporter_GetRootLayer(clockSequence);
    void * finiteTimeline = ImmExporter_CreateGroupLayer(clockSequence, nullptr, "FiniteClock", 1, 1.0f,
        &identity, &identity, 1, 100, 2);
    void * infiniteTimeline = ImmExporter_CreateGroupLayer(clockSequence, nullptr, "InfiniteClock", 1, 1.0f,
        &identity, &identity, 1, 100, 0);
    void * inactiveTimeline = ImmExporter_CreateGroupLayer(clockSequence, nullptr, "InactiveClock", 1, 1.0f,
        &identity, &identity, 1, 100, 1);
    requireRepeat(finiteTimeline && infiniteTimeline && inactiveTimeline, "clock fixture timelines");
    void * previewChild = ImmExporter_CreateGroupLayer(clockSequence, finiteTimeline, "PreviewChild", 1, 1.0f,
        &identity, &identity, 0, 0, 1);
    requireRepeat(previewChild && ImmExporter_LayerAddAnimationKey(previewChild, 0, 0, 0, 1, 0, 0, 0, &identity),
        "preview fixture child visibility");
    ImmExporterTransformC laterPlacement = identity;
    laterPlacement.tx = 9;
    requireRepeat(ImmExporter_LayerAddAnimationKey(previewChild, 1, 0, 0, 0, 0, 1.0f, 0, &identity) &&
        ImmExporter_LayerAddAnimationKey(previewChild, 1, 90, 0, 0, 0, 0.75f, 0, &identity) &&
        ImmExporter_LayerAddAnimationKey(previewChild, 9, 0, 0, 0, 0, 0, 0, &identity) &&
        ImmExporter_LayerAddAnimationKey(previewChild, 9, 90, 0, 0, 0, 0, 0, &laterPlacement),
        "preview fixture separates preview-local and held-main key selections");
    void * previewPaint = ImmExporter_CreatePaintLayer(clockSequence, finiteTimeline, "PreviewPaint", 1, 1.0f,
        &identity, &identity, 0, 0, 1);
    requireRepeat(previewPaint != nullptr, "preview fixture paint");
    for (uint32_t drawingIndex = 0; drawingIndex < 2; ++drawingIndex)
    {
        void * drawing = ImmExporter_CreateDrawing(previewPaint);
        requireRepeat(drawing && ImmExporter_DrawingInit(drawing, 1, 0), "preview fixture drawing");
        void * element = ImmExporter_DrawingGetElement(drawing, 0);
        requireRepeat(element && ImmExporter_ElementInit(element, 2, 2, 1), "preview fixture Circle element");
        ImmExporterPointC points[2]{};
        for (uint32_t index = 0; index < 2; ++index)
        {
            points[index].px = -0.5f + drawingIndex * 0.3f + index * 0.2f;
            points[index].py = 1.5f; points[index].ny = 1.0f; points[index].dz = 1.0f;
            points[index].r = 1.0f; points[index].a = 1.0f; points[index].width = 0.03f;
            points[index].length = index * 0.2f; points[index].time = static_cast<float>(index);
        }
        requireRepeat(ImmExporter_ElementSetPoints(element, 0, points, 2), "preview fixture points");
        ImmExporter_ComputeElementBounds(element); ImmExporter_ComputeDrawingBounds(drawing);
        ImmExporter_PaintAddFrame(previewPaint, ImmExporter_GetDrawingIndex(drawing));
        ImmExporter_DestroyDrawing(drawing);
    }
    for (void * layer : {finiteTimeline, infiniteTimeline, inactiveTimeline})
        requireRepeat(ImmExporter_LayerAddAnimationKey(layer, 0, layer == inactiveTimeline ? 500 : 100,
            0, 1, 0, 0, 0, &identity), "clock fixture visibility span");
    requireRepeat(ImmExporter_LayerAddAnimationKey(finiteTimeline, 8, 0, 0, 0, 20, 0, 0, &identity),
        "clock fixture finite offset");
    requireRepeat(ImmExporter_LayerAddAnimationKey(clockRoot, 6, 700, 0, 0,
        static_cast<uint32_t>(Layer::AnimAction::Stop), 0, 0, &identity), "clock fixture Stop action");
    requireRepeat(ImmExporter_ExportToFile(clockSequence, "timeline-clock.imm", 96000, 0), "export timeline clock fixture");
    ImmExporter_DestroySequence(clockSequence);
    const int clockDoc = player.Load(L"timeline-clock.imm");
    requireRepeat(clockDoc >= 0 && wait([&]() {
        Player::DocumentState state{};
        player.GetDocumentState(state, clockDoc);
        return state.mLoadingState == Player::LoadingState::Loaded;
    }), "load timeline clock fixture");
    int clockRootId = -1, finiteId = -1, infiniteId = -1, inactiveId = -1, previewChildId = -1, previewPaintId = -1;
    for (int index = 0; index < player.GetLayerCount(clockDoc); ++index)
    {
        Player::LayerInfo info{};
        requireRepeat(player.GetLayerInfoByIndex(clockDoc, index, info), "clock fixture layer enumeration");
        if (info.parentId < 0) clockRootId = info.id;
        if (std::wcscmp(info.name, L"FiniteClock") == 0) finiteId = info.id;
        if (std::wcscmp(info.name, L"InfiniteClock") == 0) infiniteId = info.id;
        if (std::wcscmp(info.name, L"InactiveClock") == 0) inactiveId = info.id;
        if (std::wcscmp(info.name, L"PreviewChild") == 0) previewChildId = info.id;
        if (std::wcscmp(info.name, L"PreviewPaint") == 0) previewPaintId = info.id;
    }
    requireRepeat(clockRootId >= 0 && finiteId >= 0 && infiniteId >= 0 && inactiveId >= 0 && previewChildId >= 0 && previewPaintId >= 0,
        "clock fixture layer IDs");
    requireRepeat(wait([&]() {
        ImmAuthoringTimelineClock ready{};
        return ImmAuthoring_TimelineGetClock(clockDoc, clockRootId, &ready, sizeof(ready)) == IMM_AUTHORING_OK;
    }), "clock fixture CPU evaluation and paint ownership ready");
    player.Pause(clockDoc);
    requireRepeat(wait([&]() {
        Player::DocumentState state{};
        player.GetDocumentState(state, clockDoc);
        return state.mPlaybackState == Player::PlaybackState::Paused;
    }), "pause clock fixture");
    const auto readClock = [&](int layer) {
        ImmAuthoringTimelineClock value{};
        requireRepeat(ImmAuthoring_TimelineGetClock(clockDoc, layer, &value, sizeof(value)) == IMM_AUTHORING_OK &&
            value.reserved == 0 && value.evaluationRevision != 0, "read clock without attaching editing");
        return value;
    };
    player.SetTime(clockDoc, piTick(351), piTick(1)); // Evaluated root tick350, reported tick351.
    const auto rootClock = readClock(clockRootId);
    const auto finiteClock = readClock(finiteId);
    const auto infiniteClock = readClock(infiniteId);
    const auto inactiveClock = readClock(inactiveId);
    requireRepeat(rootClock.parentTimelineLayerId == -1 && rootClock.rootReportedTicks == 351 &&
        rootClock.rootEvaluatedTicks == 350 && rootClock.localTicks == 350 &&
        (rootClock.flags & IMM_AUTHORING_TIMELINE_ROOT), "root reported/evaluated clock domains");
    requireRepeat(finiteClock.parentTimelineLayerId == clockRootId && finiteClock.elapsedTicks == 270 &&
        finiteClock.localTicks == 100 && finiteClock.cycleIndex == 2 && finiteClock.canonicalRepeat == 2 &&
        finiteClock.effectiveRepeat == 2 && (finiteClock.flags & IMM_AUTHORING_TIMELINE_CLAMPED) &&
        (finiteClock.flags & IMM_AUTHORING_TIMELINE_HELD), "finite nested span offset and clamp");
    requireRepeat(infiniteClock.elapsedTicks == 250 && infiniteClock.localTicks == 50 &&
        infiniteClock.cycleIndex == 2 && !(infiniteClock.flags & IMM_AUTHORING_TIMELINE_CLAMPED),
        "infinite nested clock wraps at native duration");
    requireRepeat((inactiveClock.flags & IMM_AUTHORING_TIMELINE_INACTIVE) &&
        !(inactiveClock.flags & IMM_AUTHORING_TIMELINE_ACTIVE_SPAN) && inactiveClock.elapsedTicks == 0 &&
        inactiveClock.localTicks == 0 && inactiveClock.cycleIndex == 0, "inactive timeline does not invent a clock");
    const auto unchangedClock = readClock(clockRootId);
    requireRepeat(std::memcmp(&rootClock, &unchangedClock, sizeof(rootClock)) == 0,
        "clock queries preserve playback and all revision fields");
    player.SetTime(clockDoc, piTick(1101), piTick(1));
    const auto wrappedRoot = readClock(clockRootId);
    requireRepeat(wrappedRoot.elapsedTicks == 1100 && wrappedRoot.localTicks == 100 && wrappedRoot.cycleIndex == 1,
        "infinite root uses native wrapping rather than reported-time arithmetic");
    player.SetTime(clockDoc, piTick(700), piTick(0)); // Exactly Stop700 evaluates the authored hold at699.
    const auto actionClock = readClock(clockRootId);
    requireRepeat(actionClock.rootReportedTicks == 700 && actionClock.rootEvaluatedTicks == 699 &&
        (actionClock.flags & IMM_AUTHORING_TIMELINE_ACTION_HELD) && (actionClock.flags & IMM_AUTHORING_TIMELINE_HELD),
        "authored Stop clock keeps native one-tick hold domain");
    untouchedClock = originalClockBytes;
    requireRepeat(ImmAuthoring_TimelineGetClock(clockDoc, finiteId, &untouchedClock, sizeof(untouchedClock) - 1) ==
        IMM_AUTHORING_INVALID_ARGUMENT && ImmAuthoring_TimelineGetClock(-1, finiteId, &untouchedClock, sizeof(untouchedClock)) ==
        IMM_AUTHORING_NOT_FOUND && std::memcmp(&untouchedClock, &originalClockBytes, sizeof(untouchedClock)) == 0,
        "invalid or undersized clock requests preserve caller memory");
    requireRepeat(ImmAuthoring_TimelineGetClock(doc, sibling, &untouchedClock, sizeof(untouchedClock)) ==
        IMM_AUTHORING_UNSUPPORTED && std::memcmp(&untouchedClock, &originalClockBytes, sizeof(untouchedClock)) == 0,
        "paint layers cannot masquerade as timeline clocks");
    std::puts("[IMM_TIMELINE_CLOCK_CABI_TEST] pass=1 rootAndNested=1 finiteClamp=1 infiniteWrap=1 inactive=1 heldAndActions=1 noMutation=1 capacityPreserved=1");
    requireRepeat(ImmAuthoring_Attach(clockDoc) == IMM_AUTHORING_OK, "attach preview fixture");
    const auto mainBeforePreview = readClock(clockRootId);
    const auto finiteBeforePreview = readClock(finiteId);
    Player::LayerDiagnostics previewChildBefore{};
    requireRepeat(player.GetLayerDiagnostics(clockDoc, previewChildId, previewChildBefore) &&
        previewChildBefore.opacity == 0.75f && previewChildBefore.transform.mTranslation.x == 9,
        "held main child starts at finite-local tick100");
    requireRepeat(ImmAuthoring_TimelinePreviewControl(clockDoc, finiteId, 0, 0) == IMM_AUTHORING_OK &&
        ImmAuthoring_TimelinePreviewControl(clockDoc, finiteId, 1, 150) == IMM_AUTHORING_OK,
        "begin and seek independent preview");
    const auto previewClock = readClock(finiteId);
    SpawnAreaSnapshot unavailableMainPose{};
    requireRepeat(player.GetSpawnAreaSnapshot(clockDoc, unavailableMainPose, 0, nullptr) == -4,
        "main viewpoint snapshot cannot mix preview evaluated state");
    requireRepeat((previewClock.flags & IMM_AUTHORING_TIMELINE_PREVIEW) &&
        (previewClock.flags & IMM_AUTHORING_TIMELINE_HELD) && previewClock.elapsedTicks == 150 &&
        previewClock.localTicks == 50 && previewClock.cycleIndex == 1 &&
        readClock(clockRootId).elapsedTicks == mainBeforePreview.elapsedTicks,
        "preview has its own clock and excludes parent-domain Offset/main Stop");
    requireRepeat(ImmAuthoring_TimelinePreviewControl(clockDoc, infiniteId, 0, 0) == IMM_AUTHORING_QUEUE_FULL,
        "another preview cannot replace the active session");
    ImmAuthoringKey blockedKey{};
    blockedKey.structSize = sizeof(blockedKey); blockedKey.structVersion = 1;
    blockedKey.property = IMM_AUTHORING_ANIM_PROPERTY_OPACITY; blockedKey.floatValue = 0.5f;
    blockedKey.scale = 1; blockedKey.qw = 1;
    requireRepeat(ImmAuthoring_KeySet(clockDoc, finiteId, &blockedKey) == IMM_AUTHORING_OK,
        "preview boundary edit may stage for authoritative commit validation");
    uint64_t blockedRevision = 99;
    requireRepeat(ImmAuthoring_Commit(clockDoc, &blockedRevision) == IMM_AUTHORING_UNSUPPORTED && blockedRevision == 99 &&
        ImmAuthoring_DiscardPending(clockDoc) == IMM_AUTHORING_OK, "preview rejects boundary publication without losing ownership");
    const auto requirePreviewPose = [&](float opacity, double x) {
        Player::LayerDiagnostics diagnostics{};
        requireRepeat(player.GetLayerDiagnostics(clockDoc, previewChildId, diagnostics) &&
            diagnostics.opacity == opacity && diagnostics.transform.mTranslation.x == x &&
            (readClock(finiteId).flags & IMM_AUTHORING_TIMELINE_PREVIEW) && readClock(finiteId).localTicks == 50 &&
            readClock(clockRootId).rootEvaluatedTicks == mainBeforePreview.rootEvaluatedTicks,
            "paused descendant edit uses preview-local time and preserves held main clock");
    };
    const auto presentPreviewEdit = [&]() {
        uint64_t revision = 0;
        requireRepeat(ImmAuthoring_Commit(clockDoc, &revision) == IMM_AUTHORING_OK && revision != 0,
            "seal allowed preview descendant edit");
        requireRepeat(wait([&]() {
            ImmAuthoringCommitStatus status{};
            status.structSize = sizeof(status); status.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
            requireRepeat(ImmAuthoring_GetCommitStatus(clockDoc, revision, &status) == IMM_AUTHORING_OK &&
                status.state != IMM_AUTHORING_COMMIT_REJECTED, "preview descendant commit status");
            return status.state == IMM_AUTHORING_COMMIT_PRESENTED;
        }), "preview descendant edit presents without exiting session");
    };
    ImmAuthoringKey localOpacity{};
    localOpacity.structSize = sizeof(localOpacity); localOpacity.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    localOpacity.property = IMM_AUTHORING_ANIM_PROPERTY_OPACITY;
    localOpacity.timeSeconds = 40.0 / 12600.0; localOpacity.floatValue = 0.25f;
    ImmAuthoringKey localTransform{};
    localTransform.structSize = sizeof(localTransform); localTransform.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
    localTransform.property = IMM_AUTHORING_ANIM_PROPERTY_TRANSFORM;
    localTransform.timeSeconds = localOpacity.timeSeconds;
    localTransform.qw = 1; localTransform.scale = 1; localTransform.tx = 3;
    requirePreviewPose(1.0f, 0);
    requireRepeat(ImmAuthoring_KeySet(clockDoc, previewChildId, &localOpacity) == IMM_AUTHORING_OK,
        "insert paused preview opacity key");
    presentPreviewEdit(); requirePreviewPose(0.25f, 0);
    requireRepeat(ImmAuthoring_KeySet(clockDoc, previewChildId, &localTransform) == IMM_AUTHORING_OK,
        "insert paused preview transform key");
    presentPreviewEdit(); requirePreviewPose(0.25f, 3);
    uint64_t paintA = 0, paintB = 0, frameHandle = 0;
    requireRepeat(ImmAuthoring_DrawingGetHandle(clockDoc, previewPaintId, 0, &paintA) == IMM_AUTHORING_OK &&
        ImmAuthoring_DrawingGetHandle(clockDoc, previewPaintId, 1, &paintB) == IMM_AUTHORING_OK && paintA != paintB,
        "scoped frame fixture stable drawing handles");
    requireRepeat(ImmAuthoring_FrameSetHandle(clockDoc, previewPaintId, 0, paintB) == IMM_AUTHORING_OK,
        "queue scoped paint frame mapping");
    presentPreviewEdit();
    requireRepeat(ImmAuthoring_FrameGetDrawingHandle(clockDoc, previewPaintId, 0, &frameHandle) == IMM_AUTHORING_OK &&
        frameHandle == paintB && (readClock(finiteId).flags & IMM_AUTHORING_TIMELINE_PREVIEW), "scoped frame mapping presents in preview");
    const uint64_t editedTable[] = {paintB, paintA, paintB};
    requireRepeat(ImmAuthoring_FrameSequenceSetHandles(clockDoc, previewPaintId, editedTable, 3) == IMM_AUTHORING_OK,
        "queue scoped whole frame table");
    presentPreviewEdit(); requirePreviewPose(0.25f, 3);
    for (int property : {IMM_AUTHORING_ANIM_PROPERTY_OPACITY, IMM_AUTHORING_ANIM_PROPERTY_TRANSFORM})
    {
        requireRepeat(ImmAuthoring_KeyRemove(clockDoc, previewChildId, property, 40.0 / 12600.0) == IMM_AUTHORING_OK,
            "undo inserted preview key by removing original missing-track state");
        presentPreviewEdit();
    }
    requirePreviewPose(1.0f, 0);
    requireRepeat(ImmAuthoring_KeySet(clockDoc, previewChildId, &localOpacity) == IMM_AUTHORING_OK, "redo preview opacity key");
    presentPreviewEdit();
    requireRepeat(ImmAuthoring_KeySet(clockDoc, previewChildId, &localTransform) == IMM_AUTHORING_OK, "redo preview transform key");
    presentPreviewEdit(); requirePreviewPose(0.25f, 3);
    // Change a later key too: main-local100 must use these new values on exit,
    // while preview-local50 retains the inserted values. A blind capsule restore
    // would incorrectly expose the old held-main opacity0.75/translation9.
    localOpacity.timeSeconds = 90.0 / 12600.0; localOpacity.floatValue = 0.5f;
    localTransform.timeSeconds = localOpacity.timeSeconds; localTransform.tx = 5;
    requireRepeat(ImmAuthoring_KeySet(clockDoc, previewChildId, &localOpacity) == IMM_AUTHORING_OK,
        "replace later opacity key while preview remains paused");
    presentPreviewEdit();
    requireRepeat(ImmAuthoring_KeySet(clockDoc, previewChildId, &localTransform) == IMM_AUTHORING_OK,
        "replace later transform key while preview remains paused");
    presentPreviewEdit(); requirePreviewPose(0.25f, 3);
    requireRepeat(ImmAuthoring_TimelinePreviewControl(clockDoc, finiteId, 2, 0) == IMM_AUTHORING_OK &&
        wait([&]() { return readClock(finiteId).elapsedTicks > 150; }), "independent preview advances while main root is held");
    requireRepeat(ImmAuthoring_TimelinePreviewControl(clockDoc, finiteId, 3, 0) == IMM_AUTHORING_OK,
        "pause preview");
    const auto pausedPreview = readClock(finiteId);
    tick(); tick();
    requireRepeat(readClock(finiteId).elapsedTicks == pausedPreview.elapsedTicks &&
        readClock(clockRootId).rootEvaluatedTicks == mainBeforePreview.rootEvaluatedTicks,
        "paused preview and main clock remain fixed");
    requireRepeat(ImmAuthoring_TimelinePreviewControl(clockDoc, finiteId, 4, 0) == IMM_AUTHORING_OK,
        "exit preview");
    const auto mainAfterPreview = readClock(clockRootId);
    const auto finiteAfterPreview = readClock(finiteId);
    requireRepeat(!(finiteAfterPreview.flags & IMM_AUTHORING_TIMELINE_PREVIEW) &&
        mainAfterPreview.rootReportedTicks == mainBeforePreview.rootReportedTicks &&
        mainAfterPreview.rootEvaluatedTicks == mainBeforePreview.rootEvaluatedTicks &&
        finiteAfterPreview.elapsedTicks == finiteBeforePreview.elapsedTicks &&
        finiteAfterPreview.flags == finiteBeforePreview.flags,
        "preview exit restores held main and nested clocks/intent exactly");
    Player::LayerDiagnostics previewChildAfter{};
    uint32_t retainedKeys = 0;
    requireRepeat(player.GetLayerDiagnostics(clockDoc, previewChildId, previewChildAfter) &&
        previewChildAfter.opacity == 0.5f && previewChildAfter.transform.mTranslation.x == 5 &&
        ImmAuthoring_KeyGetCount(clockDoc, previewChildId, IMM_AUTHORING_ANIM_PROPERTY_OPACITY, &retainedKeys) ==
        IMM_AUTHORING_OK && retainedKeys == 3 &&
        ImmAuthoring_KeyGetCount(clockDoc, previewChildId, IMM_AUTHORING_ANIM_PROPERTY_TRANSFORM, &retainedKeys) ==
        IMM_AUTHORING_OK && retainedKeys == 3,
        "preview exit retains edited canonical tracks and reevaluates them at held main time");
    std::puts("[IMM_SCOPED_PREVIEW_EDIT_CABI_TEST] pass=1 pausedLocalKeys=1 presentedInPreview=1 undoRemove=1 redo=1 canonicalTracksRetained=1 heldMainReevaluated=1");
    for (int frameIndex = 0; frameIndex < 3; ++frameIndex)
        requireRepeat(ImmAuthoring_FrameGetDrawingHandle(clockDoc, previewPaintId, frameIndex, &frameHandle) == IMM_AUTHORING_OK &&
            frameHandle == editedTable[frameIndex], "scoped edited frame table survives preview exit");
    requireRepeat(ImmAuthoring_DrawingGetHandle(clockDoc, previewPaintId, 0, &frameHandle) == IMM_AUTHORING_OK &&
        frameHandle == paintA && ImmAuthoring_DrawingGetHandle(clockDoc, previewPaintId, 1, &frameHandle) == IMM_AUTHORING_OK &&
        frameHandle == paintB, "scoped frame editing retains drawing identities");
    for (bool reject : {false, true})
    {
        requireRepeat(ImmAuthoring_TimelinePreviewControl(clockDoc, finiteId, 0, 0) == IMM_AUTHORING_OK &&
            ImmAuthoring_TimelinePreviewControl(clockDoc, finiteId, 1, 150) == IMM_AUTHORING_OK,
            "begin pending-exit preview");
        ImmAuthoringKey pendingKey{};
        pendingKey.structSize = sizeof(pendingKey); pendingKey.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
        pendingKey.property = IMM_AUTHORING_ANIM_PROPERTY_LOOP; pendingKey.boolValue = reject ? 0 : 1;
        _putenv_s("IMM_LIVE_EDIT_HOLD_PAINT_PLAYBACK_KEYS", "1");
        requireRepeat(ImmAuthoring_KeySet(clockDoc, previewPaintId, &pendingKey) == IMM_AUTHORING_OK,
            "queue held scoped playback key");
        uint64_t revision = 0;
        requireRepeat(ImmAuthoring_Commit(clockDoc, &revision) == IMM_AUTHORING_OK && revision != 0,
            "commit held scoped playback key");
        ImmAuthoringCommitStatus status{};
        requireRepeat(wait([&]() {
            status.structSize = sizeof(status); status.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
            requireRepeat(ImmAuthoring_GetCommitStatus(clockDoc, revision, &status) == IMM_AUTHORING_OK, "held scoped key status");
            return status.state == IMM_AUTHORING_COMMIT_PREPARED;
        }), "scoped key waits before publication");
        requireRepeat(ImmAuthoring_TimelinePreviewControl(clockDoc, finiteId, 4, 0) == IMM_AUTHORING_QUEUE_FULL &&
            (readClock(finiteId).flags & IMM_AUTHORING_TIMELINE_PREVIEW), "pending preview exit reports busy and retains context");
        if (reject) _putenv_s("IMM_LIVE_EDIT_FAIL_PRESENT", "1");
        _putenv_s("IMM_LIVE_EDIT_HOLD_PAINT_PLAYBACK_KEYS", "");
        requireRepeat(wait([&]() {
            status.structSize = sizeof(status); status.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
            requireRepeat(ImmAuthoring_GetCommitStatus(clockDoc, revision, &status) == IMM_AUTHORING_OK, "draining scoped key status");
            return status.state == IMM_AUTHORING_COMMIT_PRESENTED || status.state == IMM_AUTHORING_COMMIT_REJECTED;
        }), "pending scoped key reaches terminal state");
        _putenv_s("IMM_LIVE_EDIT_FAIL_PRESENT", "");
        requireRepeat(status.state == (reject ? IMM_AUTHORING_COMMIT_REJECTED : IMM_AUTHORING_COMMIT_PRESENTED) &&
            status.result == (reject ? -10 : 0), "pending preview key preserves success/rejection semantics");
        requireRepeat(wait([&]() { return !(readClock(finiteId).flags & IMM_AUTHORING_TIMELINE_PREVIEW); }),
            "pending exit drains automatically after terminal key publication");
        ImmAuthoringKey retainedLoop{};
        retainedLoop.structSize = sizeof(retainedLoop); retainedLoop.structVersion = IMM_AUTHORING_STRUCT_VERSION_1;
        requireRepeat(ImmAuthoring_KeyGet(clockDoc, previewPaintId, IMM_AUTHORING_ANIM_PROPERTY_LOOP, 0, &retainedLoop) ==
            IMM_AUTHORING_OK && retainedLoop.boolValue == 1 &&
            readClock(clockRootId).rootEvaluatedTicks == mainBeforePreview.rootEvaluatedTicks,
            "successful key survives exit and rejected replacement preserves it with held main clock");
    }
    std::puts("[IMM_PREVIEW_PENDING_EXIT_CABI_TEST] pass=1 busyWhilePrepared=1 presentedDrain=1 rejectedDrain=1 scopedFrameTableRetained=1 stableDrawings=1");
    requireRepeat(ImmAuthoring_TimelinePreviewControl(clockDoc, finiteId, 0, 0) == IMM_AUTHORING_OK &&
        player.SetLayerOpacity(clockDoc, finiteId, 0.42f) &&
        !(readClock(finiteId).flags & IMM_AUTHORING_TIMELINE_PREVIEW), "legacy override exits preview before applying");
    Player::LayerDiagnostics overrideDiagnostics{};
    requireRepeat(player.GetLayerDiagnostics(clockDoc, finiteId, overrideDiagnostics) &&
        overrideDiagnostics.opacityOverrideEnabled && overrideDiagnostics.opacityOverrideValue == 0.42f,
        "successful legacy override survives preview exit");
    requireRepeat(ImmAuthoring_TimelinePreviewControl(clockDoc, inactiveId, 0, 0) == IMM_AUTHORING_OK &&
        (readClock(inactiveId).flags & IMM_AUTHORING_TIMELINE_ACTIVE_SPAN), "inactive main occurrence can preview independently");
    requireRepeat(ImmAuthoring_Detach(clockDoc) == IMM_AUTHORING_OK &&
        !(readClock(inactiveId).flags & IMM_AUTHORING_TIMELINE_PREVIEW), "detach ends preview safely");
    std::puts("[IMM_TIMELINE_PREVIEW_CABI_TEST] pass=1 independentClock=1 pausedMain=1 heldRestore=1 boundaryPublicationBlocked=1 scopedEdits=1 inactiveScope=1 detach=1 mutedAudioOnly=1");
    player.UnloadAllSync();
    gImmUnityPlugin.mBridge.Shutdown();
    context->Release();
    device->Release();
    std::puts("[IMM_REPEAT_CABI_TEST] pass=1 linkedProductionExports=1 loadedDocument=1 warp=1 zeroFiniteMaximum=1 queuedState=1 siblingStable=1 discard=1 detachedRejection=1");
    return 0;
}
