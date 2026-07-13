#include "imm_web_decoder.h"

#include <cstring>
#include <limits>

static_assert(sizeof(ImmWebError) == 176u, "ImmWebError ABI size changed");
static_assert(sizeof(ImmWebDocumentSummary) == 72u, "ImmWebDocumentSummary ABI size changed");

namespace
{
    constexpr uint64_t kSignatureImmersive = 0x76697372656d6d49ULL;
    constexpr uint64_t kSignatureCategory = 0x79726f6765746143ULL;
    constexpr uint64_t kSignatureCoordinateSystem = 0x73795364726f6f43ULL;
    constexpr uint64_t kSignatureSequence = 0x65636e6575716553ULL;
    constexpr uint64_t kSignatureResourceTable = 0x656c626174736552ULL;
    constexpr uint32_t kSupportedFormatVersion = 0x00010001u;
    constexpr uint64_t kChunkHeaderSize = 16u;
    constexpr uint64_t kResourceEntrySize = 20u;

    bool canRead(uint64_t offset, uint64_t length, uint64_t sourceSize)
    {
        return offset <= sourceSize && length <= sourceSize - offset;
    }

    uint32_t readU32(const uint8_t* source)
    {
        return static_cast<uint32_t>(source[0]) |
               (static_cast<uint32_t>(source[1]) << 8u) |
               (static_cast<uint32_t>(source[2]) << 16u) |
               (static_cast<uint32_t>(source[3]) << 24u);
    }

    uint64_t readU64(const uint8_t* source)
    {
        return static_cast<uint64_t>(readU32(source)) |
               (static_cast<uint64_t>(readU32(source + 4)) << 32u);
    }

    void clearOutputs(ImmWebDocumentSummary* summary, ImmWebError* error)
    {
        if (summary != nullptr)
        {
            std::memset(summary, 0, sizeof(*summary));
            summary->schema_version = IMM_WEB_OUTPUT_SCHEMA_VERSION;
        }
        if (error != nullptr)
        {
            std::memset(error, 0, sizeof(*error));
        }
    }

    ImmWebStatus fail(ImmWebError* error, ImmWebStatus status, uint64_t offset, const char* message)
    {
        if (error != nullptr)
        {
            error->status = static_cast<uint32_t>(status);
            error->byte_offset = offset;
            const size_t sourceLength = std::strlen(message);
            const size_t maximumLength = IMM_WEB_ERROR_MESSAGE_CAPACITY - 1u;
            const size_t copyLength = sourceLength < maximumLength ? sourceLength : maximumLength;
            std::memcpy(error->message, message, copyLength);
            error->message[copyLength] = '\0';
        }
        return status;
    }
}

extern "C" uint32_t imm_web_schema_version(void)
{
    return IMM_WEB_OUTPUT_SCHEMA_VERSION;
}

extern "C" ImmWebStatus imm_web_inspect(
    const uint8_t* source,
    size_t sourceSizeValue,
    ImmWebDocumentSummary* outSummary,
    ImmWebError* outError)
{
    clearOutputs(outSummary, outError);
    if (source == nullptr || outSummary == nullptr)
    {
        return fail(outError, IMM_WEB_STATUS_INVALID_ARGUMENT, 0u, "Source and summary are required");
    }

    const uint64_t sourceSize = static_cast<uint64_t>(sourceSizeValue);
    outSummary->source_size = sourceSize;
    if (!canRead(0u, kChunkHeaderSize, sourceSize))
    {
        return fail(outError, IMM_WEB_STATUS_TRUNCATED, 0u, "IMM source does not contain a complete chunk header");
    }

    uint64_t offset = 0u;
    bool foundSequence = false;
    bool foundResourceTable = false;

    while (!foundResourceTable)
    {
        if (!canRead(offset, kChunkHeaderSize, sourceSize))
        {
            return fail(outError, IMM_WEB_STATUS_TRUNCATED, offset, "Top-level chunk header is truncated");
        }

        const uint64_t signature = readU64(source + offset);
        const uint64_t chunkSize = readU64(source + offset + 8u);
        const uint64_t payloadOffset = offset + kChunkHeaderSize;
        if (!canRead(payloadOffset, chunkSize, sourceSize))
        {
            return fail(outError, IMM_WEB_STATUS_INVALID_CHUNK, offset, "Top-level chunk exceeds source bounds");
        }

        outSummary->chunk_count++;
        if (outSummary->chunk_count == 1u && signature != kSignatureImmersive)
        {
            return fail(outError, IMM_WEB_STATUS_INVALID_SIGNATURE, offset, "First chunk is not Immersiv");
        }

        if (signature == kSignatureImmersive)
        {
            if (chunkSize < sizeof(uint32_t))
            {
                return fail(outError, IMM_WEB_STATUS_TRUNCATED, payloadOffset, "Immersiv version is truncated");
            }
            outSummary->format_version = readU32(source + payloadOffset);
            outSummary->chunk_flags |= IMM_WEB_CHUNK_IMMERSIVE;
            if (outSummary->format_version != kSupportedFormatVersion)
            {
                return fail(outError, IMM_WEB_STATUS_UNSUPPORTED_VERSION, payloadOffset, "Unsupported IMM format version");
            }
        }
        else if (signature == kSignatureCoordinateSystem)
        {
            outSummary->chunk_flags |= IMM_WEB_CHUNK_COORDINATE_SYSTEM;
        }
        else if (signature == kSignatureCategory)
        {
            if (chunkSize < 2u)
            {
                return fail(outError, IMM_WEB_STATUS_TRUNCATED, payloadOffset, "Category data is truncated");
            }
            outSummary->sequence_type = source[payloadOffset];
            outSummary->sequence_capabilities = source[payloadOffset + 1u];
            outSummary->chunk_flags |= IMM_WEB_CHUNK_CATEGORY;
        }
        else if (signature == kSignatureSequence)
        {
            outSummary->sequence_offset = payloadOffset;
            outSummary->sequence_size = chunkSize;
            outSummary->chunk_flags |= IMM_WEB_CHUNK_SEQUENCE;
            foundSequence = true;
        }
        else if (signature == kSignatureResourceTable)
        {
            if (chunkSize < sizeof(uint32_t))
            {
                return fail(outError, IMM_WEB_STATUS_INVALID_RESOURCE_TABLE, payloadOffset, "Resource table count is truncated");
            }

            const uint32_t assetCount = readU32(source + payloadOffset);
            const uint64_t maximumEntries = (chunkSize - sizeof(uint32_t)) / kResourceEntrySize;
            if (static_cast<uint64_t>(assetCount) > maximumEntries)
            {
                return fail(outError, IMM_WEB_STATUS_INVALID_RESOURCE_TABLE, payloadOffset, "Resource table entries exceed chunk bounds");
            }

            uint64_t entryOffset = payloadOffset + sizeof(uint32_t);
            for (uint32_t index = 0u; index < assetCount; ++index)
            {
                const uint64_t assetOffset = readU64(source + entryOffset + 4u);
                const uint64_t assetSize = readU64(source + entryOffset + 12u);
                if (!canRead(assetOffset, assetSize, sourceSize))
                {
                    return fail(outError, IMM_WEB_STATUS_INVALID_RESOURCE_TABLE, entryOffset, "Resource entry exceeds source bounds");
                }
                entryOffset += kResourceEntrySize;
            }

            outSummary->resource_table_offset = payloadOffset;
            outSummary->resource_table_size = chunkSize;
            outSummary->asset_count = assetCount;
            outSummary->chunk_flags |= IMM_WEB_CHUNK_RESOURCE_TABLE;
            foundResourceTable = true;
        }

        if (chunkSize > std::numeric_limits<uint64_t>::max() - payloadOffset)
        {
            return fail(outError, IMM_WEB_STATUS_INVALID_CHUNK, offset, "Chunk end offset overflows");
        }
        offset = payloadOffset + chunkSize;
    }

    if (!foundSequence)
    {
        return fail(outError, IMM_WEB_STATUS_MISSING_SEQUENCE, offset, "IMM sequence chunk was not found");
    }
    if (!foundResourceTable)
    {
        return fail(outError, IMM_WEB_STATUS_MISSING_RESOURCE_TABLE, offset, "IMM resource table was not found");
    }

    return IMM_WEB_STATUS_OK;
}
