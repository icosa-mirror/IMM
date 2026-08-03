#include "imm_viewer_compositor_effect.h"

#include "appImmGodot/src/imm_godot_plugin.h"
#include "imm_viewer_metal_frame.h"
#include "imm_viewer_vulkan_frame.h"

#include <godot_cpp/classes/render_data.hpp>
#include <godot_cpp/classes/render_scene_buffers.hpp>
#include <godot_cpp/classes/render_scene_buffers_rd.hpp>
#include <godot_cpp/classes/rd_pipeline_color_blend_state.hpp>
#include <godot_cpp/classes/rd_pipeline_color_blend_state_attachment.hpp>
#include <godot_cpp/classes/rd_pipeline_depth_stencil_state.hpp>
#include <godot_cpp/classes/rd_pipeline_multisample_state.hpp>
#include <godot_cpp/classes/rd_pipeline_rasterization_state.hpp>
#include <godot_cpp/classes/rd_sampler_state.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <cstdlib>

using namespace godot;

namespace
{
    struct QueuedRenderRequest
    {
        bool queued = false;
        int camera_id = 0;
        int width = 0;
        int height = 0;
    };

    std::mutex g_queued_render_mutex;
    QueuedRenderRequest g_queued_render_request;

    struct CompositeResources
    {
        RID shader;
        RID depth_shader;
        RID sampler;
        RID pipeline;
        RID depth_pipeline;
        int64_t framebuffer_format = RenderingDevice::INVALID_FORMAT_ID;
        int64_t depth_framebuffer_format = RenderingDevice::INVALID_FORMAT_ID;
    };

    CompositeResources *g_composite_resources = nullptr;

    void release_composite_resources(RenderingDevice *rendering_device)
    {
        if (rendering_device == nullptr || g_composite_resources == nullptr)
        {
            return;
        }

        if (g_composite_resources->pipeline.is_valid())
        {
            rendering_device->free_rid(g_composite_resources->pipeline);
        }
        if (g_composite_resources->depth_pipeline.is_valid())
        {
            rendering_device->free_rid(g_composite_resources->depth_pipeline);
        }
        if (g_composite_resources->shader.is_valid())
        {
            rendering_device->free_rid(g_composite_resources->shader);
        }
        if (g_composite_resources->depth_shader.is_valid())
        {
            rendering_device->free_rid(g_composite_resources->depth_shader);
        }
        if (g_composite_resources->sampler.is_valid())
        {
            rendering_device->free_rid(g_composite_resources->sampler);
        }

        delete g_composite_resources;
        g_composite_resources = nullptr;
    }

    bool ensure_composite_resources(RenderingDevice *rendering_device, int64_t framebuffer_format)
    {
        if (rendering_device == nullptr || framebuffer_format == RenderingDevice::INVALID_FORMAT_ID)
        {
            return false;
        }

        if (g_composite_resources == nullptr)
        {
            g_composite_resources = new CompositeResources();
        }

        if (!g_composite_resources->shader.is_valid())
        {
            Ref<RDShaderSource> shader_source;
            shader_source.instantiate();
            shader_source->set_language(RenderingDevice::SHADER_LANGUAGE_GLSL);
            shader_source->set_stage_source(RenderingDevice::SHADER_STAGE_VERTEX, R"(
#version 450

layout(location = 0) out vec2 uv_interp;

void main() {
    vec2 positions[4] = vec2[](vec2(-1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0));
    vec2 pos = positions[gl_VertexIndex];
    uv_interp = vec2((1.0 - pos.x) * 0.5, (pos.y + 1.0) * 0.5);
    gl_Position = vec4(pos, 0.0, 1.0);
}
)");
            shader_source->set_stage_source(RenderingDevice::SHADER_STAGE_FRAGMENT, R"(
#version 450

layout(location = 0) in vec2 uv_interp;
layout(set = 0, binding = 0) uniform sampler2D source_color;
layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = texture(source_color, uv_interp);
}
)");

            Ref<RDShaderSPIRV> spirv = rendering_device->shader_compile_spirv_from_source(shader_source, false);
            if (!spirv.is_valid() ||
                !spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_VERTEX).is_empty() ||
                !spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_FRAGMENT).is_empty())
            {
                return false;
            }
            g_composite_resources->shader = rendering_device->shader_create_from_spirv(spirv, "IMM Godot Metal composite");
            if (!g_composite_resources->shader.is_valid())
            {
                return false;
            }
        }

        if (!g_composite_resources->sampler.is_valid())
        {
            Ref<RDSamplerState> sampler_state;
            sampler_state.instantiate();
            sampler_state->set_mag_filter(RenderingDevice::SAMPLER_FILTER_LINEAR);
            sampler_state->set_min_filter(RenderingDevice::SAMPLER_FILTER_LINEAR);
            sampler_state->set_mip_filter(RenderingDevice::SAMPLER_FILTER_NEAREST);
            sampler_state->set_repeat_u(RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
            sampler_state->set_repeat_v(RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
            sampler_state->set_repeat_w(RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
            g_composite_resources->sampler = rendering_device->sampler_create(sampler_state);
            if (!g_composite_resources->sampler.is_valid())
            {
                return false;
            }
        }

        if (!g_composite_resources->pipeline.is_valid() || g_composite_resources->framebuffer_format != framebuffer_format)
        {
            Ref<RDPipelineRasterizationState> raster_state;
            raster_state.instantiate();
            raster_state->set_cull_mode(RenderingDevice::POLYGON_CULL_DISABLED);

            Ref<RDPipelineMultisampleState> multisample_state;
            multisample_state.instantiate();

            Ref<RDPipelineDepthStencilState> depth_state;
            depth_state.instantiate();
            depth_state->set_enable_depth_test(false);
            depth_state->set_enable_depth_write(false);

            Ref<RDPipelineColorBlendStateAttachment> blend_attachment;
            blend_attachment.instantiate();
            blend_attachment->set_enable_blend(false);
            blend_attachment->set_write_r(true);
            blend_attachment->set_write_g(true);
            blend_attachment->set_write_b(true);
            blend_attachment->set_write_a(true);

            TypedArray<Ref<RDPipelineColorBlendStateAttachment>> blend_attachments;
            blend_attachments.push_back(blend_attachment);
            Ref<RDPipelineColorBlendState> blend_state;
            blend_state.instantiate();
            blend_state->set_attachments(blend_attachments);

            g_composite_resources->pipeline = rendering_device->render_pipeline_create(
                g_composite_resources->shader,
                framebuffer_format,
                RenderingDevice::INVALID_FORMAT_ID,
                RenderingDevice::RENDER_PRIMITIVE_TRIANGLE_STRIPS,
                raster_state,
                multisample_state,
                depth_state,
                blend_state);
            g_composite_resources->framebuffer_format = framebuffer_format;
        }

        return g_composite_resources->pipeline.is_valid();
    }

    bool ensure_depth_composite_resources(RenderingDevice *rendering_device, int64_t framebuffer_format)
    {
        if (rendering_device == nullptr || framebuffer_format == RenderingDevice::INVALID_FORMAT_ID)
        {
            return false;
        }

        if (g_composite_resources == nullptr)
        {
            g_composite_resources = new CompositeResources();
        }

        if (!g_composite_resources->depth_shader.is_valid())
        {
            Ref<RDShaderSource> shader_source;
            shader_source.instantiate();
            shader_source->set_language(RenderingDevice::SHADER_LANGUAGE_GLSL);
            shader_source->set_stage_source(RenderingDevice::SHADER_STAGE_VERTEX, R"(
#version 450

layout(location = 0) out vec2 uv_interp;

void main() {
    vec2 positions[4] = vec2[](vec2(-1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0));
    vec2 pos = positions[gl_VertexIndex];
    uv_interp = vec2((1.0 - pos.x) * 0.5, (pos.y + 1.0) * 0.5);
    gl_Position = vec4(pos, 0.0, 1.0);
}
)");
            shader_source->set_stage_source(RenderingDevice::SHADER_STAGE_FRAGMENT, R"(
#version 450

layout(location = 0) in vec2 uv_interp;
layout(set = 0, binding = 0) uniform sampler2D source_color;
layout(set = 0, binding = 1) uniform sampler2D source_depth;
layout(set = 0, binding = 2) uniform sampler2D scene_depth;
layout(location = 0) out vec4 frag_color;

void main() {
    vec4 imm_color = texture(source_color, uv_interp);
    float imm_depth = texture(source_depth, uv_interp).r;
    float host_depth = texture(scene_depth, uv_interp).r;
    // Both the IMM intermediate depth and Godot's Vulkan scene depth use
    // normal zero-to-one depth (near=0, far=1). Inverting the host sample
    // here made every IMM fragment appear behind the scene and left only
    // the sky sphere in the final composition.
    if (imm_color.a <= 0.01 || imm_depth >= 0.9999 || imm_depth > host_depth) {
        discard;
    }
    frag_color = imm_color;
}
)");

            Ref<RDShaderSPIRV> spirv = rendering_device->shader_compile_spirv_from_source(shader_source, false);
            if (!spirv.is_valid() ||
                !spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_VERTEX).is_empty() ||
                !spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_FRAGMENT).is_empty())
            {
                return false;
            }
            g_composite_resources->depth_shader = rendering_device->shader_create_from_spirv(spirv, "IMM Godot Vulkan depth composite");
            if (!g_composite_resources->depth_shader.is_valid())
            {
                return false;
            }
        }

        if (!g_composite_resources->sampler.is_valid())
        {
            Ref<RDSamplerState> sampler_state;
            sampler_state.instantiate();
            sampler_state->set_mag_filter(RenderingDevice::SAMPLER_FILTER_NEAREST);
            sampler_state->set_min_filter(RenderingDevice::SAMPLER_FILTER_NEAREST);
            sampler_state->set_mip_filter(RenderingDevice::SAMPLER_FILTER_NEAREST);
            sampler_state->set_repeat_u(RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
            sampler_state->set_repeat_v(RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
            sampler_state->set_repeat_w(RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
            g_composite_resources->sampler = rendering_device->sampler_create(sampler_state);
            if (!g_composite_resources->sampler.is_valid())
            {
                return false;
            }
        }

        if (!g_composite_resources->depth_pipeline.is_valid() || g_composite_resources->depth_framebuffer_format != framebuffer_format)
        {
            Ref<RDPipelineRasterizationState> raster_state;
            raster_state.instantiate();
            raster_state->set_cull_mode(RenderingDevice::POLYGON_CULL_DISABLED);

            Ref<RDPipelineMultisampleState> multisample_state;
            multisample_state.instantiate();

            Ref<RDPipelineDepthStencilState> depth_state;
            depth_state.instantiate();
            depth_state->set_enable_depth_test(false);
            depth_state->set_enable_depth_write(false);

            Ref<RDPipelineColorBlendStateAttachment> blend_attachment;
            blend_attachment.instantiate();
            blend_attachment->set_enable_blend(false);
            blend_attachment->set_write_r(true);
            blend_attachment->set_write_g(true);
            blend_attachment->set_write_b(true);
            blend_attachment->set_write_a(true);

            TypedArray<Ref<RDPipelineColorBlendStateAttachment>> blend_attachments;
            blend_attachments.push_back(blend_attachment);
            Ref<RDPipelineColorBlendState> blend_state;
            blend_state.instantiate();
            blend_state->set_attachments(blend_attachments);

            g_composite_resources->depth_pipeline = rendering_device->render_pipeline_create(
                g_composite_resources->depth_shader,
                framebuffer_format,
                RenderingDevice::INVALID_FORMAT_ID,
                RenderingDevice::RENDER_PRIMITIVE_TRIANGLE_STRIPS,
                raster_state,
                multisample_state,
                depth_state,
                blend_state);
            g_composite_resources->depth_framebuffer_format = framebuffer_format;
        }

        return g_composite_resources->depth_pipeline.is_valid();
    }

    RID create_intermediate_texture(RenderingDevice *rendering_device, const RID &color_texture, int width, int height)
    {
        if (rendering_device == nullptr || !color_texture.is_valid() || width <= 0 || height <= 0)
        {
            return RID();
        }

        Ref<RDTextureFormat> color_format = rendering_device->texture_get_format(color_texture);
        if (!color_format.is_valid())
        {
            return RID();
        }

        Ref<RDTextureFormat> texture_format;
        texture_format.instantiate();
        texture_format->set_format(color_format->get_format());
        texture_format->set_width(static_cast<uint32_t>(width));
        texture_format->set_height(static_cast<uint32_t>(height));
        texture_format->set_depth(1);
        texture_format->set_array_layers(1);
        texture_format->set_mipmaps(1);
        texture_format->set_texture_type(RenderingDevice::TEXTURE_TYPE_2D);
        texture_format->set_samples(RenderingDevice::TEXTURE_SAMPLES_1);
        texture_format->set_usage_bits(BitField<RenderingDevice::TextureUsageBits>(
            static_cast<int64_t>(RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT) |
            static_cast<int64_t>(RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) |
            static_cast<int64_t>(RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT) |
            static_cast<int64_t>(RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT)));

        Ref<RDTextureView> texture_view;
        texture_view.instantiate();
        return rendering_device->texture_create(texture_format, texture_view);
    }

    RID create_intermediate_depth_texture(RenderingDevice *rendering_device, int width, int height)
    {
        if (rendering_device == nullptr || width <= 0 || height <= 0)
        {
            return RID();
        }

        Ref<RDTextureFormat> texture_format;
        texture_format.instantiate();
        texture_format->set_format(RenderingDevice::DATA_FORMAT_D32_SFLOAT);
        texture_format->set_width(static_cast<uint32_t>(width));
        texture_format->set_height(static_cast<uint32_t>(height));
        texture_format->set_depth(1);
        texture_format->set_array_layers(1);
        texture_format->set_mipmaps(1);
        texture_format->set_texture_type(RenderingDevice::TEXTURE_TYPE_2D);
        texture_format->set_samples(RenderingDevice::TEXTURE_SAMPLES_1);
        texture_format->set_usage_bits(BitField<RenderingDevice::TextureUsageBits>(
            static_cast<int64_t>(RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT) |
            static_cast<int64_t>(RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) |
            static_cast<int64_t>(RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT) |
            static_cast<int64_t>(RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT)));

        Ref<RDTextureView> texture_view;
        texture_view.instantiate();
        return rendering_device->texture_create(texture_format, texture_view);
    }

    bool composite_texture_to_color(RenderingDevice *rendering_device, const RID &source_texture, const RID &color_texture)
    {
        if (rendering_device == nullptr || !source_texture.is_valid() || !color_texture.is_valid())
        {
            return false;
        }

        TypedArray<RID> framebuffer_textures;
        framebuffer_textures.push_back(color_texture);
        RID framebuffer = rendering_device->framebuffer_create(framebuffer_textures);
        const bool framebuffer_valid = rendering_device->framebuffer_is_valid(framebuffer);
        if (!framebuffer_valid)
        {
            if (framebuffer.is_valid())
            {
                rendering_device->free_rid(framebuffer);
            }
            return false;
        }

        const int64_t framebuffer_format = rendering_device->framebuffer_get_format(framebuffer);
        if (!ensure_composite_resources(rendering_device, framebuffer_format))
        {
            release_composite_resources(rendering_device);
            rendering_device->free_rid(framebuffer);
            return false;
        }

        Ref<RDUniform> source_uniform;
        source_uniform.instantiate();
        source_uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
        source_uniform->set_binding(0);
        source_uniform->add_id(g_composite_resources->sampler);
        source_uniform->add_id(source_texture);
        TypedArray<Ref<RDUniform>> uniforms;
        uniforms.push_back(source_uniform);
        RID uniform_set = rendering_device->uniform_set_create(uniforms, g_composite_resources->shader, 0);
        if (!uniform_set.is_valid())
        {
            release_composite_resources(rendering_device);
            rendering_device->free_rid(framebuffer);
            return false;
        }

        const BitField<RenderingDevice::DrawFlags> draw_flags(
            static_cast<int64_t>(RenderingDevice::DRAW_IGNORE_DEPTH) |
            static_cast<int64_t>(RenderingDevice::DRAW_IGNORE_STENCIL));
        const int64_t draw_list = rendering_device->draw_list_begin(framebuffer, draw_flags);
        if (draw_list == RenderingDevice::INVALID_ID)
        {
            rendering_device->free_rid(uniform_set);
            release_composite_resources(rendering_device);
            rendering_device->free_rid(framebuffer);
            return false;
        }

        rendering_device->draw_list_bind_render_pipeline(draw_list, g_composite_resources->pipeline);
        rendering_device->draw_list_bind_uniform_set(draw_list, uniform_set, 0);
        rendering_device->draw_list_draw(draw_list, false, 1, 4);
        rendering_device->draw_list_end();

        rendering_device->free_rid(uniform_set);
        release_composite_resources(rendering_device);
        rendering_device->free_rid(framebuffer);
        return true;
    }

    bool composite_texture_to_color_with_depth(RenderingDevice *rendering_device, const RID &source_texture, const RID &source_depth_texture, const RID &scene_depth_texture, const RID &color_texture)
    {
        if (rendering_device == nullptr || !source_texture.is_valid() || !source_depth_texture.is_valid() || !scene_depth_texture.is_valid() || !color_texture.is_valid())
        {
            return false;
        }

        TypedArray<RID> framebuffer_textures;
        framebuffer_textures.push_back(color_texture);
        RID framebuffer = rendering_device->framebuffer_create(framebuffer_textures);
        const bool framebuffer_valid = rendering_device->framebuffer_is_valid(framebuffer);
        if (!framebuffer_valid)
        {
            if (framebuffer.is_valid())
            {
                rendering_device->free_rid(framebuffer);
            }
            return false;
        }

        const int64_t framebuffer_format = rendering_device->framebuffer_get_format(framebuffer);
        if (!ensure_depth_composite_resources(rendering_device, framebuffer_format))
        {
            release_composite_resources(rendering_device);
            rendering_device->free_rid(framebuffer);
            return false;
        }

        TypedArray<Ref<RDUniform>> uniforms;
        Ref<RDUniform> source_color_uniform;
        source_color_uniform.instantiate();
        source_color_uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
        source_color_uniform->set_binding(0);
        source_color_uniform->add_id(g_composite_resources->sampler);
        source_color_uniform->add_id(source_texture);
        uniforms.push_back(source_color_uniform);

        Ref<RDUniform> source_depth_uniform;
        source_depth_uniform.instantiate();
        source_depth_uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
        source_depth_uniform->set_binding(1);
        source_depth_uniform->add_id(g_composite_resources->sampler);
        source_depth_uniform->add_id(source_depth_texture);
        uniforms.push_back(source_depth_uniform);

        Ref<RDUniform> scene_depth_uniform;
        scene_depth_uniform.instantiate();
        scene_depth_uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
        scene_depth_uniform->set_binding(2);
        scene_depth_uniform->add_id(g_composite_resources->sampler);
        scene_depth_uniform->add_id(scene_depth_texture);
        uniforms.push_back(scene_depth_uniform);

        RID uniform_set = rendering_device->uniform_set_create(uniforms, g_composite_resources->depth_shader, 0);
        if (!uniform_set.is_valid())
        {
            release_composite_resources(rendering_device);
            rendering_device->free_rid(framebuffer);
            return false;
        }

        const BitField<RenderingDevice::DrawFlags> draw_flags(
            static_cast<int64_t>(RenderingDevice::DRAW_IGNORE_DEPTH) |
            static_cast<int64_t>(RenderingDevice::DRAW_IGNORE_STENCIL));
        const int64_t draw_list = rendering_device->draw_list_begin(framebuffer, draw_flags);
        if (draw_list == RenderingDevice::INVALID_ID)
        {
            rendering_device->free_rid(uniform_set);
            release_composite_resources(rendering_device);
            rendering_device->free_rid(framebuffer);
            return false;
        }

        rendering_device->draw_list_bind_render_pipeline(draw_list, g_composite_resources->depth_pipeline);
        rendering_device->draw_list_bind_uniform_set(draw_list, uniform_set, 0);
        rendering_device->draw_list_draw(draw_list, false, 1, 4);
        rendering_device->draw_list_end();

        rendering_device->free_rid(uniform_set);
        release_composite_resources(rendering_device);
        rendering_device->free_rid(framebuffer);
        return true;
    }
}

ImmViewerCompositorEffect::ImmViewerCompositorEffect()
{
    set_enabled(true);
    set_effect_callback_type(CompositorEffect::EFFECT_CALLBACK_TYPE_POST_TRANSPARENT);
    set_access_resolved_color(false);
}

ImmViewerCompositorEffect::~ImmViewerCompositorEffect()
{
    RenderingServer *rendering_server = RenderingServer::get_singleton();
    RenderingDevice *rendering_device = rendering_server != nullptr ? rendering_server->get_rendering_device() : nullptr;
    if (rendering_device != nullptr && _intermediate_texture.is_valid())
    {
        rendering_device->free_rid(_intermediate_texture);
    }
    if (rendering_device != nullptr && _intermediate_depth_texture.is_valid())
    {
        rendering_device->free_rid(_intermediate_depth_texture);
    }
}

void ImmViewerCompositorEffect::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_diagnostics"), &ImmViewerCompositorEffect::get_diagnostics);
}

void ImmViewerCompositorEffect::queue_render_request(int camera_id, int width, int height)
{
    std::lock_guard<std::mutex> lock(g_queued_render_mutex);
    g_queued_render_request.queued = true;
    g_queued_render_request.camera_id = camera_id;
    g_queued_render_request.width = width;
    g_queued_render_request.height = height;
}

RID ImmViewerCompositorEffect::ensure_intermediate_texture(RenderingDevice *rendering_device, const RID &color_texture, int width, int height)
{
    if (rendering_device == nullptr || !color_texture.is_valid() || width <= 0 || height <= 0)
    {
        return RID();
    }

    Ref<RDTextureFormat> color_format = rendering_device->texture_get_format(color_texture);
    if (!color_format.is_valid())
    {
        return RID();
    }

    const int64_t format = static_cast<int64_t>(color_format->get_format());
    const Vector2i size(width, height);
    if (_intermediate_texture.is_valid() && _intermediate_size == size && _intermediate_format == format)
    {
        return _intermediate_texture;
    }

    if (_intermediate_texture.is_valid())
    {
        rendering_device->free_rid(_intermediate_texture);
        _intermediate_texture = RID();
    }

    _intermediate_texture = create_intermediate_texture(rendering_device, color_texture, width, height);
    _intermediate_size = _intermediate_texture.is_valid() ? size : Vector2i();
    _intermediate_format = _intermediate_texture.is_valid() ? format : -1;
    return _intermediate_texture;
}

RID ImmViewerCompositorEffect::ensure_intermediate_depth_texture(RenderingDevice *rendering_device, int width, int height)
{
    if (rendering_device == nullptr || width <= 0 || height <= 0)
    {
        return RID();
    }

    const int64_t format = static_cast<int64_t>(RenderingDevice::DATA_FORMAT_D32_SFLOAT);
    const Vector2i size(width, height);
    if (_intermediate_depth_texture.is_valid() && _intermediate_depth_size == size && _intermediate_depth_format == format)
    {
        return _intermediate_depth_texture;
    }

    if (_intermediate_depth_texture.is_valid())
    {
        rendering_device->free_rid(_intermediate_depth_texture);
        _intermediate_depth_texture = RID();
    }

    _intermediate_depth_texture = create_intermediate_depth_texture(rendering_device, width, height);
    _intermediate_depth_size = _intermediate_depth_texture.is_valid() ? size : Vector2i();
    _intermediate_depth_format = _intermediate_depth_texture.is_valid() ? format : -1;
    return _intermediate_depth_texture;
}

void ImmViewerCompositorEffect::_render_callback(int32_t effect_callback_type, RenderData *render_data)
{
    RenderingServer *rendering_server = RenderingServer::get_singleton();
    RenderingDevice *rendering_device = rendering_server != nullptr ? rendering_server->get_rendering_device() : nullptr;

    Ref<RenderSceneBuffers> scene_buffers;
    if (render_data != nullptr)
    {
        scene_buffers = render_data->get_render_scene_buffers();
    }

    RenderSceneBuffersRD *rd_scene_buffers = scene_buffers.is_valid() ? Object::cast_to<RenderSceneBuffersRD>(scene_buffers.ptr()) : nullptr;
    RID color_texture;
    RID depth_texture;
    Vector2i internal_size;
    Vector2i target_size;
    int view_count = 0;
    uint64_t command_queue_handle = 0;
    uint64_t color_texture_handle = 0;
    uint64_t vulkan_instance_handle = 0;
    uint64_t vulkan_physical_device_handle = 0;
    uint64_t vulkan_device_handle = 0;
    uint64_t vulkan_queue_handle = 0;
    uint64_t vulkan_queue_family_index = 0;
    uint64_t vulkan_image_handle = 0;
    uint64_t vulkan_image_view_handle = 0;
    uint32_t vulkan_image_format = 0;
    uint64_t vulkan_depth_image_handle = 0;
    uint64_t vulkan_depth_image_view_handle = 0;
    uint32_t vulkan_depth_image_format = 0;
    uint64_t intermediate_depth_image_handle = 0;
    uint64_t intermediate_depth_image_view_handle = 0;
    uint32_t intermediate_depth_image_format = 0;
    const bool rd_clear_test = std::getenv("IMM_GODOT_RD_CLEAR_TEST") != nullptr;
    Error rd_clear_result = OK;
    bool rd_framebuffer_valid = false;
    int64_t rd_clear_draw_list = RenderingDevice::INVALID_ID;
    QueuedRenderRequest render_request;

    if (rd_scene_buffers != nullptr)
    {
        color_texture = rd_scene_buffers->get_color_layer(0, false);
        if (!color_texture.is_valid())
        {
            color_texture = rd_scene_buffers->get_color_texture(false);
        }
        if (!color_texture.is_valid())
        {
            color_texture = rd_scene_buffers->get_texture(StringName("render_buffers"), StringName("color"));
        }
        depth_texture = rd_scene_buffers->get_depth_layer(0, false);
        if (!depth_texture.is_valid())
        {
            depth_texture = rd_scene_buffers->get_depth_texture(false);
        }
        if (!depth_texture.is_valid())
        {
            depth_texture = rd_scene_buffers->get_texture(StringName("render_buffers"), StringName("depth"));
        }
        internal_size = rd_scene_buffers->get_internal_size();
        target_size = rd_scene_buffers->get_target_size();
        view_count = static_cast<int>(rd_scene_buffers->get_view_count());

        if (rendering_device != nullptr)
        {
            command_queue_handle = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_COMMAND_QUEUE, RID(), 0);
            vulkan_instance_handle = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_INSTANCE, RID(), 0);
            vulkan_physical_device_handle = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_PHYSICAL_DEVICE, RID(), 0);
            vulkan_device_handle = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_DEVICE, RID(), 0);
            vulkan_queue_handle = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_QUEUE, RID(), 0);
            vulkan_queue_family_index = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_QUEUE_FAMILY_INDEX, RID(), 0);
            if (color_texture.is_valid())
            {
                color_texture_handle = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TEXTURE, color_texture, 0);
                vulkan_image_handle = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE, color_texture, 0);
                vulkan_image_view_handle = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE_VIEW, color_texture, 0);
                vulkan_image_format = static_cast<uint32_t>(rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE_NATIVE_TEXTURE_FORMAT, color_texture, 0));
            }
            if (depth_texture.is_valid())
            {
                vulkan_depth_image_handle = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE, depth_texture, 0);
                vulkan_depth_image_view_handle = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE_VIEW, depth_texture, 0);
                vulkan_depth_image_format = static_cast<uint32_t>(rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE_NATIVE_TEXTURE_FORMAT, depth_texture, 0));
            }
        }
    }

    if (rd_clear_test && rendering_device != nullptr && color_texture.is_valid())
    {
        TypedArray<RID> framebuffer_textures;
        framebuffer_textures.push_back(color_texture);
        RID framebuffer = rendering_device->framebuffer_create(framebuffer_textures);
        rd_framebuffer_valid = rendering_device->framebuffer_is_valid(framebuffer);
        if (rd_framebuffer_valid)
        {
            PackedColorArray clear_colors;
            clear_colors.push_back(Color(1.0f, 0.0f, 0.0f, 1.0f));
            const BitField<RenderingDevice::DrawFlags> draw_flags(
                static_cast<int64_t>(RenderingDevice::DRAW_CLEAR_COLOR_0) |
                static_cast<int64_t>(RenderingDevice::DRAW_IGNORE_DEPTH) |
                static_cast<int64_t>(RenderingDevice::DRAW_IGNORE_STENCIL));
            rd_clear_draw_list = rendering_device->draw_list_begin(framebuffer, draw_flags, clear_colors);
            if (rd_clear_draw_list != RenderingDevice::INVALID_ID)
            {
                rendering_device->draw_list_end();
            }
        }
        else
        {
            rd_clear_result = ERR_CANT_CREATE;
        }
        if (framebuffer.is_valid())
        {
            rendering_device->free_rid(framebuffer);
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_queued_render_mutex);
        render_request = g_queued_render_request;
    }

    bool metal_frame_started = false;
    bool vulkan_frame_started = false;
    bool composite_result = false;
    bool direct_vulkan_color_target = false;
    bool had_intermediate_texture = false;
    bool had_intermediate_depth_texture = false;
    bool depth_aware_vulkan_composite = false;
    bool depth_aware_vulkan_composite_result = false;
    int intermediate_nonzero_bytes = -1;
    int intermediate_total_bytes = 0;
    int render_result = 0;
    if (render_request.queued && command_queue_handle != 0 && color_texture_handle != 0 && target_size.x > 0 && target_size.y > 0)
    {
        const int render_width = render_request.width > 0 ? render_request.width : target_size.x;
        const int render_height = render_request.height > 0 ? render_request.height : target_size.y;
        const bool direct_vulkan_depth_composition_enabled = std::getenv("IMM_GODOT_DIRECT_VULKAN_DEPTH_COMPOSITION") != nullptr;
        const bool render_graph_depth_composition_enabled = std::getenv("IMM_GODOT_RENDER_GRAPH_VULKAN_DEPTH_COMPOSITION") != nullptr;
        const bool can_direct_vulkan_color_target = direct_vulkan_depth_composition_enabled &&
                                                   !render_graph_depth_composition_enabled &&
                                                   vulkan_instance_handle != 0 &&
                                                   vulkan_physical_device_handle != 0 &&
                                                   vulkan_device_handle != 0 &&
                                                   vulkan_queue_handle != 0 &&
                                                   color_texture_handle != 0 &&
                                                   vulkan_image_handle != 0 &&
                                                   vulkan_image_view_handle != 0 &&
                                                   vulkan_image_format != 0 &&
                                                   vulkan_depth_image_handle != 0 &&
                                                   vulkan_depth_image_view_handle != 0 &&
                                                   vulkan_depth_image_format != 0;
        const bool direct_vulkan_color_path = can_direct_vulkan_color_target;
        RID intermediate_texture;
        RID intermediate_depth_texture;
        if (!direct_vulkan_color_path)
        {
            intermediate_texture = ensure_intermediate_texture(rendering_device, color_texture, render_width, render_height);
            if (render_graph_depth_composition_enabled && depth_texture.is_valid())
            {
                intermediate_depth_texture = ensure_intermediate_depth_texture(rendering_device, render_width, render_height);
            }
        }
        had_intermediate_texture = intermediate_texture.is_valid();
        had_intermediate_depth_texture = intermediate_depth_texture.is_valid();
        const bool use_intermediate_depth = had_intermediate_texture && had_intermediate_depth_texture && depth_texture.is_valid();
        uint64_t render_texture_handle = direct_vulkan_color_path
                                             ? color_texture_handle
                                             : (had_intermediate_texture
                                             ? rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TEXTURE, intermediate_texture, 0)
                                             : 0);
        uint64_t render_texture_vulkan_image_handle = direct_vulkan_color_path
                                                          ? vulkan_image_handle
                                                          : (had_intermediate_texture
                                                          ? rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE, intermediate_texture, 0)
                                                          : 0);
        uint64_t render_texture_view_handle = direct_vulkan_color_path
                                                  ? vulkan_image_view_handle
                                                  : (had_intermediate_texture
                                                  ? rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE_VIEW, intermediate_texture, 0)
                                                  : 0);
        const uint32_t render_texture_format = direct_vulkan_color_path
                                                   ? vulkan_image_format
                                                   : (had_intermediate_texture
                                                   ? static_cast<uint32_t>(rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE_NATIVE_TEXTURE_FORMAT, intermediate_texture, 0))
                                                   : 0);
        const uint64_t render_depth_vulkan_image_handle = use_intermediate_depth
                                                              ? rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE, intermediate_depth_texture, 0)
                                                              : 0;
        const uint64_t render_depth_texture_view_handle = use_intermediate_depth
                                                              ? rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE_VIEW, intermediate_depth_texture, 0)
                                                              : 0;
        const uint32_t render_depth_texture_format = use_intermediate_depth
                                                         ? static_cast<uint32_t>(rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE_NATIVE_TEXTURE_FORMAT, intermediate_depth_texture, 0))
                                                         : 0;
        intermediate_depth_image_handle = render_depth_vulkan_image_handle;
        intermediate_depth_image_view_handle = render_depth_texture_view_handle;
        intermediate_depth_image_format = render_depth_texture_format;
        if (vulkan_instance_handle != 0 && vulkan_physical_device_handle != 0 && vulkan_device_handle != 0 && vulkan_queue_handle != 0)
        {
            vulkan_frame_started = ImmViewerGodotBeginVulkanTextureFrame(vulkan_instance_handle,
                                                                         vulkan_physical_device_handle,
                                                                         vulkan_device_handle,
                                                                         vulkan_queue_handle,
                                                                         vulkan_queue_family_index,
                                                                         render_texture_vulkan_image_handle,
                                                                         render_texture_view_handle,
                                                                         render_texture_format,
                                                                         can_direct_vulkan_color_target ? vulkan_depth_image_handle : render_depth_vulkan_image_handle,
                                                                         can_direct_vulkan_color_target ? vulkan_depth_image_view_handle : render_depth_texture_view_handle,
                                                                         can_direct_vulkan_color_target ? vulkan_depth_image_format : render_depth_texture_format,
                                                                         render_width,
                                                                         render_height,
                                                                         use_intermediate_depth);
            direct_vulkan_color_target = vulkan_frame_started && can_direct_vulkan_color_target;
        }
        if (!vulkan_frame_started)
        {
            metal_frame_started = ImmViewerGodotBeginMetalTextureFrame(command_queue_handle,
                                                                       render_texture_handle,
                                                                       render_width,
                                                                       render_height);
        }
        if (vulkan_frame_started || metal_frame_started)
        {
            render_result = ImmGodot_RenderCamera(render_request.camera_id,
                                                  0,
                                                  0.0f,
                                                  0.0f,
                                                  static_cast<float>(render_width),
                                                  static_cast<float>(render_height),
                                                  0.0f,
                                                  1.0f);
            if (vulkan_frame_started)
            {
                ImmViewerGodotEndVulkanTextureFrame();
            }
            else
            {
                ImmViewerGodotEndMetalTextureFrame();
            }
            if (std::getenv("IMM_GODOT_TRACE_INTERMEDIATE_TEXTURE") != nullptr && intermediate_texture.is_valid())
            {
                PackedByteArray intermediate_data = rendering_device->texture_get_data(intermediate_texture, 0);
                intermediate_total_bytes = intermediate_data.size();
                intermediate_nonzero_bytes = 0;
                for (int i = 0; i < intermediate_total_bytes; ++i)
                {
                    if (intermediate_data[i] != 0)
                    {
                        ++intermediate_nonzero_bytes;
                    }
                }
            }
            if (use_intermediate_depth)
            {
                depth_aware_vulkan_composite = true;
                depth_aware_vulkan_composite_result = composite_texture_to_color_with_depth(rendering_device, intermediate_texture, intermediate_depth_texture, depth_texture, color_texture);
                composite_result = depth_aware_vulkan_composite_result;
            }
            else
            {
                composite_result = direct_vulkan_color_target || composite_texture_to_color(rendering_device, intermediate_texture, color_texture);
            }
        }
        else
        {
            render_result = -1;
        }
    }

    {
        std::lock_guard<std::mutex> lock(_diagnostics_mutex);
        _callback_count++;
        _last_callback_type = effect_callback_type;
        _last_had_render_data = render_data != nullptr;
        _last_had_scene_buffers = scene_buffers.is_valid();
        _last_had_rd_scene_buffers = rd_scene_buffers != nullptr;
        _last_had_rendering_device = rendering_device != nullptr;
        _last_had_color_texture = color_texture.is_valid();
        _last_rd_clear_test = rd_clear_test;
        _last_rd_clear_result = static_cast<int>(rd_clear_result);
        _last_rd_framebuffer_valid = rd_framebuffer_valid;
        _last_rd_clear_draw_list = rd_clear_draw_list;
        _last_had_queued_render = render_request.queued;
        _last_metal_frame_started = metal_frame_started;
        _last_vulkan_frame_started = vulkan_frame_started;
        _ever_vulkan_frame_started = _ever_vulkan_frame_started || vulkan_frame_started;
        _last_composite_result = composite_result;
        _last_direct_vulkan_color_target = direct_vulkan_color_target;
        _last_had_intermediate_texture = had_intermediate_texture;
        _last_had_intermediate_depth_texture = had_intermediate_depth_texture;
        _last_depth_aware_vulkan_composite = depth_aware_vulkan_composite;
        _last_depth_aware_vulkan_composite_result = depth_aware_vulkan_composite_result;
        _last_intermediate_nonzero_bytes = intermediate_nonzero_bytes;
        _last_intermediate_total_bytes = intermediate_total_bytes;
        _last_render_result = render_result;
        _last_render_camera_id = render_request.queued ? render_request.camera_id : -1;
        _last_render_width = render_request.queued ? render_request.width : 0;
        _last_render_height = render_request.queued ? render_request.height : 0;
        _last_command_queue_handle = command_queue_handle;
        _last_color_texture_handle = color_texture_handle;
        _last_vulkan_instance_handle = vulkan_instance_handle;
        _last_vulkan_physical_device_handle = vulkan_physical_device_handle;
        _last_vulkan_device_handle = vulkan_device_handle;
        _last_vulkan_queue_handle = vulkan_queue_handle;
        _last_vulkan_queue_family_index = vulkan_queue_family_index;
        _last_vulkan_image_handle = vulkan_image_handle;
        _last_vulkan_image_view_handle = vulkan_image_view_handle;
        _last_vulkan_image_format = vulkan_image_format;
        _last_vulkan_depth_image_handle = vulkan_depth_image_handle;
        _last_vulkan_depth_image_view_handle = vulkan_depth_image_view_handle;
        _last_vulkan_depth_image_format = vulkan_depth_image_format;
        _last_intermediate_depth_image_handle = intermediate_depth_image_handle;
        _last_intermediate_depth_image_view_handle = intermediate_depth_image_view_handle;
        _last_intermediate_depth_image_format = intermediate_depth_image_format;
        _last_color_texture = color_texture;
        _last_internal_size = internal_size;
        _last_target_size = target_size;
        _last_view_count = view_count;
    }
}

Dictionary ImmViewerCompositorEffect::get_diagnostics() const
{
    std::lock_guard<std::mutex> lock(_diagnostics_mutex);

    Dictionary result;
    result["callback_count"] = _callback_count;
    result["last_callback_type"] = _last_callback_type;
    result["last_had_render_data"] = _last_had_render_data;
    result["last_had_scene_buffers"] = _last_had_scene_buffers;
    result["last_had_rd_scene_buffers"] = _last_had_rd_scene_buffers;
    result["last_had_rendering_device"] = _last_had_rendering_device;
    result["last_had_color_texture"] = _last_had_color_texture;
    result["last_rd_clear_test"] = _last_rd_clear_test;
    result["last_rd_clear_result"] = _last_rd_clear_result;
    result["last_rd_framebuffer_valid"] = _last_rd_framebuffer_valid;
    result["last_rd_clear_draw_list"] = _last_rd_clear_draw_list;
    result["last_had_queued_render"] = _last_had_queued_render;
    result["last_metal_frame_started"] = _last_metal_frame_started;
    result["last_vulkan_frame_started"] = _last_vulkan_frame_started;
    result["ever_vulkan_frame_started"] = _ever_vulkan_frame_started;
    result["last_composite_result"] = _last_composite_result;
    result["last_direct_vulkan_color_target"] = _last_direct_vulkan_color_target;
    result["last_had_intermediate_texture"] = _last_had_intermediate_texture;
    result["last_had_intermediate_depth_texture"] = _last_had_intermediate_depth_texture;
    result["last_depth_aware_vulkan_composite"] = _last_depth_aware_vulkan_composite;
    result["last_depth_aware_vulkan_composite_result"] = _last_depth_aware_vulkan_composite_result;
    result["last_intermediate_nonzero_bytes"] = _last_intermediate_nonzero_bytes;
    result["last_intermediate_total_bytes"] = _last_intermediate_total_bytes;
    result["last_render_result"] = _last_render_result;
    result["last_render_camera_id"] = _last_render_camera_id;
    result["last_render_width"] = _last_render_width;
    result["last_render_height"] = _last_render_height;
    result["last_command_queue_handle"] = _last_command_queue_handle;
    result["last_color_texture_handle"] = _last_color_texture_handle;
    result["last_vulkan_instance_handle"] = _last_vulkan_instance_handle;
    result["last_vulkan_physical_device_handle"] = _last_vulkan_physical_device_handle;
    result["last_vulkan_device_handle"] = _last_vulkan_device_handle;
    result["last_vulkan_queue_handle"] = _last_vulkan_queue_handle;
    result["last_vulkan_queue_family_index"] = _last_vulkan_queue_family_index;
    result["last_vulkan_image_handle"] = _last_vulkan_image_handle;
    result["last_vulkan_image_view_handle"] = _last_vulkan_image_view_handle;
    result["last_vulkan_image_format"] = _last_vulkan_image_format;
    result["last_vulkan_depth_image_handle"] = _last_vulkan_depth_image_handle;
    result["last_vulkan_depth_image_view_handle"] = _last_vulkan_depth_image_view_handle;
    result["last_vulkan_depth_image_format"] = _last_vulkan_depth_image_format;
    result["last_intermediate_depth_image_handle"] = _last_intermediate_depth_image_handle;
    result["last_intermediate_depth_image_view_handle"] = _last_intermediate_depth_image_view_handle;
    result["last_intermediate_depth_image_format"] = _last_intermediate_depth_image_format;
    result["last_color_texture"] = _last_color_texture;
    result["last_internal_size"] = _last_internal_size;
    result["last_target_size"] = _last_target_size;
    result["last_view_count"] = _last_view_count;
    return result;
}
