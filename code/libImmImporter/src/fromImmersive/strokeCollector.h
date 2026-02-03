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
    virtual void OnBeginLayer(uint32_t layerId, uint32_t layerType, const wchar_t* name) = 0;

    // Called when a drawing within a paint layer begins loading
    // @param drawingId - index of the drawing within the layer
    virtual void OnBeginDrawing(uint32_t drawingId) = 0;

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
