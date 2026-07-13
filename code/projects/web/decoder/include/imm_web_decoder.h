#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define IMM_WEB_OUTPUT_SCHEMA_VERSION 5u
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
    IMM_WEB_STATUS_INVALID_RESOURCE_TABLE = 8,
    IMM_WEB_STATUS_SCENE_DECODE_FAILED = 9
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

typedef struct ImmWebLayerInfo
{
    uint32_t id;
    uint32_t type;
    uint32_t drawing_count;
    uint32_t visible;
    float opacity;
    uint32_t default_spawn;
    char name[256];
} ImmWebLayerInfo;

typedef struct ImmWebTransform
{
    float rotation[4];
    float scale;
    uint32_t flip;
    float translation[3];
} ImmWebTransform;

typedef struct ImmWebAnimationInfo
{
    uint32_t frame_rate;
    uint32_t frame_count;
    uint32_t max_repeat_count;
    uint32_t reserved;
} ImmWebAnimationInfo;

typedef struct ImmWebStrokeInfo
{
    uint32_t brush_type;
    uint32_t visibility_mode;
    uint32_t point_count;
    uint32_t reserved;
    float bounds_min[3];
    float bounds_max[3];
} ImmWebStrokeInfo;

typedef struct ImmWebStrokePoint
{
    float position[3];
    float normal[3];
    float direction[3];
    float color[3];
    float alpha;
    float width;
} ImmWebStrokePoint;

typedef struct ImmWebPictureInfo
{
    uint32_t layer_id;
    uint32_t content_type;
    uint32_t viewer_locked;
    uint32_t width;
    uint32_t height;
    uint32_t has_alpha;
    uint32_t data_size;
} ImmWebPictureInfo;

typedef struct ImmWebSoundInfo
{
    uint32_t layer_id;
    uint32_t type;
    uint32_t asset_format;
    uint32_t channel_count;
    uint32_t looping;
    uint32_t play_on_load;
    float gain;
    uint32_t attenuation_type;
    float attenuation_min;
    float attenuation_max;
    uint32_t modifier_type;
    float modifier_parameters[4];
    uint32_t data_size;
} ImmWebSoundInfo;

typedef struct ImmWebPlaybackInfo
{
    uint32_t ticks_per_second;
    uint32_t animate_on_start;
    uint32_t timeline_layer_count;
    uint32_t chapter_count;
    int64_t duration_ticks;
    uint32_t reserved[2];
} ImmWebPlaybackInfo;

typedef struct ImmWebTimelineLayerInfo
{
    uint32_t id;
    int32_t parent_id;
    uint32_t type;
    uint32_t flags;
    float opacity;
    uint32_t max_repeat_count;
    int64_t duration_ticks;
    uint32_t key_count;
    uint32_t content_layer_index;
    char name[256];
} ImmWebTimelineLayerInfo;

typedef struct ImmWebAnimationKey
{
    uint32_t property;
    uint32_t interpolation;
    int64_t time_ticks;
    uint32_t bool_value;
    uint32_t uint_value;
    float float_value;
    uint32_t reserved;
    double double_value;
    ImmWebTransform transform_value;
} ImmWebAnimationKey;

typedef struct ImmWebChapterInfo
{
    int64_t start_ticks;
    int64_t end_ticks;
    uint32_t marker_action;
    uint32_t reserved;
} ImmWebChapterInfo;

typedef struct ImmWebKeepAliveInfo
{
    uint32_t type;
    uint32_t waveform;
    float parameters[6];
} ImmWebKeepAliveInfo;

uint32_t imm_web_schema_version(void);

ImmWebStatus imm_web_inspect(
    const uint8_t* source,
    size_t source_size,
    ImmWebDocumentSummary* out_summary,
    ImmWebError* out_error);

ImmWebStatus imm_web_decode_scene(
    const uint8_t* source,
    size_t source_size,
    ImmWebError* out_error);
ImmWebStatus imm_web_open_scene_metadata(
    const uint8_t* source,
    size_t source_size,
    ImmWebError* out_error);
ImmWebStatus imm_web_decode_drawing(
    uint32_t layer_id,
    uint32_t drawing_id,
    ImmWebError* out_error);
ImmWebStatus imm_web_decode_layer_asset(
    uint32_t layer_id,
    ImmWebError* out_error);
void imm_web_release_scene(void);
uint32_t imm_web_get_layer_count(void);
uint32_t imm_web_get_background_color(float* out_rgb, uint32_t float_capacity);
uint32_t imm_web_get_layer_info(uint32_t layer_index, ImmWebLayerInfo* out_info);
uint32_t imm_web_get_layer_transforms(
    uint32_t layer_index,
    ImmWebTransform* out_local,
    ImmWebTransform* out_world,
    ImmWebTransform* out_pivot);
uint32_t imm_web_get_animation_info(uint32_t layer_index, ImmWebAnimationInfo* out_info);
uint32_t imm_web_get_frame_buffer(uint32_t layer_index, uint32_t* out_frames, uint32_t frame_capacity);
uint32_t imm_web_get_drawing_count(uint32_t layer_index);
float imm_web_get_drawing_biggest_stroke(uint32_t layer_index, uint32_t drawing_index);
uint32_t imm_web_get_stroke_count(uint32_t layer_index, uint32_t drawing_index);
uint32_t imm_web_get_stroke_info(
    uint32_t layer_index,
    uint32_t drawing_index,
    uint32_t stroke_index,
    ImmWebStrokeInfo* out_info);
uint32_t imm_web_get_stroke_points(
    uint32_t layer_index,
    uint32_t drawing_index,
    uint32_t stroke_index,
    ImmWebStrokePoint* out_points,
    uint32_t point_capacity);
uint32_t imm_web_get_stroke_point_times(
    uint32_t layer_index,
    uint32_t drawing_index,
    uint32_t stroke_index,
    float* out_times,
    uint32_t point_capacity);
uint32_t imm_web_get_picture_info(uint32_t layer_index, ImmWebPictureInfo* out_info);
uint32_t imm_web_get_picture_pixels(uint32_t layer_index, uint8_t* out_pixels, uint32_t byte_capacity);
uint32_t imm_web_get_sound_info(uint32_t timeline_layer_index, ImmWebSoundInfo* out_info);
uint32_t imm_web_get_sound_bytes(uint32_t timeline_layer_index, uint8_t* out_bytes, uint32_t byte_capacity);
uint32_t imm_web_get_playback_info(ImmWebPlaybackInfo* out_info);
uint32_t imm_web_get_timeline_layer_info(uint32_t layer_index, ImmWebTimelineLayerInfo* out_info);
uint32_t imm_web_get_timeline_layer_transforms(
    uint32_t layer_index,
    ImmWebTransform* out_local,
    ImmWebTransform* out_world,
    ImmWebTransform* out_pivot);
uint32_t imm_web_get_animation_key(
    uint32_t layer_index,
    uint32_t key_index,
    ImmWebAnimationKey* out_key);
uint32_t imm_web_get_chapter_info(uint32_t chapter_index, ImmWebChapterInfo* out_info);
uint32_t imm_web_get_keep_alive_info(uint32_t layer_index, ImmWebKeepAliveInfo* out_info);

#if defined(__cplusplus)
}
#endif
