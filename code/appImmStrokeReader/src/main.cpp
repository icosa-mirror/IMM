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

// Bump this when you need to confirm Unity loaded the new dylib.
static const char* kImmStrokeReaderBuildIdA = "IMM_STROKE_READER_BUILD_ID=2026-07-19-PHASE5";
static const wchar_t* kImmStrokeReaderBuildIdW = L"IMM_STROKE_READER_BUILD_ID=2026-07-19-PHASE5";

extern "C" UNITY_INTERFACE_EXPORT const char* UNITY_INTERFACE_API StrokeReader_GetBuildId()
{
    return kImmStrokeReaderBuildIdA;
}

// Global state
static struct
{
    piLog mLog;
    bool mInitialized;
    int mNextDocId;
    std::map<int, StrokeStore*> mDocuments;
    std::mutex mMutex;
} gStrokeReader = { {}, false, 1, {}, {} };

static void ExtractChapterStartTimes(const Sequence& seq, std::vector<piTick>* chapterStartTimes)
{
    if (chapterStartTimes == nullptr)
    {
        return;
    }

    chapterStartTimes->clear();
    chapterStartTimes->push_back(piTick(0));

    Layer* root = seq.GetRoot();
    if (root == nullptr)
    {
        return;
    }

    const int numActionKeys = root->GetNumAnimKeys(Layer::AnimProperty::Action);
    int numPlayMarkers = 0;
    int numStopMarkers = 0;
    for (int i = 0; i < numActionKeys; i++)
    {
        const Layer::AnimKey* key = root->GetAnimKey(Layer::AnimProperty::Action, i);
        const Layer::AnimAction action = static_cast<Layer::AnimAction>(key->mValue.mInt);
        if (action == Layer::AnimAction::Play)
        {
            numPlayMarkers++;
        }
        else if (action == Layer::AnimAction::Stop)
        {
            numStopMarkers++;
        }
    }

    for (int i = 0; i < numActionKeys; i++)
    {
        const Layer::AnimKey* key = root->GetAnimKey(Layer::AnimProperty::Action, i);
        const Layer::AnimAction action = static_cast<Layer::AnimAction>(key->mValue.mInt);
        if (numPlayMarkers > 0)
        {
            if (action == Layer::AnimAction::Play)
            {
                chapterStartTimes->push_back(key->mTime);
            }
        }
        else if (numStopMarkers > 0)
        {
            if (action == Layer::AnimAction::Stop)
            {
                chapterStartTimes->push_back(key->mTime + 1);
            }
        }
    }
}

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

    gStrokeReader.mLog.Printf(LT_MESSAGE, L"ImmStrokeReader initialized (%s)", kImmStrokeReaderBuildIdW);
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

    // Emit build ID from an always-called API so we can tell if Unity
    // is running the updated dylib without restarting.
    gStrokeReader.mLog.Printf(LT_MESSAGE, L"%s", kImmStrokeReaderBuildIdW);

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

    store->CaptureSequenceMetadata(seq);

    // Store and return document ID
    std::vector<piTick> chapterStartTimes;
    ExtractChapterStartTimes(seq, &chapterStartTimes);
    store->SetChapterStartTimes(chapterStartTimes);
    seq.Deinit(&gStrokeReader.mLog);

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

    store->CaptureSequenceMetadata(seq);

    // Store and return document ID
    std::vector<piTick> chapterStartTimes;
    ExtractChapterStartTimes(seq, &chapterStartTimes);
    store->SetChapterStartTimes(chapterStartTimes);
    seq.Deinit(&gStrokeReader.mLog);

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

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetDocumentInfo(int docId, StrokeDocumentInfoC* info)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized || info == nullptr)
        return false;

    auto it = gStrokeReader.mDocuments.find(docId);
    return it != gStrokeReader.mDocuments.end() && it->second->GetDocumentInfo(info);
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

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetLayerTransform(int docId, int layerIdx, StrokeLayerTransformC* localTransform, StrokeLayerTransformC* worldTransform)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return false;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return false;

    return it->second->GetLayerTransform(layerIdx, localTransform, worldTransform);
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetLayerAnimationKeyCount(int docId, int layerIdx)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return 0;

    auto it = gStrokeReader.mDocuments.find(docId);
    return it == gStrokeReader.mDocuments.end() ? 0 : it->second->GetLayerAnimationKeyCount(layerIdx);
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetLayerAnimationKey(
    int docId,
    int layerIdx,
    int keyIdx,
    StrokeAnimationKeyC* key)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized || key == nullptr)
        return false;

    auto it = gStrokeReader.mDocuments.find(docId);
    return it != gStrokeReader.mDocuments.end() && it->second->GetLayerAnimationKey(layerIdx, keyIdx, key);
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

extern "C" float UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetDrawingBiggestStroke(int docId, int layerIdx, int drawingIdx)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return 0.0f;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return 0.0f;

    float biggestStroke = 0.0f;
    if (!it->second->GetDrawingBiggestStroke(layerIdx, drawingIdx, &biggestStroke))
        return 0.0f;

    return biggestStroke;
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

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetChapterCountFromFile(char* fileName)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return 0;

    if (fileName == nullptr || fileName[0] == '\0')
        return 0;

    Sequence seq;
    if (!ImportSceneGraphOnly(&seq, &gStrokeReader.mLog, pistr2ws(fileName)))
    {
        return 0;
    }

    std::vector<piTick> chapterStartTimes;
    ExtractChapterStartTimes(seq, &chapterStartTimes);
    int count = static_cast<int>(chapterStartTimes.size());

    seq.Deinit(&gStrokeReader.mLog);
    return count;
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetChapterCount(int docId)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return 0;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return 0;

    return it->second->GetChapterCount();
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetCurrentChapter(int docId)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return 0;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return 0;

    return it->second->GetCurrentChapter();
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_SetChapter(int docId, int chapterIndex)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return false;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return false;

    return it->second->SetCurrentChapter(chapterIndex);
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetDrawingIndexForChapter(int docId, int layerIdx, int chapterIndex)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return 0;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return 0;

    return it->second->GetDrawingIndexForChapter(layerIdx, chapterIndex);
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetLayerAnimationInfo(int docId, int layerIdx, int* frameRate, int* numFrames, int* maxRepeatCount)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return false;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return false;

    uint32_t fr = 0, nf = 0, mrc = 0;
    bool ok = it->second->GetLayerAnimationInfo(layerIdx, &fr, &nf, &mrc);
    if (ok)
    {
        if (frameRate) *frameRate = static_cast<int>(fr);
        if (numFrames) *numFrames = static_cast<int>(nf);
        if (maxRepeatCount) *maxRepeatCount = static_cast<int>(mrc);
    }
    return ok;
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetFrameBuffer(int docId, int layerIdx, int* frames, int maxFrames)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized)
        return 0;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return 0;

    if (!frames || maxFrames <= 0)
        return 0;

    uint32_t* uframes = new uint32_t[maxFrames];
    bool ok = it->second->GetFrameBuffer(layerIdx, uframes, maxFrames);
    if (!ok)
    {
        delete[] uframes;
        return 0;
    }

    // Get actual frame count
    int count = 0;
    uint32_t temp = 0;
    if (it->second->GetLayerAnimationInfo(layerIdx, nullptr, &temp, nullptr))
    {
        count = std::min(maxFrames, static_cast<int>(temp));
    }

    for (int i = 0; i < count; i++)
    {
        frames[i] = static_cast<int>(uframes[i]);
    }

    delete[] uframes;
    return count;
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

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetPictureInfo(
    int docId, int layerIdx, StrokePictureInfoC* info)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized || !info)
        return false;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return false;

    return it->second->GetPictureInfo(layerIdx, info);
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StrokeReader_GetPicturePixelData(
    int docId, int layerIdx, uint8_t* pixels, int maxBytes)
{
    std::lock_guard<std::mutex> lock(gStrokeReader.mMutex);

    if (!gStrokeReader.mInitialized || !pixels || maxBytes <= 0)
        return 0;

    auto it = gStrokeReader.mDocuments.find(docId);
    if (it == gStrokeReader.mDocuments.end())
        return 0;

    StrokePictureInfoC info;
    if (!it->second->GetPictureInfo(layerIdx, &info))
        return 0;

    if (!it->second->GetPicturePixels(layerIdx, pixels, maxBytes))
        return 0;

    return info.dataSize;
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
