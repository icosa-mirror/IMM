#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define IMM_WEB_OUTPUT_SCHEMA_VERSION 1u
#define IMM_WEB_ERROR_MESSAGE_CAPACITY 160u

typedef enum ImmWebStatus
{
    IMM_WEB_STATUS_OK = 0,
    IMM_WEB_STATUS_INVALID_ARGUMENT = 1,
    IMM_WEB_STATUS_TRUNCATED = 2,
    IMM_WEB_STATUS_INVALID_SIGNATURE = 3,
    IMM_WEB_STATUS_UNSUPPORTED_VERSION = 4,
    IMM_WEB_STATUS_INVALID_CHUNK = 5,
    IMM_WEB_STATUS_MISSING_SEQUENCE = 6,
    IMM_WEB_STATUS_MISSING_RESOURCE_TABLE = 7,
    IMM_WEB_STATUS_INVALID_RESOURCE_TABLE = 8
} ImmWebStatus;

typedef enum ImmWebChunkFlags
{
    IMM_WEB_CHUNK_IMMERSIVE = 1u << 0u,
    IMM_WEB_CHUNK_COORDINATE_SYSTEM = 1u << 1u,
    IMM_WEB_CHUNK_CATEGORY = 1u << 2u,
    IMM_WEB_CHUNK_SEQUENCE = 1u << 3u,
    IMM_WEB_CHUNK_RESOURCE_TABLE = 1u << 4u
} ImmWebChunkFlags;

typedef struct ImmWebError
{
    uint32_t status;
    uint32_t reserved;
    uint64_t byte_offset;
    char message[IMM_WEB_ERROR_MESSAGE_CAPACITY];
} ImmWebError;

typedef struct ImmWebDocumentSummary
{
    uint32_t schema_version;
    uint32_t format_version;
    uint64_t source_size;
    uint32_t chunk_count;
    uint32_t chunk_flags;
    uint32_t sequence_type;
    uint32_t sequence_capabilities;
    uint64_t sequence_offset;
    uint64_t sequence_size;
    uint64_t resource_table_offset;
    uint64_t resource_table_size;
    uint32_t asset_count;
    uint32_t reserved;
} ImmWebDocumentSummary;

uint32_t imm_web_schema_version(void);

ImmWebStatus imm_web_inspect(
    const uint8_t* source,
    size_t source_size,
    ImmWebDocumentSummary* out_summary,
    ImmWebError* out_error);

#if defined(__cplusplus)
}
#endif
