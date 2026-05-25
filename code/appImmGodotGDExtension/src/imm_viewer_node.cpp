#include "imm_viewer_node.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace
{
    constexpr int kDocumentLoadedState = 2;
}

ImmViewerNode::ImmViewerNode() = default;

ImmViewerNode::~ImmViewerNode() = default;

void ImmViewerNode::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("load_document", "path"), &ImmViewerNode::load_document, String());
    ClassDB::bind_method(D_METHOD("unload_document"), &ImmViewerNode::unload_document);
    ClassDB::bind_method(D_METHOD("play"), &ImmViewerNode::play);
    ClassDB::bind_method(D_METHOD("pause"), &ImmViewerNode::pause);
    ClassDB::bind_method(D_METHOD("restart"), &ImmViewerNode::restart);
    ClassDB::bind_method(D_METHOD("next_chapter"), &ImmViewerNode::next_chapter);
    ClassDB::bind_method(D_METHOD("previous_chapter"), &ImmViewerNode::previous_chapter);
    ClassDB::bind_method(D_METHOD("next_spawn_area"), &ImmViewerNode::next_spawn_area);
    ClassDB::bind_method(D_METHOD("previous_spawn_area"), &ImmViewerNode::previous_spawn_area);
    ClassDB::bind_method(D_METHOD("global_work", "enabled"), &ImmViewerNode::global_work, true);
    ClassDB::bind_method(D_METHOD("set_document_transform", "document_transform"), &ImmViewerNode::set_document_transform);
    ClassDB::bind_method(D_METHOD("set_camera_transform", "camera_transform"), &ImmViewerNode::set_camera_transform);
    ClassDB::bind_method(D_METHOD("set_camera_matrices", "camera_id", "world_to_head", "projection"), &ImmViewerNode::set_camera_matrices);
    ClassDB::bind_method(D_METHOD("render_camera", "camera_id", "viewport_size", "eye_id"), &ImmViewerNode::render_camera);
    ClassDB::bind_method(D_METHOD("is_loaded"), &ImmViewerNode::is_loaded);
    ClassDB::bind_method(D_METHOD("is_playing"), &ImmViewerNode::is_playing);
    ClassDB::bind_method(D_METHOD("get_document_state"), &ImmViewerNode::get_document_state);
    ClassDB::bind_method(D_METHOD("get_bounding_box"), &ImmViewerNode::get_bounding_box);
    ClassDB::bind_method(D_METHOD("get_background_color"), &ImmViewerNode::get_background_color);
    ClassDB::bind_method(D_METHOD("get_spawn_area_ids"), &ImmViewerNode::get_spawn_area_ids);
    ClassDB::bind_method(D_METHOD("get_active_spawn_area_index"), &ImmViewerNode::get_active_spawn_area_index);
    ClassDB::bind_method(D_METHOD("get_active_spawn_area_id"), &ImmViewerNode::get_active_spawn_area_id);
    ClassDB::bind_method(D_METHOD("set_active_spawn_area_index", "active_index"), &ImmViewerNode::set_active_spawn_area_index);
    ClassDB::bind_method(D_METHOD("get_spawn_area_info", "spawn_area_id"), &ImmViewerNode::get_spawn_area_info);
    ClassDB::bind_method(D_METHOD("get_render_diagnostics"), &ImmViewerNode::get_render_diagnostics);

    ClassDB::bind_method(D_METHOD("set_document_path", "path"), &ImmViewerNode::set_document_path);
    ClassDB::bind_method(D_METHOD("get_document_path"), &ImmViewerNode::get_document_path);
    ClassDB::bind_method(D_METHOD("set_load_on_ready", "enabled"), &ImmViewerNode::set_load_on_ready);
    ClassDB::bind_method(D_METHOD("get_load_on_ready"), &ImmViewerNode::get_load_on_ready);
    ClassDB::bind_method(D_METHOD("set_auto_play", "enabled"), &ImmViewerNode::set_auto_play);
    ClassDB::bind_method(D_METHOD("get_auto_play"), &ImmViewerNode::get_auto_play);
    ClassDB::bind_method(D_METHOD("set_volume", "value"), &ImmViewerNode::set_volume);
    ClassDB::bind_method(D_METHOD("get_volume"), &ImmViewerNode::get_volume);
    ClassDB::bind_method(D_METHOD("set_matrix_debug_logging", "enabled"), &ImmViewerNode::set_matrix_debug_logging);
    ClassDB::bind_method(D_METHOD("get_matrix_debug_logging"), &ImmViewerNode::get_matrix_debug_logging);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "document_path"), "set_document_path", "get_document_path");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "load_on_ready"), "set_load_on_ready", "get_load_on_ready");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_play"), "set_auto_play", "get_auto_play");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_volume", "get_volume");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "matrix_debug_logging"), "set_matrix_debug_logging", "get_matrix_debug_logging");

    ADD_SIGNAL(MethodInfo("document_loaded", PropertyInfo(Variant::STRING, "path")));
    ADD_SIGNAL(MethodInfo("document_unloaded"));
    ADD_SIGNAL(MethodInfo("playback_changed", PropertyInfo(Variant::BOOL, "is_playing")));
    ADD_SIGNAL(MethodInfo("spawn_area_changed", PropertyInfo(Variant::INT, "active_index")));
    ADD_SIGNAL(MethodInfo("native_backend_initialized"));
    ADD_SIGNAL(MethodInfo("native_backend_failed"));
}

void ImmViewerNode::_ready()
{
    set_process(true);
    register_render_adapter();
    const String log_path = ProjectSettings::get_singleton()->globalize_path("user://imm_godot_log.txt");
    const String temp_path = ProjectSettings::get_singleton()->globalize_path("user://");
    CharString log_path_utf8 = log_path.utf8();
    CharString temp_path_utf8 = temp_path.utf8();
    if (ImmGodot_Init(0, 8, log_path_utf8.ptrw(), temp_path_utf8.ptrw()) == 0)
    {
        _backend_initialized = true;
        ImmGodot_SetMatrixDebugLogging(_matrix_debug_logging ? 1 : 0);
        emit_signal("native_backend_initialized");
    }
    else
    {
        UtilityFunctions::push_error("ImmViewerNode failed to initialize the IMM Godot backend.");
        emit_signal("native_backend_failed");
    }

    if (_load_on_ready)
    {
        load_document(_document_path);
    }
}

void ImmViewerNode::_exit_tree()
{
    if (_document_id >= 0)
    {
        unload_document();
    }

    if (_backend_initialized)
    {
        ImmGodot_Shutdown();
        _backend_initialized = false;
    }
    unregister_render_adapter();
}

void ImmViewerNode::_process(double)
{
    global_work(true);
}

void ImmViewerNode::global_work(bool enabled)
{
    if (_backend_initialized && _document_id >= 0)
    {
        ImmGodot_GlobalWork(enabled ? 1 : 0);
        if (_play_when_loaded)
        {
            ImmGodotDocumentState state = {};
            ImmGodot_GetDocumentState(_document_id, &state);
            if (state.loadingState == kDocumentLoadedState)
            {
                _play_when_loaded = false;
                ImmGodot_Resume(_document_id);
                ImmGodot_Show(_document_id);
                _is_playing = true;
                emit_signal("playback_changed", true);
            }
        }
    }
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
    if (_document_id >= 0)
    {
        ImmGodot_SetVolume(_document_id, _volume);
    }
}

float ImmViewerNode::get_volume() const
{
    return _volume;
}

void ImmViewerNode::set_matrix_debug_logging(bool enabled)
{
    _matrix_debug_logging = enabled;
    if (_backend_initialized)
    {
        ImmGodot_SetMatrixDebugLogging(_matrix_debug_logging ? 1 : 0);
    }
}

bool ImmViewerNode::get_matrix_debug_logging() const
{
    return _matrix_debug_logging;
}

int ImmViewerNode::load_document(const String &path)
{
    const String resolved = path.is_empty() ? _document_path : path;
    if (resolved.is_empty())
    {
        return -1;
    }
    if (!_backend_initialized)
    {
        UtilityFunctions::push_warning("ImmViewerNode load_document ignored because the native backend is not initialized.");
        return -1;
    }

    _document_path = resolved;
    CharString utf8 = resolve_load_path(resolved).utf8();
    _document_id = ImmGodot_LoadFromFile(utf8.get_data());
    if (_document_id < 0)
    {
        return _document_id;
    }

    ImmGodot_SetVolume(_document_id, _volume);
    if (_has_document_transform)
    {
        apply_document_transform();
    }
    if (_auto_play)
    {
        _play_when_loaded = true;
    }

    refresh_spawn_areas();
    emit_signal("document_loaded", _document_path);
    return _document_id;
}

void ImmViewerNode::unload_document()
{
    if (_document_id < 0)
    {
        return;
    }

    ImmGodot_Unload(_document_id);
    _document_id = -1;
    _is_playing = false;
    _play_when_loaded = false;
    _spawn_area_ids.clear();
    _active_spawn_area_index = -1;
    emit_signal("document_unloaded");
    emit_signal("playback_changed", false);
}

void ImmViewerNode::play()
{
    if (_document_id < 0)
    {
        return;
    }

    ImmGodotDocumentState state = {};
    ImmGodot_GetDocumentState(_document_id, &state);
    if (state.loadingState != kDocumentLoadedState)
    {
        _play_when_loaded = true;
        _is_playing = true;
        emit_signal("playback_changed", true);
        return;
    }

    ImmGodot_Resume(_document_id);
    ImmGodot_Show(_document_id);
    _is_playing = true;
    emit_signal("playback_changed", true);
}

void ImmViewerNode::pause()
{
    if (_document_id < 0)
    {
        return;
    }

    _play_when_loaded = false;
    ImmGodot_Pause(_document_id);
    _is_playing = false;
    emit_signal("playback_changed", false);
}

void ImmViewerNode::restart()
{
    if (_document_id < 0)
    {
        return;
    }

    ImmGodot_Restart(_document_id);
    _is_playing = true;
    emit_signal("playback_changed", true);
}

void ImmViewerNode::next_chapter()
{
    if (_document_id < 0)
    {
        return;
    }

    const int count = ImmGodot_GetChapterCount(_document_id);
    if (count <= 0)
    {
        ImmGodot_SkipForward(_document_id);
        return;
    }

    const int current = ImmGodot_GetCurrentChapter(_document_id);
    ImmGodot_SetChapter(_document_id, (current + 1) % count);
}

void ImmViewerNode::previous_chapter()
{
    if (_document_id < 0)
    {
        return;
    }

    const int count = ImmGodot_GetChapterCount(_document_id);
    if (count <= 0)
    {
        ImmGodot_SkipBack(_document_id);
        return;
    }

    const int current = ImmGodot_GetCurrentChapter(_document_id);
    ImmGodot_SetChapter(_document_id, (current - 1 + count) % count);
}

void ImmViewerNode::next_spawn_area()
{
    if (_spawn_area_ids.is_empty())
    {
        return;
    }

    set_active_spawn_area_index((_active_spawn_area_index + 1) % _spawn_area_ids.size());
}

void ImmViewerNode::previous_spawn_area()
{
    if (_spawn_area_ids.is_empty())
    {
        return;
    }

    set_active_spawn_area_index((_active_spawn_area_index - 1 + _spawn_area_ids.size()) % _spawn_area_ids.size());
}

void ImmViewerNode::set_document_transform(const Transform3D &document_transform)
{
    _document_transform = document_transform;
    _has_document_transform = true;
    apply_document_transform();
}

void ImmViewerNode::set_camera_transform(const Transform3D &camera_transform)
{
    const Transform3D view_transform = camera_transform.affine_inverse();
    float world_to_head[16] = {
        static_cast<float>(view_transform.basis.rows[0].x),
        static_cast<float>(view_transform.basis.rows[1].x),
        static_cast<float>(view_transform.basis.rows[2].x),
        0.0f,
        static_cast<float>(view_transform.basis.rows[0].y),
        static_cast<float>(view_transform.basis.rows[1].y),
        static_cast<float>(view_transform.basis.rows[2].y),
        0.0f,
        static_cast<float>(view_transform.basis.rows[0].z),
        static_cast<float>(view_transform.basis.rows[1].z),
        static_cast<float>(view_transform.basis.rows[2].z),
        0.0f,
        static_cast<float>(view_transform.origin.x),
        static_cast<float>(view_transform.origin.y),
        static_cast<float>(view_transform.origin.z),
        1.0f,
    };

    float projection[16];
    fill_default_camera_matrices(nullptr, projection);

    submit_camera_matrices(0, world_to_head, projection);
}

bool ImmViewerNode::set_camera_matrices(int camera_id, const PackedFloat32Array &world_to_head, const PackedFloat32Array &projection)
{
    if (camera_id < 0 || world_to_head.size() != 16 || projection.size() != 16)
    {
        return false;
    }

    submit_camera_matrices(camera_id, world_to_head.ptr(), projection.ptr());
    return true;
}

int ImmViewerNode::render_camera(int camera_id, const Vector2i &viewport_size, int eye_id)
{
    if (!_backend_initialized)
    {
        return -1;
    }
    if (viewport_size.x <= 0 || viewport_size.y <= 0)
    {
        return -1;
    }

    if (_has_last_matrices)
    {
        submit_camera_matrices(camera_id, _last_world_to_head, _last_projection);
    }
    else
    {
        float world_to_head[16];
        float projection[16];
        fill_default_camera_matrices(world_to_head, projection);
        submit_camera_matrices(camera_id, world_to_head, projection);
    }

    return ImmGodot_RenderCamera(camera_id,
                                 eye_id,
                                 0.0f,
                                 0.0f,
                                 static_cast<float>(viewport_size.x),
                                 static_cast<float>(viewport_size.y),
                                 0.0f,
                                 1.0f);
}

bool ImmViewerNode::is_loaded() const
{
    return _document_id >= 0;
}

bool ImmViewerNode::is_playing() const
{
    return _is_playing;
}

Dictionary ImmViewerNode::get_document_state() const
{
    Dictionary result;
    result["loading_state"] = _document_id >= 0 ? 2 : 0;
    result["playback_state"] = _is_playing ? 0 : 1;
    if (_document_id >= 0)
    {
        ImmGodotDocumentState state = {};
        ImmGodot_GetDocumentState(_document_id, &state);
        result["loading_state"] = state.loadingState;
        result["playback_state"] = state.playbackState;
    }
    return result;
}

Dictionary ImmViewerNode::get_bounding_box() const
{
    Dictionary result;
    result["valid"] = false;
    result["min"] = Vector3();
    result["max"] = Vector3();
    result["center"] = Vector3();
    result["size"] = Vector3();
    if (_document_id < 0)
    {
        return result;
    }

    ImmGodotBounds bounds = {};
    ImmGodot_GetBoundingBox(_document_id, &bounds);
    const bool valid = bounds.minX <= bounds.maxX &&
                       bounds.minY <= bounds.maxY &&
                       bounds.minZ <= bounds.maxZ &&
                       bounds.minX > -1.0e20f &&
                       bounds.maxX < 1.0e20f &&
                       bounds.minY > -1.0e20f &&
                       bounds.maxY < 1.0e20f &&
                       bounds.minZ > -1.0e20f &&
                       bounds.maxZ < 1.0e20f;
    const Vector3 min(bounds.minX, bounds.minY, bounds.minZ);
    const Vector3 max(bounds.maxX, bounds.maxY, bounds.maxZ);
    result["valid"] = valid;
    result["min"] = min;
    result["max"] = max;
    result["center"] = (min + max) * 0.5;
    result["size"] = max - min;
    return result;
}

Color ImmViewerNode::get_background_color() const
{
    if (!_backend_initialized)
    {
        return Color(0.0f, 0.0f, 0.0f, 1.0f);
    }

    ImmGodotPlayerInfo info = {};
    ImmGodot_GetPlayerInfo(&info);
    return Color(info.backgroundColor.red, info.backgroundColor.green, info.backgroundColor.blue, 1.0f);
}

PackedInt32Array ImmViewerNode::get_spawn_area_ids()
{
    refresh_spawn_areas();
    return _spawn_area_ids;
}

int ImmViewerNode::get_active_spawn_area_index() const
{
    return _active_spawn_area_index;
}

int ImmViewerNode::get_active_spawn_area_id() const
{
    if (_active_spawn_area_index < 0 || _active_spawn_area_index >= _spawn_area_ids.size())
    {
        return -1;
    }
    return _spawn_area_ids[_active_spawn_area_index];
}

bool ImmViewerNode::set_active_spawn_area_index(int active_index)
{
    if (_document_id < 0 || active_index < 0 || active_index >= _spawn_area_ids.size())
    {
        return false;
    }

    _active_spawn_area_index = active_index;
    ImmGodot_SetActiveSpawnAreaId(_document_id, _spawn_area_ids[_active_spawn_area_index]);
    emit_signal("spawn_area_changed", _active_spawn_area_index);
    return true;
}

Dictionary ImmViewerNode::get_spawn_area_info(int spawn_area_id) const
{
    Dictionary result;
    result["valid"] = false;
    result["id"] = spawn_area_id;
    if (_document_id < 0)
    {
        return result;
    }

    ImmGodotSpawnArea info = {};
    if (!ImmGodot_GetSpawnAreaInfo(_document_id, spawn_area_id, &info))
    {
        return result;
    }

    Dictionary volume;
    volume["type"] = static_cast<int>(info.volume.type);
    volume["offset"] = Vector3(info.volume.offset.x, info.volume.offset.y, info.volume.offset.z);
    volume["sphere_radius"] = info.volume.sphereExtent.r;
    volume["box_extent"] = Vector3(info.volume.boxExtent.x, info.volume.boxExtent.y, info.volume.boxExtent.z);

    Dictionary transform;
    transform["position"] = Vector3(info.transform.posx, info.transform.posy, info.transform.posz);
    transform["rotation"] = Quaternion(info.transform.rotx, info.transform.roty, info.transform.rotz, info.transform.rotw);
    transform["scale"] = info.transform.sca;

    result["valid"] = true;
    result["id"] = spawn_area_id;
    result["name"] = String(info.name);
    result["version"] = info.version;
    result["type"] = static_cast<int>(info.type);
    result["animated"] = info.animated != 0;
    result["volume"] = volume;
    result["transform"] = transform;
    result["locomotion"] = info.locomotion;
    return result;
}

Dictionary ImmViewerNode::get_render_diagnostics() const
{
    Dictionary result;
    result["backend_initialized"] = _backend_initialized;
    result["graphics_initialized_count"] = _graphics_initialized_count;
    result["graphics_shutdown_count"] = _graphics_shutdown_count;
    result["before_render_count"] = _before_render_count;
    result["after_render_count"] = _after_render_count;
    result["last_render_camera_id"] = _last_render_camera_id;
    result["last_render_eye_id"] = _last_render_eye_id;
    result["last_render_result"] = _last_render_result;
    result["last_viewport_x"] = _last_render_viewport.x;
    result["last_viewport_y"] = _last_render_viewport.y;
    result["last_viewport_width"] = _last_render_viewport.width;
    result["last_viewport_height"] = _last_render_viewport.height;
    result["last_viewport_min_depth"] = _last_render_viewport.minDepth;
    result["last_viewport_max_depth"] = _last_render_viewport.maxDepth;
    const Dictionary document_state = get_document_state();
    result["document_loading_state"] = document_state["loading_state"];
    result["document_playback_state"] = document_state["playback_state"];
    const Dictionary bounding_box = get_bounding_box();
    result["bounding_box_valid"] = bounding_box["valid"];
    result["bounding_box_min"] = bounding_box["min"];
    result["bounding_box_max"] = bounding_box["max"];
    result["background_color"] = get_background_color();
    result["spawn_area_count"] = _spawn_area_ids.size();
    result["active_spawn_area_index"] = _active_spawn_area_index;
    result["active_spawn_area_id"] = get_active_spawn_area_id();
    result["matrix_debug_logging"] = _matrix_debug_logging;
    result["has_document_transform"] = _has_document_transform;
    result["last_document_to_world"] = copy_matrix(_last_document_to_world);
    result["has_last_matrices"] = _has_last_matrices;
    result["last_matrix_camera_id"] = _last_matrix_camera_id;
    result["last_world_to_head"] = copy_matrix(_last_world_to_head);
    result["last_projection"] = copy_matrix(_last_projection);
    return result;
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
    if (!path.is_absolute_path())
    {
        return ProjectSettings::get_singleton()->globalize_path("res://" + path);
    }
    return path;
}

void ImmViewerNode::register_render_adapter()
{
    _render_adapter = {};
    _render_adapter.userData = this;
    _render_adapter.graphicsInitialized = &ImmViewerNode::render_adapter_graphics_initialized;
    _render_adapter.graphicsShutdown = &ImmViewerNode::render_adapter_graphics_shutdown;
    _render_adapter.beforeRenderCamera = &ImmViewerNode::render_adapter_before_camera;
    _render_adapter.afterRenderCamera = &ImmViewerNode::render_adapter_after_camera;
    ImmGodot_SetRenderAdapter(&_render_adapter);
}

void ImmViewerNode::unregister_render_adapter()
{
    ImmGodot_ClearRenderAdapter();
    _render_adapter = {};
}

void ImmViewerNode::apply_document_transform()
{
    _last_document_to_world[0] = static_cast<float>(_document_transform.basis.rows[0].x);
    _last_document_to_world[1] = static_cast<float>(_document_transform.basis.rows[1].x);
    _last_document_to_world[2] = static_cast<float>(_document_transform.basis.rows[2].x);
    _last_document_to_world[3] = 0.0f;
    _last_document_to_world[4] = static_cast<float>(_document_transform.basis.rows[0].y);
    _last_document_to_world[5] = static_cast<float>(_document_transform.basis.rows[1].y);
    _last_document_to_world[6] = static_cast<float>(_document_transform.basis.rows[2].y);
    _last_document_to_world[7] = 0.0f;
    _last_document_to_world[8] = static_cast<float>(_document_transform.basis.rows[0].z);
    _last_document_to_world[9] = static_cast<float>(_document_transform.basis.rows[1].z);
    _last_document_to_world[10] = static_cast<float>(_document_transform.basis.rows[2].z);
    _last_document_to_world[11] = 0.0f;
    _last_document_to_world[12] = static_cast<float>(_document_transform.origin.x);
    _last_document_to_world[13] = static_cast<float>(_document_transform.origin.y);
    _last_document_to_world[14] = static_cast<float>(_document_transform.origin.z);
    _last_document_to_world[15] = 1.0f;

    if (_document_id >= 0)
    {
        ImmGodot_SetDocumentToWorld(_document_id, _last_document_to_world);
    }
}

void ImmViewerNode::submit_camera_matrices(int camera_id, const float *world_to_head, const float *projection)
{
    _last_matrix_camera_id = camera_id;
    for (int i = 0; i < 16; ++i)
    {
        _last_world_to_head[i] = world_to_head[i];
        _last_projection[i] = projection[i];
    }
    _has_last_matrices = true;
    ImmGodot_SetCameraMatrices(camera_id, 0, _last_world_to_head, _last_projection, nullptr, nullptr, nullptr, nullptr);
}

void ImmViewerNode::fill_default_camera_matrices(float *world_to_head, float *projection) const
{
    if (world_to_head != nullptr)
    {
        const float identity[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        for (int i = 0; i < 16; ++i)
        {
            world_to_head[i] = identity[i];
        }
    }

    if (projection != nullptr)
    {
        const float default_projection[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, -1.0f,
            0.0f, 0.0f, -0.1f, 0.0f,
        };
        for (int i = 0; i < 16; ++i)
        {
            projection[i] = default_projection[i];
        }
    }
}

PackedFloat32Array ImmViewerNode::copy_matrix(const float *matrix) const
{
    PackedFloat32Array result;
    result.resize(16);
    for (int i = 0; i < 16; ++i)
    {
        result.set(i, matrix[i]);
    }
    return result;
}

void ImmViewerNode::render_adapter_graphics_initialized(void *user_data)
{
    ImmViewerNode *self = static_cast<ImmViewerNode *>(user_data);
    if (self != nullptr)
    {
        self->_graphics_initialized_count += 1;
    }
}

void ImmViewerNode::render_adapter_graphics_shutdown(void *user_data)
{
    ImmViewerNode *self = static_cast<ImmViewerNode *>(user_data);
    if (self != nullptr)
    {
        self->_graphics_shutdown_count += 1;
    }
}

int ImmViewerNode::render_adapter_before_camera(void *user_data, int camera_id, int eye_id, const ImmGodotViewport *viewport)
{
    ImmViewerNode *self = static_cast<ImmViewerNode *>(user_data);
    if (self == nullptr)
    {
        return -1;
    }

    self->_before_render_count += 1;
    self->_last_render_camera_id = camera_id;
    self->_last_render_eye_id = eye_id;
    if (viewport != nullptr)
    {
        self->_last_render_viewport = *viewport;
    }
    return 0;
}

void ImmViewerNode::render_adapter_after_camera(void *user_data, int camera_id, int eye_id, const ImmGodotViewport *viewport, int render_result)
{
    ImmViewerNode *self = static_cast<ImmViewerNode *>(user_data);
    if (self == nullptr)
    {
        return;
    }

    self->_after_render_count += 1;
    self->_last_render_camera_id = camera_id;
    self->_last_render_eye_id = eye_id;
    self->_last_render_result = render_result;
    if (viewport != nullptr)
    {
        self->_last_render_viewport = *viewport;
    }
}
