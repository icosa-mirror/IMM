#pragma once

#include "libImmCore/src/libBasics/piTArray.h"
#include "libImmCore/src/libBasics/piTypes.h"
#include "../document//sequence.h"
#include "strokeCollector.h"

namespace ImmImporter
{

    bool IsLoadingAsync();

    bool IsStoppedLoading();
    void StopLoadingAsync();

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
