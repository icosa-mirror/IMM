#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>
#include "appImmShared/src/imm_engine_bridge.h"
#include "libImmCore/src/libBasics/piFile.h"
using namespace ImmCore;
using namespace ImmShared;
static void require(bool value, const char * message)
{ if (!value) { std::fprintf(stderr, "[IMM_NATIVE_LOG_TEST] %s\n", message); std::exit(1); } }
int wmain(int argc, wchar_t ** argv)
{
    require(argc == 3, "expected explicit path and invalid-parent path");
    ImmEngineBridge bridge;
    ImmEngineBridge::InitConfig config;
    config.rendererApi = piRenderer::API::DX;
    config.enableSound = false;
    config.initializeRendererOnInit = false;
    config.captureWarnings = true;
    require(bridge.Init(config), "default bridge initialization");
    require(!piFile::Exists(L"imm_player_log.txt") && !piFile::Exists(L"imm-render-graph.log"), "no implicit file logs");
    bridge.GetLog()->Printf(LT_MESSAGE, L"verbose info must not enter Unity warning queue");
    std::thread worker([&]() { bridge.GetLog()->Printf(LT_WARNING, L"unicode \u8b66\u544a 100%% complete"); });
    worker.join();
    char message[32769]; int severity = 0;
    int needed = bridge.ReadDiagnostic(nullptr, 0, &severity);
    require(needed < 0, "short-buffer query preserves diagnostic");
    const int size = bridge.ReadDiagnostic(message, sizeof(message), &severity);
    require(size == -needed && severity == 2 && std::string(message).find("100% complete") != std::string::npos,
        "worker warning forwarded as literal UTF-8");
    require(std::string(message).find("\xe8\xad\xa6\xe5\x91\x8a") != std::string::npos, "Unicode warning preserved");
    require(bridge.ReadDiagnostic(message, sizeof(message), &severity) == 0, "info excluded");
    bridge.Shutdown();
    std::string explicitPath = EncodeUtf8(argv[1]);
    config.logFileName = explicitPath.c_str();
    require(bridge.Init(config), "explicit diagnostic file initialization");
    bridge.GetLog()->Printf(LT_MESSAGE, L"verbose diagnostic");
    bridge.Shutdown();
    require(piFile::Exists(argv[1]), "explicit file created");
    std::string invalidPath = EncodeUtf8(argv[2]);
    config.logFileName = invalidPath.c_str();
    require(bridge.Init(config), "unwritable diagnostic path must not block initialization");
    require(bridge.ReadDiagnostic(message, sizeof(message), &severity) > 0 && severity == 2 &&
        std::string(message).find("continuing without it") != std::string::npos, "file failure warning forwarded");
    bridge.Shutdown();
    piDiagnosticQueue queue;
    for (int i = 0; i < 70; ++i) piDiagnosticQueue::Capture(&queue, 1, L"bounded error");
    require(queue.Read(message, sizeof(message), &severity) > 0 && severity == 2 &&
        std::string(message).find("6 messages dropped") != std::string::npos, "bounded overflow notification");
    int count = 0;
    while (queue.Read(message, sizeof(message), &severity) > 0) ++count;
    require(count == 64, "queue bounded to 64 messages");
    std::puts("[IMM_NATIVE_LOG_TEST] default-off files, worker diagnostics, explicit file, nonfatal file failure, bounded queue passed");
}
