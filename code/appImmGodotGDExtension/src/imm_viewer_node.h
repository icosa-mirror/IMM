#pragma once

#include "appImmGodot/src/imm_godot_plugin.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace godot
{
    class ImmViewerNode : public Node3D
    {
        GDCLASS(ImmViewerNode, Node3D)

    public:
        ImmViewerNode();
        ~ImmViewerNode() override;

        static void _bind_methods();

        void _ready() override;
        void _exit_tree() override;
        void _process(double delta) override;

        void set_document_path(const String &path);
        String get_document_path() const;

        void set_load_on_ready(bool enabled);
        bool get_load_on_ready() const;

        void set_auto_play(bool enabled);
        bool get_auto_play() const;

        void set_volume(float value);
        float get_volume() const;
        void set_matrix_debug_logging(bool enabled);
        bool get_matrix_debug_logging() const;

        int load_document(const String &path = String());
        void unload_document();
        void play();
        void pause();
        void restart();
        void next_chapter();
        void previous_chapter();
        void next_spawn_area();
        void previous_spawn_area();
        void global_work(bool enabled = true);
        void set_document_transform(const Transform3D &document_transform);
        void set_camera_transform(const Transform3D &camera_transform);
        bool set_camera_matrices(int camera_id, const PackedFloat32Array &world_to_head, const PackedFloat32Array &projection);
        int render_camera(int camera_id, const Vector2i &viewport_size, int eye_id = 0);

        bool is_loaded() const;
        bool is_playing() const;
        Dictionary get_document_state() const;
        Dictionary get_bounding_box() const;
        Color get_background_color() const;
        PackedInt32Array get_spawn_area_ids();
        int get_active_spawn_area_index() const;
        int get_active_spawn_area_id() const;
        bool set_active_spawn_area_index(int active_index);
        Dictionary get_spawn_area_info(int spawn_area_id) const;
        Dictionary get_render_diagnostics() const;

    private:
        void refresh_spawn_areas();
        String resolve_load_path(const String &path) const;
        void register_render_adapter();
        void unregister_render_adapter();
        void apply_document_transform();
        void submit_camera_matrices(int camera_id, const float *world_to_head, const float *projection);
        void fill_default_camera_matrices(float *world_to_head, float *projection) const;
        PackedFloat32Array copy_matrix(const float *matrix) const;

        static void render_adapter_graphics_initialized(void *user_data);
        static void render_adapter_graphics_shutdown(void *user_data);
        static int render_adapter_before_camera(void *user_data, int camera_id, int eye_id, const ImmGodotViewport *viewport);
        static void render_adapter_after_camera(void *user_data, int camera_id, int eye_id, const ImmGodotViewport *viewport, int render_result);

    private:
        String _document_path;
        bool _load_on_ready = false;
        bool _auto_play = true;
        float _volume = 1.0f;
        int _document_id = -1;
        bool _is_playing = false;
        bool _play_when_loaded = false;
        bool _backend_initialized = false;
        bool _matrix_debug_logging = false;
        int _graphics_initialized_count = 0;
        int _graphics_shutdown_count = 0;
        int _before_render_count = 0;
        int _after_render_count = 0;
        int _last_render_camera_id = -1;
        int _last_render_eye_id = -1;
        int _last_render_result = 0;
        ImmGodotViewport _last_render_viewport = {};
        ImmGodotRenderAdapter _render_adapter = {};
        int _last_matrix_camera_id = -1;
        float _last_world_to_head[16] = {};
        float _last_projection[16] = {};
        bool _has_last_matrices = false;
        Transform3D _document_transform;
        float _last_document_to_world[16] = {};
        bool _has_document_transform = false;
        PackedInt32Array _spawn_area_ids;
        int _active_spawn_area_index = -1;
    };
}
