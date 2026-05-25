#pragma once

#include "appImmGodot/src/imm_godot_plugin.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/string.hpp>

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
        void _process(double delta) override;

        void set_document_path(const String &path);
        String get_document_path() const;

        void set_load_on_ready(bool enabled);
        bool get_load_on_ready() const;

        void set_auto_play(bool enabled);
        bool get_auto_play() const;

        void set_volume(float value);
        float get_volume() const;

        int load_document(const String &path = String());
        void unload_document();
        void play();
        void pause();
        void restart();
        void next_chapter();
        void previous_chapter();
        void next_spawn_area();
        void previous_spawn_area();

        bool is_loaded() const;
        bool is_playing() const;
        PackedInt32Array get_spawn_area_ids() const;
        int get_active_spawn_area_index() const;

    private:
        void refresh_spawn_areas();
        String resolve_load_path(const String &path) const;

    private:
        String _document_path;
        bool _load_on_ready = false;
        bool _auto_play = true;
        float _volume = 1.0f;
        int _document_id = -1;
        bool _is_playing = false;
        PackedInt32Array _spawn_area_ids;
        int _active_spawn_area_index = -1;
    };
}
