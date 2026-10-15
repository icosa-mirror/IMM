// Headless test bridge: actual managed LoadFromFile declaration -> shared UTF-8
// decoder -> real ImportFromDisk. No graphics/player initialization is required.
#include <cstring>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string>
#include "appImmUnity/src/imm_utf8.h"
#include "libImmCore/src/libBasics/piLog.h"
#include "libImmCore/src/libBasics/piFile.h"
#include "libImmCore/src/libBasics/piStr.h"
#include "libImmImporter/src/fromImmersive/fromImmersive.h"

static ImmCore::piLog testLog;
extern "C" __declspec(dllexport) int BeginUnicodePathTestLog(const wchar_t * path)
{
    return testLog.Init(path, PILOG_TXT) ? 1 : 0;
}
extern "C" __declspec(dllexport) void EndUnicodePathTestLog() { testLog.End(); }
extern "C" __declspec(dllexport) int LoadFromFile(const char * utf8)
{
    try
    {
        std::wstring path;
        if (utf8 == nullptr || !ImmUnity::DecodeUtf8(utf8, std::strlen(utf8), path)) return -1;
        testLog.Printf(LT_MESSAGE, L"[IMM_UNICODE_PATH_TEST] filename=\"%ls\"", path.c_str());
        ImmImporter::Sequence sequence;
        if (!ImmImporter::ImportFromDisk(&sequence, &testLog, path.c_str(),
            ImmImporter::Drawing::ColorSpace::Linear,
            ImmImporter::Drawing::PaintRenderingTechnique::Static)) return -1;
        // The normal importer returns after buffering; keep path/model/log alive
        // until its detached asset loader has finished, just as the player does.
        while (ImmImporter::IsLoadingAsync())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const int layers = static_cast<int>(sequence.GetLayerStorageCount());
        sequence.Deinit(&testLog);
        return layers;
    }
    catch (...) { return -1; }
}
extern "C" __declspec(dllexport) int LegacyAnsiPathExists(const char * utf8)
{
    wchar_t * legacy = ImmCore::pistr2ws(utf8);
    const bool exists = legacy != nullptr && ImmCore::piFile::Exists(legacy);
    std::free(legacy);
    return exists ? 1 : 0;
}
extern "C" __declspec(dllexport) int TestStrictUtf8Decoder()
{
    std::wstring wide;
    const char * invalid[] = {"", "\xc0\xaf", "\xe0\x80\x80", "\xed\xa0\x80",
        "\xf4\x90\x80\x80", "\xe4\xb8", "\x80", "\xe4\x41\xad"};
    for (const char * text : invalid)
        if (ImmUnity::DecodeUtf8(text, std::strlen(text), wide)) return 0;
    if (ImmUnity::DecodeUtf8(nullptr, 0, wide)) return 0;
    if (!ImmUnity::DecodeUtf8("\xe9\xbe\x9f\xe5\x85\x94", 6, wide) || wide != L"\u9f9f\u5154") return 0;
    if (!ImmUnity::DecodeUtf8("\xf0\x9f\x90\xa2", 4, wide)) return 0;
    if (sizeof(wchar_t) == 2 && (wide.size() != 2 || wide[0] != 0xd83d || wide[1] != 0xdc22)) return 0;
    if (sizeof(wchar_t) == 4 && (wide.size() != 1 || wide[0] != 0x1f422)) return 0;
    const char embeddedNull[] = {'a', 0, 'b'};
    return ImmUnity::DecodeUtf8(embeddedNull, 3, wide) ? 0 : 1;
}
