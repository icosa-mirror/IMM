#include "imm_viewer_node.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

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
    ClassDB::bind_method(D_METHOD("is_loaded"), &ImmViewerNode::is_loaded);
    ClassDB::bind_method(D_METHOD("is_playing"), &ImmViewerNode::is_playing);
    ClassDB::bind_method(D_METHOD("get_spawn_area_ids"), &ImmViewerNode::get_spawn_area_ids);
    ClassDB::bind_method(D_METHOD("get_active_spawn_area_index"), &ImmViewerNode::get_active_spawn_area_index);

    ClassDB::bind_method(D_METHOD("set_document_path", "path"), &ImmViewerNode::set_document_path);
    ClassDB::bind_method(D_METHOD("get_document_path"), &ImmViewerNode::get_document_path);
    ClassDB::bind_method(D_METHOD("set_load_on_ready", "enabled"), &ImmViewerNode::set_load_on_ready);
    ClassDB::bind_method(D_METHOD("get_load_on_ready"), &ImmViewerNode::get_load_on_ready);
    ClassDB::bind_method(D_METHOD("set_auto_play", "enabled"), &ImmViewerNode::set_auto_play);
    ClassDB::bind_method(D_METHOD("get_auto_play"), &ImmViewerNode::get_auto_play);
    ClassDB::bind_method(D_METHOD("set_volume", "value"), &ImmViewerNode::set_volume);
    ClassDB::bind_method(D_METHOD("get_volume"), &ImmViewerNode::get_volume);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "document_path"), "set_document_path", "get_document_path");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "load_on_ready"), "set_load_on_ready", "get_load_on_ready");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_play"), "set_auto_play", "get_auto_play");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_volume", "get_volume");

    ADD_SIGNAL(MethodInfo("document_loaded", PropertyInfo(Variant::STRING, "path")));
    ADD_SIGNAL(MethodInfo("document_unloaded"));
    ADD_SIGNAL(MethodInfo("playback_changed", PropertyInfo(Variant::BOOL, "is_playing")));
    ADD_SIGNAL(MethodInfo("spawn_area_changed", PropertyInfo(Variant::INT, "active_index")));
}

void ImmViewerNode::_ready()
{
    UtilityFunctions::push_warning("ImmViewerNode GDExtension scaffold is present, but render-thread integration is not implemented yet.");
    if (_load_on_ready)
    {
        load_document(_document_path);
    }
}

void ImmViewerNode::_process(double)
{
    if (_document_id >= 0)
    {
        ImmGodot_GlobalWork(1);
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

int ImmViewerNode::load_document(const String &path)
{
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
    if (_auto_play)
    {
        play();
        ImmGodot_Show(_document_id);
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

    _active_spawn_area_index = (_active_spawn_area_index + 1) % _spawn_area_ids.size();
    ImmGodot_SetActiveSpawnAreaId(_document_id, _spawn_area_ids[_active_spawn_area_index]);
    emit_signal("spawn_area_changed", _active_spawn_area_index);
}

void ImmViewerNode::previous_spawn_area()
{
    if (_spawn_area_ids.is_empty())
    {
        return;
    }

    _active_spawn_area_index = (_active_spawn_area_index - 1 + _spawn_area_ids.size()) % _spawn_area_ids.size();
    ImmGodot_SetActiveSpawnAreaId(_document_id, _spawn_area_ids[_active_spawn_area_index]);
    emit_signal("spawn_area_changed", _active_spawn_area_index);
}

bool ImmViewerNode::is_loaded() const
{
    return _document_id >= 0;
}

bool ImmViewerNode::is_playing() const
{
    return _is_playing;
}

PackedInt32Array ImmViewerNode::get_spawn_area_ids() const
{
    return _spawn_area_ids;
}

int ImmViewerNode::get_active_spawn_area_index() const
{
    return _active_spawn_area_index;
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
