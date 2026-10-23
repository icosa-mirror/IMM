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
    const int doc = player.Load(argv[1]);
    requireRepeat(doc >= 0, "load fixture");
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
        requireRepeat(ImmAuthoring_Commit(doc, &revision) == IMM_AUTHORING_OK && revision != 0,
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
    }
    requireRepeat(ImmAuthoring_KeyRemove(doc, target, IMM_AUTHORING_ANIM_PROPERTY_TRANSFORM, 0.0) == IMM_AUTHORING_OK,
        "remove held transform key");
    presentHierarchyEdit();
    requireRepeat(player.GetLayerDiagnostics(doc, target, heldTransform) &&
        heldTransform.transform.mTranslation.x == heldTransform.canonicalTransform.mTranslation.x,
        "last transform key removal restores canonical held pose");
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
    player.UnloadAllSync();
    gImmUnityPlugin.mBridge.Shutdown();
    context->Release();
    device->Release();
    std::puts("[IMM_REPEAT_CABI_TEST] pass=1 linkedProductionExports=1 loadedDocument=1 warp=1 zeroFiniteMaximum=1 queuedState=1 siblingStable=1 discard=1 detachedRejection=1");
    return 0;
}
