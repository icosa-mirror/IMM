extends Node
class_name ImmViewerNode

signal document_loaded(path: String)
signal document_unloaded()
signal playback_changed(is_playing: bool)
signal spawn_area_changed(active_index: int)

@export var document_path: String = "../../../exampleImmFiles/sample1.imm"
@export var load_on_ready: bool = false
@export var auto_play: bool = true
@export_range(0.0, 1.0, 0.01) var volume: float = 1.0

var _is_loaded := false
var _is_playing := false
var _active_spawn_area_index := -1
var _spawn_area_ids: Array[int] = []

func _ready() -> void:
    if load_on_ready:
        load_document(document_path)

func load_document(path: String = document_path) -> void:
    document_path = path
    _is_loaded = true
    _is_playing = auto_play
    _spawn_area_ids = []
    _active_spawn_area_index = -1
    push_warning("ImmViewerNode is running in script stub mode. Native GDExtension wiring is still pending.")
    document_loaded.emit(document_path)
    playback_changed.emit(_is_playing)

func unload_document() -> void:
    if not _is_loaded:
        return
    _is_loaded = false
    _is_playing = false
    _spawn_area_ids.clear()
    _active_spawn_area_index = -1
    document_unloaded.emit()
    playback_changed.emit(false)

func play() -> void:
    if not _is_loaded:
        return
    _is_playing = true
    playback_changed.emit(true)

func pause() -> void:
    if not _is_loaded:
        return
    _is_playing = false
    playback_changed.emit(false)

func toggle_pause() -> void:
    if _is_playing:
        pause()
    else:
        play()

func restart() -> void:
    if not _is_loaded:
        return
    _is_playing = true
    playback_changed.emit(true)

func next_chapter() -> void:
    if _is_loaded:
        push_warning("Chapter navigation is not connected until the native Godot backend is bound.")

func previous_chapter() -> void:
    if _is_loaded:
        push_warning("Chapter navigation is not connected until the native Godot backend is bound.")

func next_spawn_area() -> void:
    _cycle_spawn_area(1)

func previous_spawn_area() -> void:
    _cycle_spawn_area(-1)

func set_document_transform(_document_transform: Transform3D) -> void:
    pass

func set_camera_transform(_camera_transform: Transform3D) -> void:
    pass

func is_loaded() -> bool:
    return _is_loaded

func is_playing() -> bool:
    return _is_playing

func get_spawn_area_ids() -> Array[int]:
    return _spawn_area_ids.duplicate()

func get_active_spawn_area_index() -> int:
    return _active_spawn_area_index

func _cycle_spawn_area(offset: int) -> void:
    if _spawn_area_ids.is_empty():
        push_warning("No spawn areas available yet. Native bridge hookup will populate them.")
        return

    _active_spawn_area_index = posmod(_active_spawn_area_index + offset, _spawn_area_ids.size())
    spawn_area_changed.emit(_active_spawn_area_index)
