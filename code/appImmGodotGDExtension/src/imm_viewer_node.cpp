#include "imm_viewer_node.h"

#include "imm_viewer_compositor_effect.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

using namespace godot;

namespace
{
    constexpr int kImmDocumentLoadingStateLoaded = 3;

    bool is_document_timeline_ready(int document_id)
    {
        if (document_id < 0 || ImmGodot_IsSequenceReady(document_id) == 0)
        {
            return false;
        }

        ImmGodotDocumentState state = {};
        return ImmGodot_GetDocumentState(document_id, &state) == 0 &&
               state.loadingState == kImmDocumentLoadingStateLoaded;
    }
}

ImmViewerNode::ImmViewerNode() = default;

ImmViewerNode::~ImmViewerNode() = default;

void ImmViewerNode::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("load_document", "path"), &ImmViewerNode::load_document, String());
    ClassDB::bind_method(D_METHOD("unload_document"), &ImmViewerNode::unload_document);
    ClassDB::bind_method(D_METHOD("play"), &ImmViewerNode::play);
    ClassDB::bind_method(D_METHOD("pause"), &ImmViewerNode::pause);
    ClassDB::bind_method(D_METHOD("toggle_pause"), &ImmViewerNode::toggle_pause);
    ClassDB::bind_method(D_METHOD("restart"), &ImmViewerNode::restart);
    ClassDB::bind_method(D_METHOD("skip_forward"), &ImmViewerNode::skip_forward);
    ClassDB::bind_method(D_METHOD("skip_back"), &ImmViewerNode::skip_back);
    ClassDB::bind_method(D_METHOD("next_chapter"), &ImmViewerNode::next_chapter);
    ClassDB::bind_method(D_METHOD("previous_chapter"), &ImmViewerNode::previous_chapter);
    ClassDB::bind_method(D_METHOD("set_chapter", "chapter_index"), &ImmViewerNode::set_chapter);
    ClassDB::bind_method(D_METHOD("get_chapter_count"), &ImmViewerNode::get_chapter_count);
    ClassDB::bind_method(D_METHOD("get_current_chapter"), &ImmViewerNode::get_current_chapter);
    ClassDB::bind_method(D_METHOD("set_time", "time_since_start", "time_since_stop"), &ImmViewerNode::set_time);
    ClassDB::bind_method(D_METHOD("get_time"), &ImmViewerNode::get_time);
    ClassDB::bind_method(D_METHOD("get_play_time"), &ImmViewerNode::get_play_time);
    ClassDB::bind_method(D_METHOD("get_play_time_seconds"), &ImmViewerNode::get_play_time_seconds);
    ClassDB::bind_method(D_METHOD("seek_relative_seconds", "seconds"), &ImmViewerNode::seek_relative_seconds);
    ClassDB::bind_method(D_METHOD("next_spawn_area"), &ImmViewerNode::next_spawn_area);
    ClassDB::bind_method(D_METHOD("previous_spawn_area"), &ImmViewerNode::previous_spawn_area);
    ClassDB::bind_method(D_METHOD("submit_mono_camera_matrices", "camera_id", "world_to_camera", "projection"), &ImmViewerNode::submit_mono_camera_matrices);
    ClassDB::bind_method(D_METHOD("smoke_render_camera", "camera_id", "width", "height"), &ImmViewerNode::smoke_render_camera);
    ClassDB::bind_method(D_METHOD("set_document_transform", "document_transform"), &ImmViewerNode::set_document_transform);
    ClassDB::bind_method(D_METHOD("get_document_transform"), &ImmViewerNode::get_document_transform);
    ClassDB::bind_method(D_METHOD("is_loaded"), &ImmViewerNode::is_loaded);
    ClassDB::bind_method(D_METHOD("is_playing"), &ImmViewerNode::is_playing);
    ClassDB::bind_method(D_METHOD("is_sequence_ready"), &ImmViewerNode::is_sequence_ready);
    ClassDB::bind_method(D_METHOD("get_document_state"), &ImmViewerNode::get_document_state);
    ClassDB::bind_method(D_METHOD("get_document_info_flags"), &ImmViewerNode::get_document_info_flags);
    ClassDB::bind_method(D_METHOD("get_bounding_box"), &ImmViewerNode::get_bounding_box);
    ClassDB::bind_method(D_METHOD("get_layer_count"), &ImmViewerNode::get_layer_count);
    ClassDB::bind_method(D_METHOD("get_layer_info", "index"), &ImmViewerNode::get_layer_info);
    ClassDB::bind_method(D_METHOD("set_layer_visible", "layer_id", "visible"), &ImmViewerNode::set_layer_visible);
    ClassDB::bind_method(D_METHOD("clear_layer_visibility_override", "layer_id"), &ImmViewerNode::clear_layer_visibility_override);
    ClassDB::bind_method(D_METHOD("set_layer_opacity", "layer_id", "opacity"), &ImmViewerNode::set_layer_opacity);
    ClassDB::bind_method(D_METHOD("set_layer_transform", "layer_id", "layer_transform"), &ImmViewerNode::set_layer_transform);
    ClassDB::bind_method(D_METHOD("clear_layer_transform_override", "layer_id"), &ImmViewerNode::clear_layer_transform_override);
    ClassDB::bind_method(D_METHOD("get_layer_diagnostics", "layer_id"), &ImmViewerNode::get_layer_diagnostics);
    ClassDB::bind_method(D_METHOD("get_background_color"), &ImmViewerNode::get_background_color);
    ClassDB::bind_method(D_METHOD("get_spawn_area_ids"), &ImmViewerNode::get_spawn_area_ids);
    ClassDB::bind_method(D_METHOD("get_active_spawn_area_index"), &ImmViewerNode::get_active_spawn_area_index);
    ClassDB::bind_method(D_METHOD("get_spawn_area_info", "spawn_area_id"), &ImmViewerNode::get_spawn_area_info);
    ClassDB::bind_method(D_METHOD("get_active_spawn_area_info"), &ImmViewerNode::get_active_spawn_area_info);

    ClassDB::bind_method(D_METHOD("set_document_path", "path"), &ImmViewerNode::set_document_path);
    ClassDB::bind_method(D_METHOD("get_document_path"), &ImmViewerNode::get_document_path);
    ClassDB::bind_method(D_METHOD("set_load_on_ready", "enabled"), &ImmViewerNode::set_load_on_ready);
    ClassDB::bind_method(D_METHOD("get_load_on_ready"), &ImmViewerNode::get_load_on_ready);
    ClassDB::bind_method(D_METHOD("set_auto_play", "enabled"), &ImmViewerNode::set_auto_play);
    ClassDB::bind_method(D_METHOD("get_auto_play"), &ImmViewerNode::get_auto_play);
    ClassDB::bind_method(D_METHOD("set_volume", "value"), &ImmViewerNode::set_volume);
    ClassDB::bind_method(D_METHOD("get_volume"), &ImmViewerNode::get_volume);
    ClassDB::bind_method(D_METHOD("set_debug_logging", "enabled"), &ImmViewerNode::set_debug_logging);
    ClassDB::bind_method(D_METHOD("get_debug_logging"), &ImmViewerNode::get_debug_logging);
    ClassDB::bind_method(D_METHOD("set_antialiasing", "value"), &ImmViewerNode::set_antialiasing);
    ClassDB::bind_method(D_METHOD("get_antialiasing"), &ImmViewerNode::get_antialiasing);
    ClassDB::bind_method(D_METHOD("set_color_space", "value"), &ImmViewerNode::set_color_space);
    ClassDB::bind_method(D_METHOD("get_color_space"), &ImmViewerNode::get_color_space);
    ClassDB::bind_method(D_METHOD("set_renderer_api", "value"), &ImmViewerNode::set_renderer_api);
    ClassDB::bind_method(D_METHOD("get_renderer_api"), &ImmViewerNode::get_renderer_api);
    ClassDB::bind_method(D_METHOD("set_log_file_path", "path"), &ImmViewerNode::set_log_file_path);
    ClassDB::bind_method(D_METHOD("get_log_file_path"), &ImmViewerNode::get_log_file_path);
    ClassDB::bind_method(D_METHOD("set_tmp_folder_path", "path"), &ImmViewerNode::set_tmp_folder_path);
    ClassDB::bind_method(D_METHOD("get_tmp_folder_path"), &ImmViewerNode::get_tmp_folder_path);
    ClassDB::bind_method(D_METHOD("set_smoke_camera_id", "value"), &ImmViewerNode::set_smoke_camera_id);
    ClassDB::bind_method(D_METHOD("get_smoke_camera_id"), &ImmViewerNode::get_smoke_camera_id);
    ClassDB::bind_method(D_METHOD("set_smoke_viewport_width", "value"), &ImmViewerNode::set_smoke_viewport_width);
    ClassDB::bind_method(D_METHOD("get_smoke_viewport_width"), &ImmViewerNode::get_smoke_viewport_width);
    ClassDB::bind_method(D_METHOD("set_smoke_viewport_height", "value"), &ImmViewerNode::set_smoke_viewport_height);
    ClassDB::bind_method(D_METHOD("get_smoke_viewport_height"), &ImmViewerNode::get_smoke_viewport_height);
    ClassDB::bind_method(D_METHOD("set_auto_queue_render", "enabled"), &ImmViewerNode::set_auto_queue_render);
    ClassDB::bind_method(D_METHOD("get_auto_queue_render"), &ImmViewerNode::get_auto_queue_render);
    ClassDB::bind_method(D_METHOD("set_render_camera_path", "path"), &ImmViewerNode::set_render_camera_path);
    ClassDB::bind_method(D_METHOD("get_render_camera_path"), &ImmViewerNode::get_render_camera_path);
    ClassDB::bind_method(D_METHOD("set_camera_transform", "camera_transform"), &ImmViewerNode::set_camera_transform);
    ClassDB::bind_method(D_METHOD("smoke_render_last_camera"), &ImmViewerNode::smoke_render_last_camera);
    ClassDB::bind_method(D_METHOD("queue_render_last_camera"), &ImmViewerNode::queue_render_last_camera);
    ClassDB::bind_method(D_METHOD("queue_render_camera_transform", "camera_transform", "width", "height", "fov_degrees", "camera_id"), &ImmViewerNode::queue_render_camera_transform);
    ClassDB::bind_method(D_METHOD("register_render_camera", "camera_id"), &ImmViewerNode::register_render_camera);
    ClassDB::bind_method(D_METHOD("unregister_render_camera", "camera_id"), &ImmViewerNode::unregister_render_camera);
    ClassDB::bind_method(D_METHOD("is_render_camera_registered", "camera_id"), &ImmViewerNode::is_render_camera_registered);
    ClassDB::bind_method(D_METHOD("get_registered_render_camera_ids"), &ImmViewerNode::get_registered_render_camera_ids);
    ClassDB::bind_method(D_METHOD("get_render_diagnostics"), &ImmViewerNode::get_render_diagnostics);
    ClassDB::bind_method(D_METHOD("get_render_backend_diagnostics"), &ImmViewerNode::get_render_backend_diagnostics);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "document_path"), "set_document_path", "get_document_path");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "load_on_ready"), "set_load_on_ready", "get_load_on_ready");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_play"), "set_auto_play", "get_auto_play");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_volume", "get_volume");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_logging"), "set_debug_logging", "get_debug_logging");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "color_space", PROPERTY_HINT_ENUM, "Linear,Gamma"), "set_color_space", "get_color_space");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "renderer_api", PROPERTY_HINT_ENUM, "Auto,OpenGL,Direct3D,GLES,Metal,Vulkan"), "set_renderer_api", "get_renderer_api");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "antialiasing", PROPERTY_HINT_RANGE, "1,16,1"), "set_antialiasing", "get_antialiasing");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "log_file_path"), "set_log_file_path", "get_log_file_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "tmp_folder_path"), "set_tmp_folder_path", "get_tmp_folder_path");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "smoke_camera_id", PROPERTY_HINT_RANGE, "0,255,1"), "set_smoke_camera_id", "get_smoke_camera_id");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "smoke_viewport_width", PROPERTY_HINT_RANGE, "1,8192,1"), "set_smoke_viewport_width", "get_smoke_viewport_width");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "smoke_viewport_height", PROPERTY_HINT_RANGE, "1,8192,1"), "set_smoke_viewport_height", "get_smoke_viewport_height");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_queue_render"), "set_auto_queue_render", "get_auto_queue_render");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "render_camera_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Camera3D"), "set_render_camera_path", "get_render_camera_path");

    ADD_SIGNAL(MethodInfo("document_loaded", PropertyInfo(Variant::STRING, "path")));
    ADD_SIGNAL(MethodInfo("document_unloaded"));
    ADD_SIGNAL(MethodInfo("playback_changed", PropertyInfo(Variant::BOOL, "is_playing")));
    ADD_SIGNAL(MethodInfo("spawn_area_changed", PropertyInfo(Variant::INT, "active_index")));
    ADD_SIGNAL(MethodInfo("native_backend_initialized"));
    ADD_SIGNAL(MethodInfo("native_backend_failed", PropertyInfo(Variant::INT, "error_code")));
}

void ImmViewerNode::_ready()
{
    set_process(true);

    if (!initialize_native_backend())
    {
        return;
    }

    if (_auto_queue_render)
    {
        register_render_camera(_smoke_camera_id);
    }

    if (_load_on_ready)
    {
        load_document(_document_path);
    }
}

void ImmViewerNode::_exit_tree()
{
    if (_auto_queue_render)
    {
        unregister_render_camera(_smoke_camera_id);
    }
    shutdown_native_backend();
}

void ImmViewerNode::_process(double)
{
    if (_native_initialized && _document_id >= 0)
    {
        ImmGodot_GlobalWork(1);
        if (is_sequence_ready())
        {
            if (!_sequence_ready_seen)
            {
                refresh_spawn_areas();
                _sequence_ready_seen = true;
            }
            if (_pending_show_after_load && _is_playing)
            {
                ImmGodot_Resume(_document_id);
                ImmGodot_Show(_document_id);
                _pending_show_after_load = false;
            }
        }
    }

    update_auto_render_camera();
}

void ImmViewerNode::set_document_path(const String &path)
{
    _document_path = path;
}

String ImmViewerNode::get_document_path() const
{
    return _document_path;
}

void ImmViewerNode::set_load_on_ready(bool enabled)
{
    _load_on_ready = enabled;
}

bool ImmViewerNode::get_load_on_ready() const
{
    return _load_on_ready;
}

void ImmViewerNode::set_auto_play(bool enabled)
{
    _auto_play = enabled;
}

bool ImmViewerNode::get_auto_play() const
{
    return _auto_play;
}

void ImmViewerNode::set_volume(float value)
{
    _volume = CLAMP(value, 0.0f, 1.0f);
    if (_native_initialized && _document_id >= 0)
    {
        ImmGodot_SetVolume(_document_id, _volume);
    }
}

float ImmViewerNode::get_volume() const
{
    return _volume;
}

void ImmViewerNode::set_debug_logging(bool enabled)
{
    _debug_logging = enabled;
    ImmGodot_SetDebugLogging(_debug_logging ? 1 : 0);
}

bool ImmViewerNode::get_debug_logging() const
{
    return _debug_logging;
}

void ImmViewerNode::set_antialiasing(int value)
{
    _antialiasing = CLAMP(value, 1, 16);
}

int ImmViewerNode::get_antialiasing() const
{
    return _antialiasing;
}

void ImmViewerNode::set_color_space(int value)
{
    _color_space = CLAMP(value, 0, 1);
}

int ImmViewerNode::get_color_space() const
{
    return _color_space;
}

void ImmViewerNode::set_renderer_api(int value)
{
    _renderer_api = CLAMP(value, ImmGodotRendererApi_Auto, ImmGodotRendererApi_Vulkan);
}

int ImmViewerNode::get_renderer_api() const
{
    return _renderer_api;
}

void ImmViewerNode::set_log_file_path(const String &path)
{
    _log_file_path = path;
}

String ImmViewerNode::get_log_file_path() const
{
    return _log_file_path;
}

void ImmViewerNode::set_tmp_folder_path(const String &path)
{
    _tmp_folder_path = path;
}

String ImmViewerNode::get_tmp_folder_path() const
{
    return _tmp_folder_path;
}

void ImmViewerNode::set_smoke_camera_id(int value)
{
    const int next_camera_id = CLAMP(value, 0, 255);
    if (_smoke_camera_id == next_camera_id)
    {
        return;
    }

    const int previous_camera_id = _smoke_camera_id;
    _smoke_camera_id = next_camera_id;
    if (_auto_queue_render && is_inside_tree())
    {
        unregister_render_camera(previous_camera_id);
        register_render_camera(_smoke_camera_id);
    }
}

int ImmViewerNode::get_smoke_camera_id() const
{
    return _smoke_camera_id;
}

void ImmViewerNode::set_smoke_viewport_width(int value)
{
    _smoke_viewport_width = MAX(value, 1);
}

int ImmViewerNode::get_smoke_viewport_width() const
{
    return _smoke_viewport_width;
}

void ImmViewerNode::set_smoke_viewport_height(int value)
{
    _smoke_viewport_height = MAX(value, 1);
}

int ImmViewerNode::get_smoke_viewport_height() const
{
    return _smoke_viewport_height;
}

void ImmViewerNode::set_auto_queue_render(bool enabled)
{
    if (_auto_queue_render == enabled)
    {
        return;
    }

    _auto_queue_render = enabled;
    if (!is_inside_tree())
    {
        return;
    }

    if (_auto_queue_render)
    {
        register_render_camera(_smoke_camera_id);
    }
    else
    {
        unregister_render_camera(_smoke_camera_id);
    }
}

bool ImmViewerNode::get_auto_queue_render() const
{
    return _auto_queue_render;
}

void ImmViewerNode::set_render_camera_path(const NodePath &path)
{
    _render_camera_path = path;
}

NodePath ImmViewerNode::get_render_camera_path() const
{
    return _render_camera_path;
}

int ImmViewerNode::load_document(const String &path)
{
    if (!_native_initialized && !initialize_native_backend())
    {
        return -1;
    }

    const String resolved = path.is_empty() ? _document_path : path;
    if (resolved.is_empty())
    {
        return -1;
    }

    _document_path = resolved;
    CharString utf8 = resolve_load_path(resolved).utf8();
    _document_id = ImmGodot_LoadFromFile(const_cast<char *>(utf8.get_data()));
    if (_document_id < 0)
    {
        return _document_id;
    }

    ImmGodot_SetVolume(_document_id, _volume);
    set_document_transform(_document_transform);
    _sequence_ready_seen = false;
    _pending_show_after_load = _auto_play;
    ImmGodotPlayerInfo player_info = {};
    if (ImmGodot_GetPlayerInfo(&player_info) == 0)
    {
        _background_color = Color(player_info.backgroundRed,
                                  player_info.backgroundGreen,
                                  player_info.backgroundBlue,
                                  1.0f);
    }
    if (_auto_play)
    {
        _is_playing = true;
        emit_signal("playback_changed", true);
    }

    emit_signal("document_loaded", _document_path);
    return _document_id;
}

void ImmViewerNode::unload_document()
{
    if (!_native_initialized || _document_id < 0)
    {
        return;
    }

    ImmGodot_Unload(_document_id);
    _document_id = -1;
    _is_playing = false;
    _pending_show_after_load = false;
    _sequence_ready_seen = false;
    _spawn_area_ids.clear();
    _active_spawn_area_index = -1;
    emit_signal("document_unloaded");
    emit_signal("playback_changed", false);
}

void ImmViewerNode::play()
{
    if (!_native_initialized || _document_id < 0)
    {
        return;
    }

    if (is_sequence_ready())
    {
        ImmGodot_Resume(_document_id);
        ImmGodot_Show(_document_id);
        _pending_show_after_load = false;
    }
    else
    {
        _pending_show_after_load = true;
    }
    _is_playing = true;
    emit_signal("playback_changed", true);
}

void ImmViewerNode::pause()
{
    if (!_native_initialized || _document_id < 0)
    {
        return;
    }

    if (is_sequence_ready())
    {
        ImmGodot_Pause(_document_id);
    }
    _pending_show_after_load = false;
    _is_playing = false;
    emit_signal("playback_changed", false);
}

void ImmViewerNode::toggle_pause()
{
    if (_is_playing)
    {
        pause();
    }
    else
    {
        play();
    }
}

void ImmViewerNode::restart()
{
    if (!_native_initialized || _document_id < 0)
    {
        return;
    }

    ImmGodot_Restart(_document_id);
    _is_playing = true;
    emit_signal("playback_changed", true);
}

void ImmViewerNode::skip_forward()
{
    if (!_native_initialized || _document_id < 0)
    {
        return;
    }

    ImmGodot_SkipForward(_document_id);
}

void ImmViewerNode::skip_back()
{
    if (!_native_initialized || _document_id < 0)
    {
        return;
    }

    ImmGodot_SkipBack(_document_id);
}

void ImmViewerNode::next_chapter()
{
    if (!_native_initialized || _document_id < 0)
    {
        return;
    }

    const int count = ImmGodot_GetChapterCount(_document_id);
    if (count <= 0)
    {
        skip_forward();
        return;
    }

    const int current = get_current_chapter();
    set_chapter((current + 1) % count);
}

void ImmViewerNode::previous_chapter()
{
    if (!_native_initialized || _document_id < 0)
    {
        return;
    }

    const int count = ImmGodot_GetChapterCount(_document_id);
    if (count <= 0)
    {
        skip_back();
        return;
    }

    const int current = get_current_chapter();
    set_chapter((current - 1 + count) % count);
}

void ImmViewerNode::set_chapter(int chapter_index)
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready() || chapter_index < 0)
    {
        return;
    }

    const int count = get_chapter_count();
    if (count > 0 && chapter_index >= count)
    {
        return;
    }

    ImmGodot_SetChapter(_document_id, chapter_index);
}

int ImmViewerNode::get_chapter_count() const
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready())
    {
        return 0;
    }

    return ImmGodot_GetChapterCount(_document_id);
}

int ImmViewerNode::get_current_chapter() const
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready())
    {
        return 0;
    }

    return ImmGodot_GetCurrentChapter(_document_id);
}

void ImmViewerNode::set_time(int64_t time_since_start, int64_t time_since_stop)
{
    if (!_native_initialized || !is_document_timeline_ready(_document_id))
    {
        return;
    }

    const int64_t start = time_since_start < 0 ? 0 : time_since_start;
    const int64_t stop = time_since_stop < 0 ? 0 : time_since_stop;
    ImmGodot_SetTime(_document_id, start, stop);
}

Dictionary ImmViewerNode::get_time() const
{
    Dictionary result;
    if (!_native_initialized || !is_document_timeline_ready(_document_id))
    {
        result["time_since_start"] = static_cast<int64_t>(0);
        result["time_since_stop"] = static_cast<int64_t>(0);
        result["play_time"] = static_cast<int64_t>(0);
        result["play_time_seconds"] = 0.0;
        return result;
    }

    ImmGodotTimeInfo time_info = {};
    if (ImmGodot_GetTime(_document_id, &time_info) != 0)
    {
        return result;
    }

    result["time_since_start"] = time_info.timeSinceStart;
    result["time_since_stop"] = time_info.timeSinceStop;
    result["play_time"] = time_info.playTime;
    result["play_time_seconds"] = ImmGodot_TicksToSeconds(time_info.playTime);
    return result;
}

int64_t ImmViewerNode::get_play_time() const
{
    if (!_native_initialized || !is_document_timeline_ready(_document_id))
    {
        return 0;
    }

    return ImmGodot_GetPlayTime(_document_id);
}

double ImmViewerNode::get_play_time_seconds() const
{
    return ImmGodot_TicksToSeconds(get_play_time());
}

void ImmViewerNode::seek_relative_seconds(double seconds)
{
    if (!_native_initialized || !is_document_timeline_ready(_document_id))
    {
        return;
    }

    const int64_t current = ImmGodot_GetPlayTime(_document_id);
    const int64_t offset = ImmGodot_SecondsToTicks(seconds);
    const int64_t target = (current + offset) < 0 ? 0 : current + offset;
    set_time(target, 0);
}

void ImmViewerNode::next_spawn_area()
{
    if (!_native_initialized || _spawn_area_ids.is_empty())
    {
        return;
    }

    _active_spawn_area_index = (_active_spawn_area_index + 1) % _spawn_area_ids.size();
    ImmGodot_SetActiveSpawnAreaId(_document_id, _spawn_area_ids[_active_spawn_area_index]);
    emit_signal("spawn_area_changed", _active_spawn_area_index);
}

void ImmViewerNode::previous_spawn_area()
{
    if (!_native_initialized || _spawn_area_ids.is_empty())
    {
        return;
    }

    _active_spawn_area_index = (_active_spawn_area_index - 1 + _spawn_area_ids.size()) % _spawn_area_ids.size();
    ImmGodot_SetActiveSpawnAreaId(_document_id, _spawn_area_ids[_active_spawn_area_index]);
    emit_signal("spawn_area_changed", _active_spawn_area_index);
}

bool ImmViewerNode::submit_mono_camera_matrices(int camera_id,
                                                const PackedFloat32Array &world_to_camera,
                                                const PackedFloat32Array &projection)
{
    if (!_native_initialized && !initialize_native_backend())
    {
        return false;
    }
    if (camera_id < 0 || camera_id >= 256)
    {
        UtilityFunctions::push_error("ImmViewerNode camera_id must be in [0, 255]: ", camera_id);
        return false;
    }
    if (!validate_matrix_array(world_to_camera, "world_to_camera") ||
        !validate_matrix_array(projection, "projection"))
    {
        return false;
    }

    PackedFloat32Array mutable_world_to_camera = world_to_camera;
    PackedFloat32Array mutable_projection = projection;
    ImmGodot_SetCameraMatrices(camera_id,
                               0,
                               mutable_world_to_camera.ptrw(),
                               mutable_projection.ptrw(),
                               nullptr,
                               nullptr,
                               nullptr,
                               nullptr);
    return true;
}

int ImmViewerNode::smoke_render_camera(int camera_id, int width, int height)
{
    if (!_native_initialized && !initialize_native_backend())
    {
        return -1;
    }
    if (camera_id < 0 || camera_id >= 256 || width <= 0 || height <= 0)
    {
        UtilityFunctions::push_error("ImmViewerNode smoke_render_camera received invalid camera or viewport.");
        return -1;
    }

    return ImmGodot_RenderCamera(camera_id,
                                 0,
                                 0.0f,
                                 0.0f,
                                 static_cast<float>(width),
                                 static_cast<float>(height),
                                 0.0f,
                                 1.0f);
}

void ImmViewerNode::set_document_transform(const Transform3D &document_transform)
{
    _document_transform = document_transform;
    if (!_native_initialized || _document_id < 0)
    {
        return;
    }

    PackedFloat32Array matrix = transform_to_matrix_array(_document_transform);
    ImmGodot_SetDocumentToWorld(_document_id, matrix.ptrw());
}

Transform3D ImmViewerNode::get_document_transform() const
{
    return _document_transform;
}

void ImmViewerNode::set_camera_transform(const Transform3D &camera_transform)
{
    _last_camera_transform = camera_transform;
    const float aspect = static_cast<float>(_smoke_viewport_width) / static_cast<float>(_smoke_viewport_height);
    _last_camera_projection = make_perspective_projection(70.0f, aspect, 0.05f, 10000.0f);
    submit_mono_camera_matrices(_smoke_camera_id,
                                transform_to_matrix_array(camera_transform.affine_inverse()),
                                _last_camera_projection);
}

int ImmViewerNode::smoke_render_last_camera()
{
    if (_last_camera_projection.is_empty())
    {
        set_camera_transform(_last_camera_transform);
    }

    return smoke_render_camera(_smoke_camera_id, _smoke_viewport_width, _smoke_viewport_height);
}

int ImmViewerNode::queue_render_last_camera()
{
    if (!_native_initialized && !initialize_native_backend())
    {
        return -1;
    }
    if (_last_camera_projection.is_empty())
    {
        set_camera_transform(_last_camera_transform);
    }

    {
        std::lock_guard<std::mutex> lock(_render_request_mutex);
        _pending_render_request.cameraId = _smoke_camera_id;
        _pending_render_request.viewportWidth = _smoke_viewport_width;
        _pending_render_request.viewportHeight = _smoke_viewport_height;
    }
    ImmViewerCompositorEffect::queue_render_request(_smoke_camera_id, _smoke_viewport_width, _smoke_viewport_height);
    return 0;
}

int ImmViewerNode::queue_render_camera_transform(const Transform3D &camera_transform,
                                                 int width,
                                                 int height,
                                                 float fov_degrees,
                                                 int camera_id)
{
    if (!_native_initialized && !initialize_native_backend())
    {
        return -1;
    }
    if (camera_id < 0 || camera_id >= 256 || width <= 0 || height <= 0)
    {
        UtilityFunctions::push_error("ImmViewerNode queue_render_camera_transform received invalid camera or viewport.");
        return -1;
    }
    if (!is_render_camera_registered(camera_id))
    {
        return 1;
    }

    _last_camera_transform = camera_transform;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    _last_camera_projection = make_perspective_projection(fov_degrees, aspect, 0.05f, 10000.0f);
    if (!submit_mono_camera_matrices(camera_id,
                                     transform_to_matrix_array(camera_transform.affine_inverse()),
                                     _last_camera_projection))
    {
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock(_render_request_mutex);
        _pending_render_request.cameraId = camera_id;
        _pending_render_request.viewportWidth = width;
        _pending_render_request.viewportHeight = height;
    }
    ImmViewerCompositorEffect::queue_render_request(camera_id, width, height);
    return 0;
}

bool ImmViewerNode::register_render_camera(int camera_id)
{
    if (camera_id < 0 || camera_id >= 256)
    {
        UtilityFunctions::push_error("ImmViewerNode render camera id must be in [0, 255]: ", camera_id);
        return false;
    }
    if (is_render_camera_registered(camera_id))
    {
        return true;
    }

    _registered_render_camera_ids.push_back(camera_id);
    return true;
}

bool ImmViewerNode::unregister_render_camera(int camera_id)
{
    for (int index = 0; index < _registered_render_camera_ids.size(); ++index)
    {
        if (_registered_render_camera_ids[index] == camera_id)
        {
            _registered_render_camera_ids.remove_at(index);
            return true;
        }
    }

    return false;
}

bool ImmViewerNode::is_render_camera_registered(int camera_id) const
{
    for (int index = 0; index < _registered_render_camera_ids.size(); ++index)
    {
        if (_registered_render_camera_ids[index] == camera_id)
        {
            return true;
        }
    }

    return false;
}

PackedInt32Array ImmViewerNode::get_registered_render_camera_ids() const
{
    return _registered_render_camera_ids;
}

Dictionary ImmViewerNode::get_render_diagnostics() const
{
    RenderRequest request;
    int adapter_last_camera_id = -1;
    int adapter_last_eye_id = -1;
    int adapter_last_render_result = 0;
    ImmGodotViewport adapter_last_viewport = {};
    {
        std::lock_guard<std::mutex> lock(_render_request_mutex);
        request = _pending_render_request;
        adapter_last_camera_id = _adapter_last_camera_id;
        adapter_last_eye_id = _adapter_last_eye_id;
        adapter_last_render_result = _adapter_last_render_result;
        adapter_last_viewport = _adapter_last_viewport;
    }

    Dictionary result;
    result["last_camera_id"] = request.cameraId;
    result["last_viewport_width"] = request.viewportWidth;
    result["last_viewport_height"] = request.viewportHeight;
    result["registered_camera_ids"] = _registered_render_camera_ids;
    result["render_callback_queued"] = false;
    result["auto_queue_render"] = _auto_queue_render;
    result["render_camera_path"] = _render_camera_path;
    result["has_last_projection"] = !_last_camera_projection.is_empty();
    result["last_projection_size"] = _last_camera_projection.size();
    result["last_camera_origin"] = _last_camera_transform.origin;
    result["adapter_before_render_count"] = _adapter_before_render_count.load();
    result["adapter_after_render_count"] = _adapter_after_render_count.load();
    result["adapter_graphics_initialized_count"] = _adapter_graphics_initialized_count.load();
    result["adapter_graphics_shutdown_count"] = _adapter_graphics_shutdown_count.load();
    result["adapter_last_camera_id"] = adapter_last_camera_id;
    result["adapter_last_eye_id"] = adapter_last_eye_id;
    result["adapter_last_render_result"] = adapter_last_render_result;
    result["adapter_last_viewport_width"] = adapter_last_viewport.width;
    result["adapter_last_viewport_height"] = adapter_last_viewport.height;
    return result;
}

Dictionary ImmViewerNode::get_render_backend_diagnostics() const
{
    ProjectSettings *project_settings = ProjectSettings::get_singleton();
    RenderingServer *rendering_server = RenderingServer::get_singleton();
    RenderingDevice *rendering_device = rendering_server != nullptr ? rendering_server->get_rendering_device() : nullptr;

    String rendering_method;
    String rendering_driver;
    String actual_rendering_method;
    String actual_rendering_driver;
    if (project_settings != nullptr)
    {
        rendering_method = project_settings->get_setting("rendering/renderer/rendering_method");
        rendering_driver = project_settings->get_setting("rendering/rendering_device/driver");
    }
    if (rendering_server != nullptr)
    {
        actual_rendering_method = rendering_server->get_current_rendering_method();
        actual_rendering_driver = rendering_server->get_current_rendering_driver_name();
    }

    const String effective_rendering_method = actual_rendering_method.is_empty() ? rendering_method : actual_rendering_method;
    const String effective_rendering_driver = actual_rendering_driver.is_empty() ? rendering_driver : actual_rendering_driver;
    const bool is_compatibility = effective_rendering_method == "gl_compatibility";
    const bool wants_metal = _renderer_api == ImmGodotRendererApi_Auto || _renderer_api == ImmGodotRendererApi_Metal;
    const bool driver_is_metal = effective_rendering_driver == "metal";
    const bool has_generic_driver_resources = true;
    const bool has_compositor_effect_path = true;
    const bool metal_adapter_candidate = rendering_device != nullptr && !is_compatibility && wants_metal && driver_is_metal && has_generic_driver_resources && has_compositor_effect_path;

    Dictionary result;
    result["native_backend_initialized"] = _native_initialized;
    result["renderer_api"] = _renderer_api;
    result["project_rendering_method"] = rendering_method;
    result["project_rendering_driver"] = rendering_driver;
    result["actual_rendering_method"] = actual_rendering_method;
    result["actual_rendering_driver"] = actual_rendering_driver;
    result["has_rendering_device"] = rendering_device != nullptr;
    result["is_compatibility_renderer"] = is_compatibility;
    result["wants_metal_renderer"] = wants_metal;
    result["has_generic_driver_resources"] = has_generic_driver_resources;
    result["has_compositor_effect_path"] = has_compositor_effect_path;
    result["metal_adapter_candidate"] = metal_adapter_candidate;
    return result;
}

void ImmViewerNode::update_auto_render_camera()
{
    if (!_native_initialized || !_auto_queue_render || _render_camera_path.is_empty())
    {
        return;
    }

    Node *node = get_node_or_null(_render_camera_path);
    Camera3D *camera = Object::cast_to<Camera3D>(node);
    if (camera == nullptr)
    {
        return;
    }

    Viewport *viewport = get_viewport();
    if (viewport == nullptr)
    {
        return;
    }

    const Vector2 viewport_size = viewport->get_visible_rect().size;
    const int width = MAX(static_cast<int>(viewport_size.x), 1);
    const int height = MAX(static_cast<int>(viewport_size.y), 1);
    if (is_loaded())
    {
        queue_render_camera_transform(camera->get_global_transform(),
                                      width,
                                      height,
                                      static_cast<float>(camera->get_fov()),
                                      _smoke_camera_id);
    }
    else
    {
        set_camera_transform(camera->get_global_transform());
    }
}

int ImmViewerNode::render_adapter_before_camera(void *user_data,
                                                int camera_id,
                                                int eye_id,
                                                const ImmGodotViewport *viewport)
{
    ImmViewerNode *viewer = static_cast<ImmViewerNode *>(user_data);
    if (viewer == nullptr)
    {
        return 1;
    }

    viewer->_adapter_before_render_count++;
    {
        std::lock_guard<std::mutex> lock(viewer->_render_request_mutex);
        viewer->_adapter_last_camera_id = camera_id;
        viewer->_adapter_last_eye_id = eye_id;
        if (viewport != nullptr)
        {
            viewer->_adapter_last_viewport = *viewport;
        }
    }
    return 1;
}

void ImmViewerNode::render_adapter_after_camera(void *user_data,
                                               int camera_id,
                                               int eye_id,
                                               const ImmGodotViewport *viewport,
                                               int render_result)
{
    ImmViewerNode *viewer = static_cast<ImmViewerNode *>(user_data);
    if (viewer == nullptr)
    {
        return;
    }

    viewer->_adapter_after_render_count++;
    {
        std::lock_guard<std::mutex> lock(viewer->_render_request_mutex);
        viewer->_adapter_last_camera_id = camera_id;
        viewer->_adapter_last_eye_id = eye_id;
        viewer->_adapter_last_render_result = render_result;
        if (viewport != nullptr)
        {
            viewer->_adapter_last_viewport = *viewport;
        }
    }
}

void ImmViewerNode::render_adapter_graphics_initialized(void *user_data)
{
    ImmViewerNode *viewer = static_cast<ImmViewerNode *>(user_data);
    if (viewer != nullptr)
    {
        viewer->_adapter_graphics_initialized_count++;
    }
}

void ImmViewerNode::render_adapter_graphics_shutdown(void *user_data)
{
    ImmViewerNode *viewer = static_cast<ImmViewerNode *>(user_data);
    if (viewer != nullptr)
    {
        viewer->_adapter_graphics_shutdown_count++;
    }
}

bool ImmViewerNode::is_loaded() const
{
    return _document_id >= 0;
}

bool ImmViewerNode::is_playing() const
{
    return _is_playing;
}

bool ImmViewerNode::is_sequence_ready() const
{
    return _native_initialized && _document_id >= 0 && ImmGodot_IsSequenceReady(_document_id) != 0;
}

Dictionary ImmViewerNode::get_document_state() const
{
    Dictionary result;
    result["loading_state"] = 0;
    result["playback_state"] = _is_playing ? 0 : 1;
    result["sequence_ready"] = false;
    result["info_flags"] = 0;

    if (!_native_initialized || _document_id < 0)
    {
        return result;
    }

    ImmGodotDocumentState state = {};
    if (ImmGodot_GetDocumentState(_document_id, &state) == 0)
    {
        result["loading_state"] = state.loadingState;
        result["playback_state"] = state.playbackState;
    }
    result["sequence_ready"] = ImmGodot_IsSequenceReady(_document_id) != 0;
    result["info_flags"] = static_cast<int>(ImmGodot_GetDocumentInfoEx(_document_id));
    return result;
}

int ImmViewerNode::get_document_info_flags() const
{
    if (!_native_initialized || _document_id < 0)
    {
        return 0;
    }

    return static_cast<int>(ImmGodot_GetDocumentInfoEx(_document_id));
}

Dictionary ImmViewerNode::get_bounding_box() const
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready())
    {
        return Dictionary();
    }

    ImmGodotBounds3 bounds = {};
    if (ImmGodot_GetBoundingBox(_document_id, &bounds) != 0)
    {
        return Dictionary();
    }

    return bounds_to_dictionary(bounds);
}

int ImmViewerNode::get_layer_count() const
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready())
    {
        return 0;
    }

    return ImmGodot_GetLayerCount(_document_id);
}

Dictionary ImmViewerNode::get_layer_info(int index) const
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready() || index < 0)
    {
        return Dictionary();
    }

    ImmGodotLayerInfo layer = {};
    if (ImmGodot_GetLayerInfoByIndex(_document_id, index, &layer) != 0)
    {
        return Dictionary();
    }

    return layer_to_dictionary(layer);
}

bool ImmViewerNode::set_layer_visible(int layer_id, bool visible)
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready())
    {
        return false;
    }

    return ImmGodot_SetLayerVisible(_document_id, layer_id, visible ? 1 : 0) == 0;
}

bool ImmViewerNode::clear_layer_visibility_override(int layer_id)
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready())
    {
        return false;
    }

    return ImmGodot_ClearLayerVisibilityOverride(_document_id, layer_id) == 0;
}

bool ImmViewerNode::set_layer_opacity(int layer_id, float opacity)
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready())
    {
        return false;
    }

    const float clamped = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
    return ImmGodot_SetLayerOpacity(_document_id, layer_id, clamped) == 0;
}

bool ImmViewerNode::set_layer_transform(int layer_id, const Transform3D &layer_transform)
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready())
    {
        return false;
    }

    PackedFloat32Array matrix = transform_to_matrix_array(layer_transform);
    return ImmGodot_SetLayerTransform(_document_id, layer_id, matrix.ptrw()) == 0;
}

bool ImmViewerNode::clear_layer_transform_override(int layer_id)
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready())
    {
        return false;
    }

    return ImmGodot_ClearLayerTransformOverride(_document_id, layer_id) == 0;
}

Dictionary ImmViewerNode::get_layer_diagnostics(int layer_id) const
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready())
    {
        return Dictionary();
    }

    ImmGodotLayerDiagnostics diagnostics = {};
    if (ImmGodot_GetLayerDiagnostics(_document_id, layer_id, &diagnostics) != 0)
    {
        return Dictionary();
    }

    return layer_diagnostics_to_dictionary(diagnostics);
}

Color ImmViewerNode::get_background_color() const
{
    return _background_color;
}

PackedInt32Array ImmViewerNode::get_spawn_area_ids() const
{
    return _spawn_area_ids;
}

int ImmViewerNode::get_active_spawn_area_index() const
{
    return _active_spawn_area_index;
}

Dictionary ImmViewerNode::get_spawn_area_info(int spawn_area_id) const
{
    if (!_native_initialized || _document_id < 0 || !is_sequence_ready())
    {
        return Dictionary();
    }

    ImmGodotSpawnArea spawn_area = {};
    if (ImmGodot_GetSpawnAreaInfo(_document_id, spawn_area_id, &spawn_area) != 0)
    {
        return Dictionary();
    }

    return spawn_area_to_dictionary(spawn_area_id, spawn_area);
}

Dictionary ImmViewerNode::get_active_spawn_area_info() const
{
    if (_active_spawn_area_index < 0 || _active_spawn_area_index >= _spawn_area_ids.size())
    {
        return Dictionary();
    }

    return get_spawn_area_info(_spawn_area_ids[_active_spawn_area_index]);
}

bool ImmViewerNode::initialize_native_backend()
{
    if (_native_initialized)
    {
        return true;
    }

    ImmGodot_SetDebugLogging(_debug_logging ? 1 : 0);
    ImmGodotRenderAdapter adapter = {};
    adapter.version = 1;
    adapter.userData = this;
    adapter.beforeRenderCamera = &ImmViewerNode::render_adapter_before_camera;
    adapter.afterRenderCamera = &ImmViewerNode::render_adapter_after_camera;
    adapter.onGraphicsInitialized = &ImmViewerNode::render_adapter_graphics_initialized;
    adapter.onGraphicsShutdown = &ImmViewerNode::render_adapter_graphics_shutdown;
    ImmGodot_SetRenderAdapter(&adapter);

    const String log_path = resolve_load_path(_log_file_path);
    const String tmp_path = resolve_load_path(_tmp_folder_path);
    CharString log_utf8 = log_path.utf8();
    CharString tmp_utf8 = tmp_path.utf8();
    const int result = ImmGodot_InitEx(_color_space,
                                       _antialiasing,
                                       const_cast<char *>(log_utf8.get_data()),
                                       const_cast<char *>(tmp_utf8.get_data()),
                                       _renderer_api);
    if (result != 0)
    {
        UtilityFunctions::push_error("ImmViewerNode failed to initialize native IMM backend: ", result);
        emit_signal("native_backend_failed", result);
        return false;
    }

    _native_initialized = true;
    emit_signal("native_backend_initialized");
    return true;
}

void ImmViewerNode::shutdown_native_backend()
{
    if (!_native_initialized)
    {
        return;
    }

    if (_document_id >= 0)
    {
        ImmGodot_Unload(_document_id);
        _document_id = -1;
    }
    ImmGodot_Shutdown();
    ImmGodot_SetRenderAdapter(nullptr);
    _native_initialized = false;
    _is_playing = false;
    _pending_show_after_load = false;
    _sequence_ready_seen = false;
    _spawn_area_ids.clear();
    _active_spawn_area_index = -1;
    _registered_render_camera_ids.clear();
}

void ImmViewerNode::refresh_spawn_areas()
{
    _spawn_area_ids.clear();
    _active_spawn_area_index = -1;

    if (_document_id < 0)
    {
        return;
    }

    const int count = ImmGodot_GetSpawnAreaCount(_document_id);
    if (count <= 0)
    {
        return;
    }

    _spawn_area_ids.resize(count);
    ImmGodot_GetSpawnAreaList(_document_id, count, _spawn_area_ids.ptrw());

    const int active = ImmGodot_GetActiveSpawnAreaId(_document_id);
    for (int i = 0; i < _spawn_area_ids.size(); ++i)
    {
        if (_spawn_area_ids[i] == active)
        {
            _active_spawn_area_index = i;
            break;
        }
    }
}

String ImmViewerNode::resolve_load_path(const String &path) const
{
    if (path.begins_with("res://") || path.begins_with("user://"))
    {
        return ProjectSettings::get_singleton()->globalize_path(path);
    }
    return path;
}

bool ImmViewerNode::validate_matrix_array(const PackedFloat32Array &matrix, const char *name) const
{
    if (matrix.size() == 16)
    {
        return true;
    }

    UtilityFunctions::push_error("ImmViewerNode matrix argument must contain exactly 16 floats: ", name);
    return false;
}

Dictionary ImmViewerNode::bounds_to_dictionary(const ImmGodotBounds3 &bounds) const
{
    const Vector3 min(bounds.minX, bounds.minY, bounds.minZ);
    const Vector3 max(bounds.maxX, bounds.maxY, bounds.maxZ);
    Dictionary result;
    result["min"] = min;
    result["max"] = max;
    result["center"] = (min + max) * 0.5f;
    result["size"] = max - min;
    return result;
}

Dictionary ImmViewerNode::layer_to_dictionary(const ImmGodotLayerInfo &layer) const
{
    Dictionary result;
    result["id"] = layer.id;
    result["type"] = layer.type;
    result["parent_id"] = layer.parentId;
    result["is_timeline"] = layer.isTimeline != 0;
    result["is_loaded"] = layer.isLoaded != 0;
    result["is_visible"] = layer.isVisible != 0;
    result["opacity"] = layer.opacity;
    result["has_bounds"] = layer.hasBounds != 0;
    result["bounds"] = bounds_to_dictionary(layer.bounds);
    result["num_children"] = layer.numChildren;
    result["asset_id"] = layer.assetId;
    result["paint_num_drawings"] = layer.paintNumDrawings;
    result["paint_num_frames"] = layer.paintNumFrames;
    result["paint_num_strokes"] = layer.paintNumStrokes;
    result["name"] = String(layer.name);
    result["full_name"] = String(layer.fullName);
    return result;
}

Dictionary ImmViewerNode::layer_diagnostics_to_dictionary(const ImmGodotLayerDiagnostics &diagnostics) const
{
    Dictionary result;
    result["has_visibility_keys"] = diagnostics.hasVisibilityKeys != 0;
    result["has_opacity_keys"] = diagnostics.hasOpacityKeys != 0;
    result["is_visible"] = diagnostics.isVisible != 0;
    result["opacity"] = diagnostics.opacity;
    result["is_world_visible"] = diagnostics.isWorldVisible != 0;
    result["world_opacity"] = diagnostics.worldOpacity;
    result["parent_id"] = diagnostics.parentId;
    result["visibility_override_enabled"] = diagnostics.visibilityOverrideEnabled != 0;
    result["visibility_override_value"] = diagnostics.visibilityOverrideValue != 0;
    result["has_transform_keys"] = diagnostics.hasTransformKeys != 0;
    result["transform_override_enabled"] = diagnostics.transformOverrideEnabled != 0;
    return result;
}

Dictionary ImmViewerNode::spawn_area_to_dictionary(int spawn_area_id, const ImmGodotSpawnArea &spawn_area) const
{
    const float x = spawn_area.transform.rotx;
    const float y = spawn_area.transform.roty;
    const float z = spawn_area.transform.rotz;
    const float w = spawn_area.transform.rotw;
    const float scale = spawn_area.transform.sca;

    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    const float m00 = (1.0f - 2.0f * (yy + zz)) * scale;
    const float m01 = (2.0f * (xy - wz)) * scale;
    const float m02 = (2.0f * (xz + wy)) * scale;
    const float m10 = (2.0f * (xy + wz)) * scale;
    const float m11 = (1.0f - 2.0f * (xx + zz)) * scale;
    const float m12 = (2.0f * (yz - wx)) * scale;
    const float m20 = (2.0f * (xz - wy)) * scale;
    const float m21 = (2.0f * (yz + wx)) * scale;
    const float m22 = (1.0f - 2.0f * (xx + yy)) * scale;

    Dictionary transform;
    transform["position"] = Vector3(spawn_area.transform.posx,
                                    spawn_area.transform.posy,
                                    -spawn_area.transform.posz);
    transform["basis_x"] = Vector3(m00, m10, -m20);
    transform["basis_y"] = Vector3(m01, m11, -m21);
    transform["basis_z"] = Vector3(-m02, -m12, m22);
    transform["raw_position"] = Vector3(spawn_area.transform.posx,
                                        spawn_area.transform.posy,
                                        spawn_area.transform.posz);
    transform["raw_rotation"] = Vector3(spawn_area.transform.rotx,
                                        spawn_area.transform.roty,
                                        spawn_area.transform.rotz);
    transform["raw_rotation_w"] = spawn_area.transform.rotw;
    transform["scale"] = scale;

    Dictionary volume;
    volume["type"] = static_cast<int>(spawn_area.volume.type);
    volume["offset"] = Vector3(spawn_area.volume.offset.x,
                               spawn_area.volume.offset.y,
                               spawn_area.volume.offset.z);
    volume["sphere_radius"] = spawn_area.volume.sphereExtent.r;
    volume["box_extent"] = Vector3(spawn_area.volume.boxExtent.x,
                                   spawn_area.volume.boxExtent.y,
                                   spawn_area.volume.boxExtent.z);

    Dictionary result;
    result["id"] = spawn_area_id;
    result["name"] = String(spawn_area.name);
    result["version"] = spawn_area.version;
    result["type"] = static_cast<int>(spawn_area.type);
    result["animated"] = spawn_area.animated != 0;
    result["locomotion"] = spawn_area.locomotion;
    result["transform"] = transform;
    result["volume"] = volume;
    return result;
}

PackedFloat32Array ImmViewerNode::transform_to_matrix_array(const Transform3D &transform) const
{
    PackedFloat32Array result;
    result.resize(16);

    // The native C ABI consumes Unity-style column-major Matrix4x4 float arrays.
    result[0] = transform.basis.rows[0].x;
    result[1] = transform.basis.rows[1].x;
    result[2] = transform.basis.rows[2].x;
    result[3] = 0.0f;
    result[4] = transform.basis.rows[0].y;
    result[5] = transform.basis.rows[1].y;
    result[6] = transform.basis.rows[2].y;
    result[7] = 0.0f;
    result[8] = transform.basis.rows[0].z;
    result[9] = transform.basis.rows[1].z;
    result[10] = transform.basis.rows[2].z;
    result[11] = 0.0f;
    result[12] = transform.origin.x;
    result[13] = transform.origin.y;
    result[14] = transform.origin.z;
    result[15] = 1.0f;
    return result;
}

PackedFloat32Array ImmViewerNode::make_perspective_projection(float fov_degrees, float aspect, float z_near, float z_far) const
{
    const float f = 1.0f / std::tan((fov_degrees * 0.017453292519943295769f) * 0.5f);
    const float depth = z_near - z_far;
#if defined(__APPLE__)
    const bool uses_zero_to_one_depth = _renderer_api == ImmGodotRendererApi_Auto ||
                                        _renderer_api == ImmGodotRendererApi_Metal ||
                                        _renderer_api == ImmGodotRendererApi_Vulkan;
#else
    const bool uses_zero_to_one_depth = _renderer_api == ImmGodotRendererApi_Metal ||
                                        _renderer_api == ImmGodotRendererApi_Direct3D ||
                                        _renderer_api == ImmGodotRendererApi_Vulkan;
#endif

    PackedFloat32Array result;
    result.resize(16);
    result[0] = f / aspect;
    result[1] = 0.0f;
    result[2] = 0.0f;
    result[3] = 0.0f;
    result[4] = 0.0f;
    result[5] = f;
    result[6] = 0.0f;
    result[7] = 0.0f;
    result[8] = 0.0f;
    result[9] = 0.0f;
    result[10] = uses_zero_to_one_depth ? z_far / depth : (z_far + z_near) / depth;
    result[11] = -1.0f;
    result[12] = 0.0f;
    result[13] = 0.0f;
    result[14] = uses_zero_to_one_depth ? (z_far * z_near) / depth : (2.0f * z_far * z_near) / depth;
    result[15] = 0.0f;
    return result;
}
