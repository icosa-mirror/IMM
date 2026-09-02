#pragma once

#include <cstdint>

#include "element.h"

namespace ImmImporter
{
namespace PaintGeometry
{
    uint32_t GetSectionCount(Element::BrushSectionType brush);
    ImmCore::vec2 GetSectionPosition(Element::BrushSectionType brush, uint32_t sectionIndex);
    ImmCore::vec3 ComputeTangent(const Point* points, uint32_t pointCount, uint32_t pointIndex);
    void ComputeBasis(
        const Point* points,
        uint32_t pointCount,
        uint32_t pointIndex,
        ImmCore::vec3* tangent,
        ImmCore::vec3* basisU,
        ImmCore::vec3* basisV);
    ImmCore::vec3 ComputeVertexPosition(
        const Point* points,
        uint32_t pointCount,
        uint32_t pointIndex,
        Element::BrushSectionType brush,
        uint32_t sectionIndex,
        float width);
    uint32_t GetTriangleIndexCount(uint32_t pointCount, Element::BrushSectionType brush);
    bool BuildTriangleIndices(
        uint32_t pointCount,
        Element::BrushSectionType brush,
        uint32_t vertexBase,
        bool flipped,
        uint32_t* indices,
        uint32_t indexCapacity);
}
}
