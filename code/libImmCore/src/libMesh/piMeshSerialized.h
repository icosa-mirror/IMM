#pragma once

#include <cmath>
#include <cstring>
#include "piMesh.h"

namespace ImmCore
{
    // Validate the existing version-0 mesh representation before its allocating
    // reader consumes a document asset. All required platforms are little endian.
    inline bool piMeshValidateSerialized(const uint8_t* data, uint64_t size)
    {
        uint64_t cursor = 0;
        auto read = [&](uint32_t& value) {
            if (cursor > size || size - cursor < sizeof(value)) return false;
            std::memcpy(&value, data + cursor, sizeof(value));
            cursor += sizeof(value);
            return true;
        };
        auto skip = [&](uint64_t bytes) {
            if (cursor > size || bytes > size - cursor) return false;
            cursor += bytes;
            return true;
        };
        uint32_t value;
        if (!data || !read(value) || value != 0) return false;
        float bounds[6];
        for (float& component : bounds) {
            if (!read(value)) return false;
            std::memcpy(&component, &value, sizeof(component));
            if (!std::isfinite(component)) return false;
        }
        uint32_t streams, vertices = 0;
        if (!read(streams) || streams == 0 || streams > piMesh_MAXVERTEXARRAYS) return false;
        const uint32_t typeSizes[] = {1, 4, 4, 8, 2, 2};
        for (uint32_t stream = 0; stream < streams; ++stream) {
            uint32_t count, stride, divisor, elements;
            if (!read(count) || !read(stride) || !read(divisor) || !read(elements) ||
                count == 0 || stride == 0 || stride % 4 != 0 || divisor != 0 ||
                elements == 0 || elements > piMesh_MAXELEMS) return false;
            if (stream == 0) vertices = count;
            else if (count != vertices) return false;
            for (uint32_t element = 0; element < elements; ++element) {
                uint32_t type, components, normalize, offset;
                if (!read(type) || !read(components) || !read(normalize) || !read(offset) ||
                    type >= sizeof(typeSizes) / sizeof(typeSizes[0]) ||
                    components == 0 || components > 4 || normalize > 1 ||
                    uint64_t(offset) + components * typeSizes[type] > stride) return false;
            }
            if (!skip(uint64_t(count) * stride)) return false;
        }
        uint32_t topology, depth, arrays;
        if (!read(topology) || topology != uint32_t(piMesh::Type::Polys) ||
            !read(depth) || depth != uint32_t(piMesh::FaceData::Depth::Bits32) ||
            !read(arrays) || arrays == 0 || arrays > piMesh_MAXINDEXARRAYS) return false;
        for (uint32_t array = 0; array < arrays; ++array) {
            uint32_t faces;
            if (!read(faces) || faces == 0 || uint64_t(faces) * sizeof(piMesh::Face32) > size - cursor) return false;
            for (uint32_t face = 0; face < faces; ++face) {
                uint32_t count;
                if (!read(count) || (count != 3 && count != 4)) return false;
                for (uint32_t corner = 0; corner < 4; ++corner)
                    if (!read(value) || (corner < count && value >= vertices)) return false;
            }
        }
        return cursor == size;
    }
}
