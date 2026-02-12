#pragma once

#include <cstdint>
#include "libImmCore/src/libBasics/piVecTypes.h"
#include "../document/layerPaint/element.h"

namespace ImmImporter
{

// Interface for collecting raw stroke data during IMM import.
// Implement this interface to receive stroke data as it is decoded.
class IStrokeCollector
{
public:
    virtual ~IStrokeCollector() = default;

    // Called when a layer begins loading
    // @param layerId - unique ID of the layer
    // @param layerType - type of layer (Paint, Group, etc.)
    // @param name - name of the layer (null-terminated wide string)
    virtual void OnBeginLayer(uint32_t layerId, uint32_t layerType, const wchar_t* name, bool visible, float opacity) = 0;

    // Optional callback when layer transforms are available
    virtual void OnLayerTransform(uint32_t layerId, const ImmCore::trans3d& localTransform, const ImmCore::trans3d& worldTransform, const ImmCore::trans3d& pivotTransform)
    {
        (void)layerId;
        (void)localTransform;
        (void)worldTransform;
        (void)pivotTransform;
    }

    virtual void OnPictureLayer(
        uint32_t layerId,
        uint32_t contentType,
        bool isViewerLocked,
        int width,
        int height,
        bool hasAlpha,
        const uint8_t* pixels,
        int pixelDataSize)
    {
        (void)layerId;
        (void)contentType;
        (void)isViewerLocked;
        (void)width;
        (void)height;
        (void)hasAlpha;
        (void)pixels;
        (void)pixelDataSize;
    }

    // Called when a paint layer's frame buffer info is available
    // @param frameRate - frames per second
    // @param numFrames - total number of frames in the animation
    // @param maxRepeatCount - 0 = infinite loops, >0 = max repeats
    virtual void OnPaintLayerInfo(uint32_t frameRate, uint32_t numFrames, uint32_t maxRepeatCount)
    {
        (void)frameRate;
        (void)numFrames;
        (void)maxRepeatCount;
    }

    // Called when the frame buffer is available (maps frame indices to drawing indices)
    // @param frameBuffer - array of drawing indices, one per frame
    // @param numFrames - length of the frameBuffer array
    virtual void OnFrameBuffer(const uint32_t* frameBuffer, uint32_t numFrames)
    {
        (void)frameBuffer;
        (void)numFrames;
    }

    // Called when a drawing within a paint layer begins loading
    // @param drawingId - index of the drawing within the layer
    virtual void OnBeginDrawing(uint32_t drawingId) = 0;

    // Optional callback providing the per-drawing scale used to decode quantized data.
    // This is read from the .imm and is needed to reconstruct widths (see Element::Point::mWid comment).
    virtual void OnDrawingBiggestStroke(uint32_t drawingId, float biggestStroke)
    {
        (void)drawingId;
        (void)biggestStroke;
    }

    // Called for each stroke (element) in a drawing after it has been decoded
    // @param strokeId - index of the stroke within the drawing
    // @param brushType - brush section type (Point, Segment, Circle, Ellipse, Square)
    // @param visibilityMode - visibility mode (FadePow2, Always)
    // @param numPoints - number of points in the stroke
    // @param points - array of decoded point data
    // @param bbox - bounding box of the stroke
    virtual void OnStroke(
        uint32_t strokeId,
        uint8_t brushType,
        uint8_t visibilityMode,
        uint32_t numPoints,
        const Point* points,
        const ImmCore::bound3& bbox
    ) = 0;

    // Called when a drawing finishes loading
    virtual void OnEndDrawing() = 0;

    // Called when a layer finishes loading
    virtual void OnEndLayer() = 0;
};

} // namespace ImmImporter
