#pragma once

#include "libImmCore/src/libBasics/piTArray.h"
#include "libImmCore/src/libBasics/piTypes.h"
#include "../document//sequence.h"
#include "strokeCollector.h"

namespace ImmImporter
{

    // One token per active importer/asset worker. The owner must retain the
    // sequence until every token has been released, including failed imports.
    class ImportActivity
    {
    public:
        explicit ImportActivity(Sequence * sequence) : mSequence(sequence)
        { mSequence->GetImportState().activeWorkers.fetch_add(1); }
        ~ImportActivity() { mSequence->GetImportState().activeWorkers.fetch_sub(1); }
        ImportActivity(const ImportActivity &) = delete;
        ImportActivity & operator=(const ImportActivity &) = delete;
    private:
        Sequence * mSequence;
    };

    bool IsLoadingAsync(const Sequence * sequence);

    bool IsStoppedLoading(const Sequence * sequence);
    void StopLoadingAsync(Sequence * sequence);

    bool ImportFromDisk(Sequence* sq, ImmCore::piLog* log, const wchar_t* filename, const Drawing::ColorSpace colorSpace, Drawing::PaintRenderingTechnique renderingTechnique, IStrokeCollector* collector);
    bool ImportFromDisk(Sequence* sq, ImmCore::piLog* log, const wchar_t* filename, const Drawing::ColorSpace colorSpace, Drawing::PaintRenderingTechnique renderingTechnique);
    bool ImportFromMemory(ImmCore::piTArray<uint8_t>* data, Sequence* sq, ImmCore::piLog* log, const Drawing::ColorSpace colorSpace, Drawing::PaintRenderingTechnique renderingTechnique, IStrokeCollector* collector);
    bool ImportFromMemory(ImmCore::piTArray<uint8_t>* data, Sequence* sq, ImmCore::piLog* log, const Drawing::ColorSpace colorSpace, Drawing::PaintRenderingTechnique renderingTechnique);

    // Browser/native-parity staged loading. Metadata initializes layer assets
    // and drawing offsets without decoding drawing payloads or non-paint assets.
    bool ImportMetadataFromMemory(
        ImmCore::piTArray<uint8_t>* data,
        Sequence* sq,
        ImmCore::piLog* log,
        const Drawing::ColorSpace colorSpace,
        Drawing::PaintRenderingTechnique renderingTechnique,
        IStrokeCollector* collector);
    bool DecodeDrawingFromMemory(
        ImmCore::piTArray<uint8_t>* data,
        Sequence* sq,
        ImmCore::piLog* log,
        uint32_t layerId,
        uint32_t drawingId,
        const Drawing::ColorSpace colorSpace,
        Drawing::PaintRenderingTechnique renderingTechnique,
        IStrokeCollector* collector);
    bool DecodeLayerAssetFromMemory(
        ImmCore::piTArray<uint8_t>* data,
        Sequence* sq,
        ImmCore::piLog* log,
        uint32_t layerId,
        const Drawing::ColorSpace colorSpace,
        Drawing::PaintRenderingTechnique renderingTechnique,
        IStrokeCollector* collector);

    bool ImportSceneGraphOnly(Sequence* sq, ImmCore::piLog* log, const wchar_t* filename);
}
