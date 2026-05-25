#include "imm_viewer_compositor_effect.h"

#include <godot_cpp/classes/render_data.hpp>
#include <godot_cpp/classes/render_scene_buffers.hpp>
#include <godot_cpp/classes/render_scene_buffers_rd.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

ImmViewerCompositorEffect::ImmViewerCompositorEffect()
{
    set_enabled(true);
    set_effect_callback_type(CompositorEffect::EFFECT_CALLBACK_TYPE_POST_TRANSPARENT);
}

ImmViewerCompositorEffect::~ImmViewerCompositorEffect() = default;

void ImmViewerCompositorEffect::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_diagnostics"), &ImmViewerCompositorEffect::get_diagnostics);
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
    Vector2i internal_size;
    Vector2i target_size;
    int view_count = 0;
    uint64_t command_queue_handle = 0;
    uint64_t color_texture_handle = 0;

    if (rd_scene_buffers != nullptr)
    {
        color_texture = rd_scene_buffers->get_color_texture(false);
        internal_size = rd_scene_buffers->get_internal_size();
        target_size = rd_scene_buffers->get_target_size();
        view_count = static_cast<int>(rd_scene_buffers->get_view_count());

        if (rendering_device != nullptr)
        {
            command_queue_handle = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_COMMAND_QUEUE, RID(), 0);
            if (color_texture.is_valid())
            {
                color_texture_handle = rendering_device->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TEXTURE, color_texture, 0);
            }
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
        _last_command_queue_handle = command_queue_handle;
        _last_color_texture_handle = color_texture_handle;
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
    result["last_command_queue_handle"] = _last_command_queue_handle;
    result["last_color_texture_handle"] = _last_color_texture_handle;
    result["last_color_texture"] = _last_color_texture;
    result["last_internal_size"] = _last_internal_size;
    result["last_target_size"] = _last_target_size;
    result["last_view_count"] = _last_view_count;
    return result;
}
