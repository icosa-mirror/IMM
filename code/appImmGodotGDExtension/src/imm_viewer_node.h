#pragma once

#include "appImmGodot/src/imm_godot_plugin.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <atomic>
#include <mutex>

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
        void set_debug_logging(bool enabled);
        bool get_debug_logging() const;
        void set_antialiasing(int value);
        int get_antialiasing() const;
        void set_color_space(int value);
        int get_color_space() const;
        void set_renderer_api(int value);
        int get_renderer_api() const;
        void set_log_file_path(const String &path);
        String get_log_file_path() const;
        void set_tmp_folder_path(const String &path);
        String get_tmp_folder_path() const;
        void set_smoke_camera_id(int value);
        int get_smoke_camera_id() const;
        void set_smoke_viewport_width(int value);
        int get_smoke_viewport_width() const;
        void set_smoke_viewport_height(int value);
        int get_smoke_viewport_height() const;
        void set_auto_queue_render(bool enabled);
        bool get_auto_queue_render() const;
        void set_render_camera_path(const NodePath &path);
        NodePath get_render_camera_path() const;

        int load_document(const String &path = String());
        void unload_document();
        void play();
        void pause();
        void toggle_pause();
        void restart();
        void skip_forward();
        void skip_back();
        void next_chapter();
        void previous_chapter();
        void set_chapter(int chapter_index);
        int get_chapter_count() const;
        int get_current_chapter() const;
        void set_time(int64_t time_since_start, int64_t time_since_stop);
        Dictionary get_time() const;
        int64_t get_play_time() const;
        double get_play_time_seconds() const;
        void seek_relative_seconds(double seconds);
        void next_spawn_area();
        void previous_spawn_area();
        bool submit_mono_camera_matrices(int camera_id,
                                         const PackedFloat32Array &world_to_camera,
                                         const PackedFloat32Array &projection);
        int smoke_render_camera(int camera_id, int width, int height);
        void set_document_transform(const Transform3D &document_transform);
        Transform3D get_document_transform() const;
        void set_camera_transform(const Transform3D &camera_transform);
        int smoke_render_last_camera();
        int queue_render_last_camera();
        int queue_render_camera_transform(const Transform3D &camera_transform,
                                          int width,
                                          int height,
                                          float fov_degrees,
                                          int camera_id);
        bool register_render_camera(int camera_id);
        bool unregister_render_camera(int camera_id);
        bool is_render_camera_registered(int camera_id) const;
        PackedInt32Array get_registered_render_camera_ids() const;
        Dictionary get_render_diagnostics() const;
        Dictionary get_render_backend_diagnostics() const;

        bool is_loaded() const;
        bool is_playing() const;
        bool is_sequence_ready() const;
        Dictionary get_document_state() const;
        int get_document_info_flags() const;
        Dictionary get_bounding_box() const;
        int get_layer_count() const;
        Dictionary get_layer_info(int index) const;
        bool set_layer_visible(int layer_id, bool visible);
        bool clear_layer_visibility_override(int layer_id);
        bool set_layer_opacity(int layer_id, float opacity);
        bool set_layer_transform(int layer_id, const Transform3D &layer_transform);
        bool clear_layer_transform_override(int layer_id);
        Dictionary get_layer_diagnostics(int layer_id) const;
        Color get_background_color() const;
        PackedInt32Array get_spawn_area_ids() const;
        int get_active_spawn_area_index() const;
        Dictionary get_spawn_area_info(int spawn_area_id) const;
        Dictionary get_active_spawn_area_info() const;

    private:
        bool initialize_native_backend();
        void shutdown_native_backend();
        void refresh_spawn_areas();
        String resolve_load_path(const String &path) const;
        bool validate_matrix_array(const PackedFloat32Array &matrix, const char *name) const;
        Dictionary bounds_to_dictionary(const ImmGodotBounds3 &bounds) const;
        Dictionary layer_to_dictionary(const ImmGodotLayerInfo &layer) const;
        Dictionary layer_diagnostics_to_dictionary(const ImmGodotLayerDiagnostics &diagnostics) const;
        Dictionary spawn_area_to_dictionary(int spawn_area_id, const ImmGodotSpawnArea &spawn_area) const;
        PackedFloat32Array transform_to_matrix_array(const Transform3D &transform) const;
        PackedFloat32Array make_perspective_projection(float fov_degrees, float aspect, float z_near, float z_far) const;
        void update_auto_render_camera();
        static int render_adapter_before_camera(void *user_data, int camera_id, int eye_id, const ImmGodotViewport *viewport);
        static void render_adapter_after_camera(void *user_data, int camera_id, int eye_id, const ImmGodotViewport *viewport, int render_result);
        static void render_adapter_graphics_initialized(void *user_data);
        static void render_adapter_graphics_shutdown(void *user_data);

    private:
        struct RenderRequest
        {
            int cameraId = 0;
            int viewportWidth = 1280;
            int viewportHeight = 720;
        };

        String _document_path;
        String _log_file_path = "user://imm_godot_log.txt";
        String _tmp_folder_path = "user://";
        bool _load_on_ready = false;
        bool _auto_play = true;
        bool _debug_logging = false;
        bool _native_initialized = false;
        int _color_space = 0;
        int _renderer_api = ImmGodotRendererApi_Auto;
        int _antialiasing = 8;
        int _smoke_camera_id = 0;
        int _smoke_viewport_width = 1280;
        int _smoke_viewport_height = 720;
        bool _auto_queue_render = false;
        float _volume = 1.0f;
        int _document_id = -1;
        bool _is_playing = false;
        bool _pending_show_after_load = false;
        bool _sequence_ready_seen = false;
        Color _background_color = Color(0.0f, 0.0f, 0.0f, 1.0f);
        PackedInt32Array _spawn_area_ids;
        int _active_spawn_area_index = -1;
        Transform3D _document_transform;
        Transform3D _last_camera_transform;
        NodePath _render_camera_path;
        PackedFloat32Array _last_camera_projection;
        RenderRequest _pending_render_request;
        mutable std::mutex _render_request_mutex;
        PackedInt32Array _registered_render_camera_ids;
        std::atomic_int _adapter_before_render_count = 0;
        std::atomic_int _adapter_after_render_count = 0;
        std::atomic_int _adapter_graphics_initialized_count = 0;
        std::atomic_int _adapter_graphics_shutdown_count = 0;
        int _adapter_last_camera_id = -1;
        int _adapter_last_eye_id = -1;
        int _adapter_last_render_result = 0;
        ImmGodotViewport _adapter_last_viewport = {};
    };
}
