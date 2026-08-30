#include "libImmImporter/src/document/layerPaint/paintGeometry.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "FAIL: " << message << '\n';
        return condition;
    }

    bool nearlyEqual(float left, float right)
    {
        return std::fabs(left - right) < 0.00001f;
    }
}

int main()
{
    using ImmImporter::Element;
    using namespace ImmImporter::PaintGeometry;

    bool result = true;
    const uint32_t expectedSections[] = {2u, 2u, 7u, 7u, 4u};
    for (uint32_t brush = 0u; brush < 5u; ++brush)
    {
        const auto type = static_cast<Element::BrushSectionType>(brush);
        result &= expect(GetSectionCount(type) == expectedSections[brush], "brush section count changed");
        result &= expect(GetTriangleIndexCount(2u, type) == expectedSections[brush] * 6u,
            "two-point triangle count changed");
    }

    ImmImporter::Point points[3]{};
    points[0].mPos = ImmCore::vec3(0.0f, 0.0f, 0.0f);
    points[1].mPos = ImmCore::vec3(0.0f, 0.0f, 0.0f);
    points[2].mPos = ImmCore::vec3(0.0f, 0.0f, 2.0f);
    for (auto& point : points)
        point.mNor = ImmCore::vec3(0.0f, 1.0f, 0.0f);

    const ImmCore::vec3 tangent = ComputeTangent(points, 3u, 0u);
    result &= expect(nearlyEqual(tangent.x, 0.0f) && nearlyEqual(tangent.y, 0.0f) && nearlyEqual(tangent.z, 1.0f),
        "duplicate endpoint tangent changed");
    const ImmCore::vec3 segmentVertex = ComputeVertexPosition(
        points, 3u, 0u, Element::BrushSectionType::Segment, 0u, 1.0f);
    result &= expect(nearlyEqual(segmentVertex.x, -1.0f) && nearlyEqual(segmentVertex.y, 0.0f),
        "segment cross-section position changed");

    std::vector<uint32_t> indices(GetTriangleIndexCount(2u, Element::BrushSectionType::Circle));
    result &= expect(BuildTriangleIndices(
        2u, Element::BrushSectionType::Circle, 0u, false, indices.data(), static_cast<uint32_t>(indices.size())),
        "circle indices were not built");
    const uint32_t expectedFirst[] = {0u, 1u, 7u, 1u, 8u, 7u};
    for (uint32_t index = 0u; index < 6u; ++index)
        result &= expect(indices[index] == expectedFirst[index], "circle triangle topology changed");

    std::vector<uint32_t> flipped(indices.size());
    result &= expect(BuildTriangleIndices(
        2u, Element::BrushSectionType::Circle, 0u, true, flipped.data(), static_cast<uint32_t>(flipped.size())),
        "flipped circle indices were not built");
    result &= expect(flipped[0] == 0u && flipped[1] == 7u && flipped[2] == 1u,
        "flipped winding changed");

    if (result)
        std::cout << "IMM shared paint geometry tests passed\n";
    return result ? 0 : 1;
}
