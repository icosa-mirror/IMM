// ImmStrokeReader - Unity plugin for reading raw stroke data from IMM files
// This plugin extracts stroke data (positions, colors, widths, etc.) without rendering

#include "strokeStore.h"
#include "libImmCore/src/libBasics/piLog.h"
#include "libImmCore/src/libBasics/piStr.h"
#include "libImmCore/src/libBasics/piTArray.h"
#include "libImmImporter/src/document/sequence.h"
#include "libImmImporter/src/fromImmersive/fromImmersive.h"

#include <map>
#include <mutex>

using namespace ImmCore;
using namespace ImmImporter;
using namespace ImmStrokeReader;

// Unity interface macros
#if defined(__CYGWIN32__)
    #define UNITY_INTERFACE_API __stdcall
    #define UNITY_INTERFACE_EXPORT __declspec(dllexport)
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(WINAPI_FAMILY)
    #define UNITY_INTERFACE_API __stdcall
    #define UNITY_INTERFACE_EXPORT __declspec(dllexport)
#elif defined(__MACH__) || defined(__ANDROID__) || defined(__linux__)
    #define UNITY_INTERFACE_API
    #define UNITY_INTERFACE_EXPORT __attribute__ ((visibility ("default")))
#else
    #define UNITY_INTERFACE_API
    #define UNITY_INTERFACE_EXPORT
#endif

// Global state
static struct
{
    piLog mLog;
    bool mInitialized;
    int mNextDocId;
    std::map<int, StrokeStore*> mDocuments;
    std::mutex mMutex;
} gStrokeReader = { {}, false, 1, {}, {} };

// ============================================================================
// Lifecycle API
// ============================================================================

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_Init(char* logFileName)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (gStrokeReader.mInitialized)
        return 0; // Already initialized

    const wchar_t* wstrLogFileName = (logFileName == nullptr) ? L"imm_stroke_reader_log.txt" : pistr2ws(logFileName);

#ifdef _DEBUG
    if (!gStrokeReader.mLog.Init(wstrLogFileName, PILOG_TXT + PILOG_CNS))
#else
    if (!gStrokeReader.mLog.Init(wstrLogFileName, PILOG_TXT))
#endif
    {
        return -1;
    }

    gStrokeReader.mLog.Printf(LT_MESSAGE, L"ImmStrokeReader initialized");
    gStrokeReader.mInitialized = true;
    gStrokeReader.mNextDocId = 1;

    return 0;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_End()
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return;

    // Clean up all documents
    for (auto& pair : gStrokeReader.mDocuments)
    {
        delete pair.second;
    }
    gStrokeReader.mDocuments.clear();

    gStrokeReader.mLog.Printf(LT_MESSAGE, L"ImmStrokeReader shutdown");
    gStrokeReader.mLog.End();
    gStrokeReader.mInitialized = false;
}

// ============================================================================
// Loading API
// ============================================================================

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_LoadFromFile(char* fileName)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
    {
        return -1;
    }

    if (fileName == nullptr || fileName[0] == '\0')
    {
        gStrokeReader.mLog.Printf(LT_ERROR, L"StrokeReader_LoadFromFile: filename is null or empty");
        return -2;
    }

    gStrokeReader.mLog.Printf(LT_MESSAGE, L"StrokeReader_LoadFromFile: %s", pistr2ws(fileName));

    // Create a stroke store to collect the data
    StrokeStore* store = new StrokeStore();

    // Create a sequence to parse the file structure
    Sequence seq;

    // Import with the collector
    bool result = ImportFromDisk(
        &seq,
        &gStrokeReader.mLog,
        pistr2ws(fileName),
        Drawing::ColorSpace::Gamma,
        Drawing::PaintRenderingTechnique::Static,
        store
    );

    if (!result)
    {
        gStrokeReader.mLog.Printf(LT_ERROR, L"StrokeReader_LoadFromFile: Import failed for %s", pistr2ws(fileName));
        delete store;
        return -3;
    }

    // Store and return document ID
    int docId = gStrokeReader.mNextDocId++;
    gStrokeReader.mDocuments[docId] = store;

    gStrokeReader.mLog.Printf(LT_MESSAGE, L"StrokeReader_LoadFromFile: Loaded %d layers, docId=%d",
        store->GetLayerCount(), docId);

    return docId;
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_LoadFromMemory(void* data, int size)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
    {
        return -1;
    }

    if (data == nullptr || size <= 0)
    {
        gStrokeReader.mLog.Printf(LT_ERROR, L"StrokeReader_LoadFromMemory: data is null or size is invalid");
        return -2;
    }

    gStrokeReader.mLog.Printf(LT_MESSAGE, L"StrokeReader_LoadFromMemory: size=%d", size);

    // Create a stroke store to collect the data
    StrokeStore* store = new StrokeStore();

    // Create data array for import
    piTArray<uint8_t> immData;
    immData.Init(0, false);
    immData.Set(static_cast<uint8_t*>(data), static_cast<uint64_t>(size));

    // Create a sequence to parse the file structure
    Sequence seq;

    // Import with the collector
    bool result = ImportFromMemory(
        &immData,
        &seq,
        &gStrokeReader.mLog,
        Drawing::ColorSpace::Gamma,
        Drawing::PaintRenderingTechnique::Static,
        store
    );

    if (!result)
    {
        gStrokeReader.mLog.Printf(LT_ERROR, L"StrokeReader_LoadFromMemory: Import failed");
        delete store;
        return -3;
    }

    // Store and return document ID
    int docId = gStrokeReader.mNextDocId++;
    gStrokeReader.mDocuments[docId] = store;

    gStrokeReader.mLog.Printf(LT_MESSAGE, L"StrokeReader_LoadFromMemory: Loaded %d layers, docId=%d",
        store->GetLayerCount(), docId);

    return docId;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_Unload(int docId)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it != gStrokeReader.mDocuments.end())
    {
        delete it->second;
        gStrokeReader.mDocuments.erase(it);
        gStrokeReader.mLog.Printf(LT_MESSAGE, L"StrokeReader_Unload: docId=%d", docId);
    }
}

// ============================================================================
// Query API
// ============================================================================

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetLayerCount(int docId)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return 0;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return 0;

    return it->second->GetLayerCount();
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetLayerInfo(int docId, int layerIdx, StrokeLayerInfoC* info)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized || !info)
        return false;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return false;

    return it->second->GetLayerInfo(layerIdx, info);
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetDrawingCount(int docId, int layerIdx)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return 0;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return 0;

    return it->second->GetDrawingCount(layerIdx);
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetStrokeCount(int docId, int layerIdx, int drawingIdx)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return 0;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return 0;

    return it->second->GetStrokeCount(layerIdx, drawingIdx);
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetStrokeInfo(
    int docId, int layerIdx, int drawingIdx, int strokeIdx,
    StrokeInfoC* info)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized || !info)
        return false;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return false;

    return it->second->GetStrokeInfo(layerIdx, drawingIdx, strokeIdx, info);
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetStrokePoints(
    int docId, int layerIdx, int drawingIdx, int strokeIdx,
    StrokePointC* points, int maxPoints)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized || !points || maxPoints <= 0)
        return false;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return false;

    return it->second->GetStrokePoints(layerIdx, drawingIdx, strokeIdx, points, maxPoints);
}

// ============================================================================
// Utility API
// ============================================================================

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_IsInitialized()
{
    return gStrokeReader.mInitialized;
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetDocumentCount()
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);
    return static_cast<int>(gStrokeReader.mDocuments.size());
}
