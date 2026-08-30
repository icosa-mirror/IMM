#include "paintGeometry.h"

#include <cmath>

using namespace ImmCore;

namespace ImmImporter::PaintGeometry
{
    namespace
    {
        constexpr float kDirectionEpsilon = 0.0000001f;
        constexpr float kTau = 6.2831f;
    }

    uint32_t GetSectionCount(Element::BrushSectionType brush)
    {
        const uint32_t brushIndex = static_cast<uint32_t>(brush);
        return brushIndex < static_cast<uint32_t>(Element::BrushSectionType::Count)
            ? static_cast<uint32_t>(Element::kSectionsLUT[brushIndex])
            : 0u;
    }

    vec2 GetSectionPosition(Element::BrushSectionType brush, uint32_t sectionIndex)
    {
        const uint32_t sectionCount = GetSectionCount(brush);
        if (sectionCount == 0u || sectionIndex >= sectionCount)
            return vec2(0.0f);
        if (brush == Element::BrushSectionType::Point || brush == Element::BrushSectionType::Segment)
            return sectionIndex == 0u ? vec2(-1.0f, 0.0f) : vec2(1.0f, 0.0f);
        if (brush == Element::BrushSectionType::Square)
        {
            static const vec2 square[4] = {
                vec2(-1.0f, -1.0f),
                vec2(1.0f, -1.0f),
                vec2(1.0f, 1.0f),
                vec2(-1.0f, 1.0f),
            };
            return square[sectionIndex];
        }

        const float angle = kTau * static_cast<float>(sectionIndex) / static_cast<float>(sectionCount);
        const float eccentricity = brush == Element::BrushSectionType::Ellipse ? 0.3f : 1.0f;
        return vec2(std::cos(angle), std::sin(angle) * eccentricity);
    }

    vec3 ComputeTangent(const Point* points, uint32_t pointCount, uint32_t pointIndex)
    {
        if (points == nullptr || pointCount == 0u || pointIndex >= pointCount)
            return vec3(0.0f);

        vec3 forward(0.0f);
        for (uint32_t candidate = pointIndex + 1u; candidate < pointCount; ++candidate)
        {
            const vec3 difference = points[candidate].mPos - points[pointIndex].mPos;
            const float magnitude = length(difference);
            if (magnitude >= kDirectionEpsilon)
            {
                forward = difference / magnitude;
                break;
            }
        }

        vec3 backward(0.0f);
        for (uint32_t candidate = pointIndex; candidate > 0u; --candidate)
        {
            const vec3 difference = points[pointIndex].mPos - points[candidate - 1u].mPos;
            const float magnitude = length(difference);
            if (magnitude >= kDirectionEpsilon)
            {
                backward = difference / magnitude;
                break;
            }
        }

        const vec3 averaged = forward + backward;
        const float magnitude = length(averaged);
        if (magnitude > kDirectionEpsilon)
            return averaged / magnitude;

        return normalize(points[pointCount - 1u].mPos - points[0].mPos + vec3(0.000001f, 0.000002f, 0.000003f));
    }

    void ComputeBasis(
        const Point* points,
        uint32_t pointCount,
        uint32_t pointIndex,
        vec3* tangent,
        vec3* basisU,
        vec3* basisV)
    {
        if (tangent == nullptr || basisU == nullptr || basisV == nullptr)
            return;

        *tangent = ComputeTangent(points, pointCount, pointIndex);
        if (points == nullptr || pointCount == 0u || pointIndex >= pointCount)
        {
            *basisU = vec3(0.0f);
            *basisV = vec3(0.0f);
            return;
        }

        *basisU = cross(points[pointIndex].mNor, *tangent);
        const float basisLength = length(*basisU);
        if (basisLength >= kDirectionEpsilon)
            *basisU = *basisU / basisLength;
        else if (std::fabs(tangent->x) < 0.9f)
            *basisU = vec3(0.0f, tangent->z, tangent->y);
        else if (std::fabs(tangent->y) < 0.9f)
            *basisU = vec3(-tangent->z, 0.0f, tangent->x);
        else
            *basisU = vec3(tangent->y, -tangent->x, 0.0f);

        *basisV = normalize(cross(*tangent, *basisU));
    }

    vec3 ComputeVertexPosition(
        const Point* points,
        uint32_t pointCount,
        uint32_t pointIndex,
        Element::BrushSectionType brush,
        uint32_t sectionIndex,
        float width)
    {
        if (points == nullptr || pointIndex >= pointCount)
            return vec3(0.0f);
        vec3 tangent;
        vec3 basisU;
        vec3 basisV;
        ComputeBasis(points, pointCount, pointIndex, &tangent, &basisU, &basisV);
        const vec2 section = GetSectionPosition(brush, sectionIndex);
        return points[pointIndex].mPos + width * (basisU * section.x + basisV * section.y);
    }

    uint32_t GetTriangleIndexCount(uint32_t pointCount, Element::BrushSectionType brush)
    {
        if (pointCount < 2u)
            return 0u;
        return (pointCount - 1u) * GetSectionCount(brush) * 6u;
    }

    bool BuildTriangleIndices(
        uint32_t pointCount,
        Element::BrushSectionType brush,
        uint32_t vertexBase,
        bool flipped,
        uint32_t* indices,
        uint32_t indexCapacity)
    {
        const uint32_t sectionCount = GetSectionCount(brush);
        const uint32_t required = GetTriangleIndexCount(pointCount, brush);
        if (required == 0u)
            return true;
        if (indices == nullptr || indexCapacity < required)
            return false;

        uint32_t target = 0u;
        for (uint32_t pointIndex = 0u; pointIndex + 1u < pointCount; ++pointIndex)
        {
            const uint32_t current = vertexBase + pointIndex * sectionCount;
            const uint32_t next = current + sectionCount;
            for (uint32_t sectionIndex = 0u; sectionIndex < sectionCount; ++sectionIndex)
            {
                const uint32_t nextSection = (sectionIndex + 1u) % sectionCount;
                const uint32_t triangle[6] = {
                    current + sectionIndex,
                    current + nextSection,
                    next + sectionIndex,
                    current + nextSection,
                    next + nextSection,
                    next + sectionIndex,
                };
                if (flipped)
                {
                    indices[target++] = triangle[0];
                    indices[target++] = triangle[2];
                    indices[target++] = triangle[1];
                    indices[target++] = triangle[3];
                    indices[target++] = triangle[5];
                    indices[target++] = triangle[4];
                }
                else
                {
                    for (uint32_t component = 0u; component < 6u; ++component)
                        indices[target++] = triangle[component];
                }
            }
        }
        return true;
    }
}
