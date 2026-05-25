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

    class ImmViewerCompositorEffect : public CompositorEffect
    {
        GDCLASS(ImmViewerCompositorEffect, CompositorEffect)

    public:
        ImmViewerCompositorEffect();
        ~ImmViewerCompositorEffect() override;

        static void _bind_methods();

        void _render_callback(int32_t effect_callback_type, RenderData *render_data) override;
        Dictionary get_diagnostics() const;

    private:
        mutable std::mutex _diagnostics_mutex;
        int _callback_count = 0;
        int _last_callback_type = -1;
        bool _last_had_render_data = false;
        bool _last_had_scene_buffers = false;
        bool _last_had_rd_scene_buffers = false;
        bool _last_had_rendering_device = false;
        bool _last_had_color_texture = false;
        uint64_t _last_command_queue_handle = 0;
        uint64_t _last_color_texture_handle = 0;
        RID _last_color_texture;
        Vector2i _last_internal_size;
        Vector2i _last_target_size;
        int _last_view_count = 0;
    };
}
