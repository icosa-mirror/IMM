#pragma once

#include <godot_cpp/classes/render_data.hpp>
#include <godot_cpp/classes/compositor_effect.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <mutex>

namespace godot
{
    class RenderData;
    class RenderingDevice;

    class ImmViewerCompositorEffect : public CompositorEffect
    {
        GDCLASS(ImmViewerCompositorEffect, CompositorEffect)

    public:
        ImmViewerCompositorEffect();
        ~ImmViewerCompositorEffect() override;

        static void _bind_methods();
        static void queue_render_request(int camera_id, int width, int height);

        void _render_callback(int32_t effect_callback_type, RenderData *render_data) override;
        Dictionary get_diagnostics() const;

    private:
        RID ensure_intermediate_texture(RenderingDevice *rendering_device, const RID &color_texture, int width, int height);

        mutable std::mutex _diagnostics_mutex;
        RID _intermediate_texture;
        Vector2i _intermediate_size;
        int64_t _intermediate_format = -1;
        int _callback_count = 0;
        int _last_callback_type = -1;
        bool _last_had_render_data = false;
        bool _last_had_scene_buffers = false;
        bool _last_had_rd_scene_buffers = false;
        bool _last_had_rendering_device = false;
        bool _last_had_color_texture = false;
        bool _last_rd_clear_test = false;
        int _last_rd_clear_result = 0;
        bool _last_rd_framebuffer_valid = false;
        int64_t _last_rd_clear_draw_list = -1;
        bool _last_had_queued_render = false;
        bool _last_metal_frame_started = false;
        bool _last_vulkan_frame_started = false;
        bool _ever_vulkan_frame_started = false;
        bool _last_composite_result = false;
        bool _last_had_intermediate_texture = false;
        int _last_intermediate_nonzero_bytes = -1;
        int _last_intermediate_total_bytes = 0;
        int _last_render_result = 0;
        int _last_render_camera_id = -1;
        int _last_render_width = 0;
        int _last_render_height = 0;
        uint64_t _last_command_queue_handle = 0;
        uint64_t _last_color_texture_handle = 0;
        uint64_t _last_vulkan_instance_handle = 0;
        uint64_t _last_vulkan_physical_device_handle = 0;
        uint64_t _last_vulkan_device_handle = 0;
        uint64_t _last_vulkan_queue_handle = 0;
        uint64_t _last_vulkan_queue_family_index = 0;
        uint64_t _last_vulkan_image_view_handle = 0;
        uint32_t _last_vulkan_image_format = 0;
        RID _last_color_texture;
        Vector2i _last_internal_size;
        Vector2i _last_target_size;
        int _last_view_count = 0;
    };
}
