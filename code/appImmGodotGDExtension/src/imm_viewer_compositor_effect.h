#pragma once

#include <godot_cpp/classes/render_data.hpp>
#include <godot_cpp/classes/compositor_effect.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <array>
#include <atomic>
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
        void set_render_graph_depth_composition_enabled(bool enabled);
        bool is_render_graph_depth_composition_enabled() const;
        void set_stereo_simulation_eye(int eye_index);
        int get_stereo_simulation_eye() const;
        bool set_stereo_replay_matrices(const PackedFloat32Array &world_to_head,
                                        const PackedFloat32Array &head_projection,
                                        const PackedFloat32Array &world_to_left_eye,
                                        const PackedFloat32Array &left_eye_projection,
                                        const PackedFloat32Array &world_to_right_eye,
                                        const PackedFloat32Array &right_eye_projection);
        void clear_stereo_replay_matrices();
        bool has_stereo_replay_matrices() const;
        Dictionary get_last_xr_frame_capture() const;
        Dictionary get_diagnostics() const;

    private:
        RID ensure_intermediate_texture(RenderingDevice *rendering_device, const RID &color_texture, int width, int height, int view_index);
        RID ensure_intermediate_depth_texture(RenderingDevice *rendering_device, int width, int height, int view_index);
        RID ensure_depth_composited_texture(RenderingDevice *rendering_device, const RID &color_texture, int width, int height, int view_index);

        mutable std::mutex _diagnostics_mutex;
        std::array<RID, 2> _intermediate_textures;
        std::array<RID, 2> _intermediate_depth_textures;
        std::array<RID, 2> _depth_composited_textures;
        std::array<Vector2i, 2> _intermediate_sizes;
        std::array<Vector2i, 2> _intermediate_depth_sizes;
        std::array<Vector2i, 2> _depth_composited_sizes;
        std::array<int64_t, 2> _intermediate_formats = {-1, -1};
        std::array<int64_t, 2> _intermediate_depth_formats = {-1, -1};
        std::array<int64_t, 2> _depth_composited_formats = {-1, -1};
        bool _render_graph_depth_composition_enabled = false;
        std::atomic<int> _stereo_simulation_eye = -1;
        mutable std::mutex _stereo_replay_mutex;
        bool _has_stereo_replay_matrices = false;
        std::array<float, 16> _replay_world_to_head{};
        std::array<float, 16> _replay_head_projection{};
        std::array<float, 16> _replay_world_to_left_eye{};
        std::array<float, 16> _replay_left_eye_projection{};
        std::array<float, 16> _replay_world_to_right_eye{};
        std::array<float, 16> _replay_right_eye_projection{};
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
        bool _last_direct_vulkan_color_target = false;
        bool _last_had_intermediate_texture = false;
        bool _last_had_intermediate_depth_texture = false;
        bool _last_had_depth_composited_texture = false;
        bool _last_depth_color_merge_result = false;
        bool _last_depth_aware_vulkan_composite = false;
        bool _last_depth_aware_vulkan_composite_result = false;
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
        uint64_t _last_vulkan_image_handle = 0;
        uint64_t _last_vulkan_image_view_handle = 0;
        uint32_t _last_vulkan_image_format = 0;
        uint64_t _last_vulkan_depth_image_handle = 0;
        uint64_t _last_vulkan_depth_image_view_handle = 0;
        uint32_t _last_vulkan_depth_image_format = 0;
        uint64_t _last_intermediate_depth_image_handle = 0;
        uint64_t _last_intermediate_depth_image_view_handle = 0;
        uint32_t _last_intermediate_depth_image_format = 0;
        RID _last_color_texture;
        Vector2i _last_internal_size;
        Vector2i _last_target_size;
        int _last_view_count = 0;
        bool _last_used_xr_render_data = false;
        bool _last_used_stereo_simulation = false;
        bool _last_used_stereo_replay = false;
        int _last_simulated_eye_index = -1;
        bool _last_submitted_stereo_matrices = false;
        int _last_rendered_eye_mask = 0;
        int _last_rendered_eye_count = 0;
        bool _last_xr_frame_capture_available = false;
        std::array<float, 16> _last_xr_head_transform{};
        std::array<float, 16> _last_xr_raw_head_projection{};
        std::array<float, 16> _last_xr_raw_left_eye_projection{};
        std::array<float, 16> _last_xr_raw_right_eye_projection{};
        std::array<float, 3> _last_xr_left_eye_offset{};
        std::array<float, 3> _last_xr_right_eye_offset{};
        std::array<float, 16> _last_xr_world_to_head{};
        std::array<float, 16> _last_xr_head_projection{};
        std::array<float, 16> _last_xr_world_to_left_eye{};
        std::array<float, 16> _last_xr_left_eye_projection{};
        std::array<float, 16> _last_xr_world_to_right_eye{};
        std::array<float, 16> _last_xr_right_eye_projection{};
    };
}
