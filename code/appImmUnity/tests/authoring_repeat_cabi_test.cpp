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
