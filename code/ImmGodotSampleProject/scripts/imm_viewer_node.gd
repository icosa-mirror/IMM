extends Node

signal document_loaded(path: String)
signal document_unloaded()
signal playback_changed(is_playing: bool)
signal spawn_area_changed(active_index: int)

@export var document_path: String = "../../exampleImmFiles/sample1.imm"
@export var load_on_ready: bool = false
@export var auto_play: bool = true
@export_range(0.0, 1.0, 0.01) var volume: float = 1.0
@export var matrix_debug_logging: bool = false

var _is_loaded := false
var _is_playing := false
var _active_spawn_area_index := -1
var _spawn_area_ids := PackedInt32Array()
var _render_diagnostics := {
    "backend_initialized": false,
    "graphics_initialized_count": 0,
    "graphics_shutdown_count": 0,
    "before_render_count": 0,
    "after_render_count": 0,
    "last_render_camera_id": -1,
    "last_render_eye_id": -1,
    "last_render_result": 0,
    "last_viewport_width": 0.0,
    "last_viewport_height": 0.0,
    "document_loading_state": 0,
    "document_playback_state": 1,
    "bounding_box_valid": false,
    "bounding_box_min": Vector3.ZERO,
    "bounding_box_max": Vector3.ZERO,
    "background_color": Color(0.04, 0.05, 0.08, 1.0),
    "spawn_area_count": 0,
    "active_spawn_area_index": -1,
    "active_spawn_area_id": -1,
    "matrix_debug_logging": false,
    "has_document_transform": false,
    "last_document_to_world": PackedFloat32Array(),
    "has_last_matrices": false,
    "last_matrix_camera_id": -1,
    "last_world_to_head": PackedFloat32Array(),
    "last_projection": PackedFloat32Array(),
}

func set_document_path(path: String) -> void:
    document_path = path

func get_document_path() -> String:
    return document_path

func _ready() -> void:
    _render_diagnostics["backend_initialized"] = true
    _render_diagnostics["graphics_initialized_count"] = 1
    if load_on_ready:
        load_document(document_path)

func load_document(path: String = document_path) -> void:
    document_path = path
    _is_loaded = true
    _is_playing = auto_play
    _update_document_state_diagnostics()
    _spawn_area_ids = PackedInt32Array()
    _active_spawn_area_index = -1
    _update_spawn_area_diagnostics()
    push_warning("ImmViewerNode is running in script stub mode. Native GDExtension wiring is still pending.")
    document_loaded.emit(document_path)
    playback_changed.emit(_is_playing)

func unload_document() -> void:
    if not _is_loaded:
        return
    _is_loaded = false
    _is_playing = false
    _update_document_state_diagnostics()
    _spawn_area_ids.clear()
    _active_spawn_area_index = -1
    _update_spawn_area_diagnostics()
    document_unloaded.emit()
    playback_changed.emit(false)

func play() -> void:
    if not _is_loaded:
        return
    _is_playing = true
    _update_document_state_diagnostics()
    playback_changed.emit(true)

func pause() -> void:
    if not _is_loaded:
        return
    _is_playing = false
    _update_document_state_diagnostics()
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
    _update_document_state_diagnostics()
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

func global_work(_enabled: bool = true) -> void:
    pass

func set_document_transform(document_transform: Transform3D) -> void:
    _render_diagnostics["has_document_transform"] = true
    _render_diagnostics["last_document_to_world"] = PackedFloat32Array([
        document_transform.basis.x.x, document_transform.basis.x.y, document_transform.basis.x.z, 0.0,
        document_transform.basis.y.x, document_transform.basis.y.y, document_transform.basis.y.z, 0.0,
        document_transform.basis.z.x, document_transform.basis.z.y, document_transform.basis.z.z, 0.0,
        document_transform.origin.x, document_transform.origin.y, document_transform.origin.z, 1.0,
    ])

func set_camera_transform(_camera_transform: Transform3D) -> void:
    _record_camera_matrices(0)

func set_camera_matrices(camera_id: int, world_to_head: PackedFloat32Array, projection: PackedFloat32Array) -> bool:
    if camera_id < 0 or world_to_head.size() != 16 or projection.size() != 16:
        return false
    _render_diagnostics["has_last_matrices"] = true
    _render_diagnostics["last_matrix_camera_id"] = camera_id
    _render_diagnostics["last_world_to_head"] = world_to_head.duplicate()
    _render_diagnostics["last_projection"] = projection.duplicate()
    return true

func render_camera(camera_id: int, viewport_size: Vector2i, eye_id: int = 0) -> int:
    if _render_diagnostics["has_last_matrices"]:
        _render_diagnostics["last_matrix_camera_id"] = camera_id
    else:
        _record_camera_matrices(camera_id)
    _render_diagnostics["before_render_count"] += 1
    _render_diagnostics["last_render_camera_id"] = camera_id
    _render_diagnostics["last_render_eye_id"] = eye_id
    _render_diagnostics["last_viewport_width"] = float(viewport_size.x)
    _render_diagnostics["last_viewport_height"] = float(viewport_size.y)
    _render_diagnostics["last_render_result"] = 0
    _render_diagnostics["after_render_count"] += 1
    return 0

func get_render_diagnostics() -> Dictionary:
    return _render_diagnostics.duplicate()

func set_matrix_debug_logging(enabled: bool) -> void:
    matrix_debug_logging = enabled
    _render_diagnostics["matrix_debug_logging"] = enabled

func get_matrix_debug_logging() -> bool:
    return matrix_debug_logging

func is_loaded() -> bool:
    return _is_loaded

func is_playing() -> bool:
    return _is_playing

func get_document_state() -> Dictionary:
    return {
        "loading_state": _render_diagnostics["document_loading_state"],
        "playback_state": _render_diagnostics["document_playback_state"],
    }

func get_bounding_box() -> Dictionary:
    var bounds_min: Vector3 = _render_diagnostics["bounding_box_min"]
    var bounds_max: Vector3 = _render_diagnostics["bounding_box_max"]
    return {
        "valid": _render_diagnostics["bounding_box_valid"],
        "min": bounds_min,
        "max": bounds_max,
        "center": (bounds_min + bounds_max) * 0.5,
        "size": bounds_max - bounds_min,
    }

func get_background_color() -> Color:
    return _render_diagnostics["background_color"]

func get_spawn_area_ids() -> PackedInt32Array:
    return _spawn_area_ids.duplicate()

func get_active_spawn_area_index() -> int:
    return _active_spawn_area_index

func get_active_spawn_area_id() -> int:
    if _active_spawn_area_index < 0 or _active_spawn_area_index >= _spawn_area_ids.size():
        return -1
    return _spawn_area_ids[_active_spawn_area_index]

func set_active_spawn_area_index(active_index: int) -> bool:
    if active_index < 0 or active_index >= _spawn_area_ids.size():
        return false
    _active_spawn_area_index = active_index
    _update_spawn_area_diagnostics()
    spawn_area_changed.emit(_active_spawn_area_index)
    return true

func get_spawn_area_info(spawn_area_id: int) -> Dictionary:
    return {
        "valid": _spawn_area_ids.has(spawn_area_id),
        "id": spawn_area_id,
        "name": "",
        "version": 0,
        "type": 0,
        "animated": false,
        "volume": {
            "type": 0,
            "offset": Vector3.ZERO,
            "sphere_radius": 0.0,
            "box_extent": Vector3.ZERO,
        },
        "transform": {
            "position": Vector3.ZERO,
            "rotation": Quaternion.IDENTITY,
            "scale": 1.0,
        },
        "locomotion": 0,
    }

func _cycle_spawn_area(offset: int) -> void:
    if _spawn_area_ids.is_empty():
        push_warning("No spawn areas available yet. Native bridge hookup will populate them.")
        return

    set_active_spawn_area_index(posmod(_active_spawn_area_index + offset, _spawn_area_ids.size()))

func _update_spawn_area_diagnostics() -> void:
    _render_diagnostics["spawn_area_count"] = _spawn_area_ids.size()
    _render_diagnostics["active_spawn_area_index"] = _active_spawn_area_index
    _render_diagnostics["active_spawn_area_id"] = get_active_spawn_area_id()

func _update_document_state_diagnostics() -> void:
    _render_diagnostics["document_loading_state"] = 2 if _is_loaded else 0
    _render_diagnostics["document_playback_state"] = 0 if _is_playing else 1

func _record_camera_matrices(camera_id: int) -> void:
    _render_diagnostics["has_last_matrices"] = true
    _render_diagnostics["last_matrix_camera_id"] = camera_id
    _render_diagnostics["last_world_to_head"] = PackedFloat32Array([
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    ])
    _render_diagnostics["last_projection"] = PackedFloat32Array([
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, -1.0, -1.0,
        0.0, 0.0, -0.1, 0.0,
    ])
