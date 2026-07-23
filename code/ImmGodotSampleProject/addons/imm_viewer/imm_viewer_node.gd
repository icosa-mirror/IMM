extends Node

const IMM_TICKS_PER_SECOND := 12600

signal document_loaded(path: String)
signal document_unloaded()
signal playback_changed(is_playing: bool)
signal spawn_area_changed(active_index: int)
signal native_backend_initialized()
signal native_backend_failed(error_code: int)

@export var document_path: String = "res://../../exampleImmFiles/sample1.imm"
@export var load_on_ready: bool = false
@export var auto_play: bool = true
@export_range(0.0, 1.0, 0.01) var volume: float = 1.0
@export var debug_logging: bool = false
@export_enum("Linear", "Gamma") var color_space: int = 0
@export_enum("Auto", "OpenGL", "Direct3D", "GLES", "Metal", "Vulkan") var renderer_api: int = 0
@export_range(1, 16, 1) var antialiasing: int = 8
@export var log_file_path: String = "user://imm_godot_log.txt"
@export var tmp_folder_path: String = "user://"
@export var smoke_camera_id: int = 0
@export var smoke_viewport_width: int = 1280
@export var smoke_viewport_height: int = 720
@export var auto_queue_render: bool = false
@export_node_path("Camera3D") var render_camera_path: NodePath

var _is_loaded := false
var _is_playing := false
var _time_since_start := 0
var _time_since_stop := 0
var _background_color := Color.BLACK
var _active_spawn_area_index := -1
var _spawn_area_ids: Array[int] = []
var _document_transform := Transform3D.IDENTITY
var _last_camera_transform := Transform3D.IDENTITY
var _last_camera_projection := PackedFloat32Array()
var _last_render_camera_id := 0
var _last_render_viewport_width := 1280
var _last_render_viewport_height := 720
var _registered_render_camera_ids: Array[int] = []
var _adapter_before_render_count := 0
var _adapter_after_render_count := 0
var _adapter_graphics_initialized_count := 0
var _adapter_graphics_shutdown_count := 0
var _adapter_last_camera_id := -1
var _adapter_last_eye_id := -1
var _adapter_last_render_result := 0
var _adapter_last_viewport_width := 0
var _adapter_last_viewport_height := 0

func _ready() -> void:
    _adapter_graphics_initialized_count += 1
    if auto_queue_render:
        register_render_camera(smoke_camera_id)
    if load_on_ready:
        load_document(document_path)

func _exit_tree() -> void:
    if auto_queue_render:
        unregister_render_camera(smoke_camera_id)
    _adapter_graphics_shutdown_count += 1

func _process(_delta: float) -> void:
    _update_auto_render_camera()

func load_document(path: String = document_path) -> int:
    document_path = path
    _is_loaded = true
    _is_playing = auto_play
    _time_since_start = 0
    _time_since_stop = 0
    _background_color = Color.BLACK
    _spawn_area_ids = []
    _active_spawn_area_index = -1
    push_warning("ImmViewerNode is running in script stub mode. Native GDExtension wiring is still pending.")
    document_loaded.emit(document_path)
    playback_changed.emit(_is_playing)
    return 0

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
    _time_since_start = 0
    _time_since_stop = 0
    _is_playing = true
    playback_changed.emit(true)

func skip_forward() -> void:
    seek_relative_seconds(1.0)

func skip_back() -> void:
    seek_relative_seconds(-1.0)

func set_volume(value: float) -> void:
    volume = clampf(value, 0.0, 1.0)

func get_volume() -> float:
    return volume

func next_chapter() -> void:
    if _is_loaded and get_chapter_count() > 0:
        set_chapter((get_current_chapter() + 1) % get_chapter_count())
    elif _is_loaded:
        skip_forward()

func previous_chapter() -> void:
    if _is_loaded and get_chapter_count() > 0:
        set_chapter(posmod(get_current_chapter() - 1, get_chapter_count()))
    elif _is_loaded:
        skip_back()

func set_chapter(_chapter_index: int) -> void:
    if _is_loaded:
        push_warning("Direct chapter selection is not connected until the native Godot backend is bound.")

func get_chapter_count() -> int:
    return 0

func get_current_chapter() -> int:
    return 0

func set_time(time_since_start: int, time_since_stop: int) -> void:
    if not _is_loaded:
        return
    _time_since_start = max(time_since_start, 0)
    _time_since_stop = max(time_since_stop, 0)

func get_time() -> Dictionary:
    return {
        "time_since_start": _time_since_start,
        "time_since_stop": _time_since_stop,
        "play_time": _time_since_start,
        "play_time_seconds": get_play_time_seconds(),
    }

func get_play_time() -> int:
    return _time_since_start if _is_loaded else 0

func get_play_time_seconds() -> float:
    return float(get_play_time()) / float(IMM_TICKS_PER_SECOND)

func seek_relative_seconds(seconds: float) -> void:
    if not _is_loaded:
        return
    set_time(_time_since_start + int(seconds * IMM_TICKS_PER_SECOND), 0)

func next_spawn_area() -> void:
    _cycle_spawn_area(1)

func previous_spawn_area() -> void:
    _cycle_spawn_area(-1)

func submit_mono_camera_matrices(camera_id: int, world_to_camera: PackedFloat32Array, projection: PackedFloat32Array) -> bool:
    if world_to_camera.size() != 16 or projection.size() != 16:
        push_error("IMM matrix smoke inputs must contain exactly 16 floats.")
        return false
    if debug_logging:
        print("IMM stub submit_mono_camera_matrices camera=%d" % camera_id)
    return true

func smoke_render_camera(camera_id: int, width: int, height: int) -> int:
    if width <= 0 or height <= 0:
        return -1
    _adapter_before_render_count += 1
    _adapter_last_camera_id = camera_id
    _adapter_last_eye_id = 0
    _adapter_last_viewport_width = width
    _adapter_last_viewport_height = height
    if debug_logging:
        print("IMM stub smoke_render_camera camera=%d viewport=%dx%d" % [camera_id, width, height])
    _adapter_after_render_count += 1
    _adapter_last_render_result = 0
    return 0

func set_document_transform(document_transform: Transform3D) -> void:
    _document_transform = document_transform
    if debug_logging:
        print("IMM stub set_document_transform origin=%s" % _document_transform.origin)

func get_document_transform() -> Transform3D:
    return _document_transform

func set_camera_transform(camera_transform: Transform3D) -> void:
    _last_camera_transform = camera_transform
    _last_camera_projection = make_perspective_projection(70.0, float(smoke_viewport_width) / float(smoke_viewport_height), 0.05, 1000.0)
    submit_mono_camera_matrices(smoke_camera_id, transform_to_matrix_array(camera_transform.affine_inverse()), _last_camera_projection)

func smoke_render_last_camera() -> int:
    if _last_camera_projection.is_empty():
        set_camera_transform(_last_camera_transform)
    return smoke_render_camera(smoke_camera_id, smoke_viewport_width, smoke_viewport_height)

func queue_render_last_camera() -> int:
    if _last_camera_projection.is_empty():
        set_camera_transform(_last_camera_transform)
    _last_render_camera_id = smoke_camera_id
    _last_render_viewport_width = smoke_viewport_width
    _last_render_viewport_height = smoke_viewport_height
    return smoke_render_last_camera()

func queue_render_camera_transform(camera_transform: Transform3D, width: int, height: int, fov_degrees: float, camera_id: int) -> int:
    if width <= 0 or height <= 0:
        return -1
    if not is_render_camera_registered(camera_id):
        return 1
    _last_camera_transform = camera_transform
    _last_render_camera_id = camera_id
    _last_render_viewport_width = width
    _last_render_viewport_height = height
    _last_camera_projection = make_perspective_projection(fov_degrees, float(width) / float(height), 0.05, 1000.0)
    if not submit_mono_camera_matrices(camera_id, transform_to_matrix_array(camera_transform.affine_inverse()), _last_camera_projection):
        return -1
    return smoke_render_camera(camera_id, width, height)

func register_render_camera(camera_id: int) -> bool:
    if camera_id < 0 or camera_id >= 256:
        return false
    if is_render_camera_registered(camera_id):
        return true
    _registered_render_camera_ids.append(camera_id)
    return true

func unregister_render_camera(camera_id: int) -> bool:
    var index := _registered_render_camera_ids.find(camera_id)
    if index < 0:
        return false
    _registered_render_camera_ids.remove_at(index)
    return true

func is_render_camera_registered(camera_id: int) -> bool:
    return _registered_render_camera_ids.has(camera_id)

func get_registered_render_camera_ids() -> PackedInt32Array:
    return PackedInt32Array(_registered_render_camera_ids)

func get_render_diagnostics() -> Dictionary:
    return {
        "last_camera_id": _last_render_camera_id,
        "last_viewport_width": _last_render_viewport_width,
        "last_viewport_height": _last_render_viewport_height,
        "registered_camera_ids": get_registered_render_camera_ids(),
        "render_callback_queued": false,
        "auto_queue_render": auto_queue_render,
        "render_camera_path": render_camera_path,
        "has_last_projection": not _last_camera_projection.is_empty(),
        "last_projection_size": _last_camera_projection.size(),
        "last_camera_origin": _last_camera_transform.origin,
        "adapter_before_render_count": _adapter_before_render_count,
        "adapter_after_render_count": _adapter_after_render_count,
        "adapter_graphics_initialized_count": _adapter_graphics_initialized_count,
        "adapter_graphics_shutdown_count": _adapter_graphics_shutdown_count,
        "adapter_last_camera_id": _adapter_last_camera_id,
        "adapter_last_eye_id": _adapter_last_eye_id,
        "adapter_last_render_result": _adapter_last_render_result,
        "adapter_last_viewport_width": _adapter_last_viewport_width,
        "adapter_last_viewport_height": _adapter_last_viewport_height,
    }

func get_render_backend_diagnostics() -> Dictionary:
    var rendering_method := str(ProjectSettings.get_setting("rendering/renderer/rendering_method", ""))
    var rendering_driver := str(ProjectSettings.get_setting("rendering/rendering_device/driver", ""))
    var actual_rendering_method := RenderingServer.get_current_rendering_method()
    var actual_rendering_driver := RenderingServer.get_current_rendering_driver_name()
    var rendering_device := RenderingServer.get_rendering_device()
    var effective_rendering_method := actual_rendering_method if not actual_rendering_method.is_empty() else rendering_method
    var effective_rendering_driver := actual_rendering_driver if not actual_rendering_driver.is_empty() else rendering_driver
    var is_compatibility := effective_rendering_method == "gl_compatibility"
    var wants_metal := renderer_api == 0 or renderer_api == 4
    var wants_vulkan := renderer_api == 5
    var driver_is_metal := effective_rendering_driver == "metal"
    var driver_is_vulkan := effective_rendering_driver.to_lower() == "vulkan"
    var has_generic_driver_resources := true
    var has_vulkan_driver_resources := has_generic_driver_resources
    var has_compositor_effect_path := true
    return {
        "native_backend_initialized": false,
        "renderer_api": renderer_api,
        "project_rendering_method": rendering_method,
        "project_rendering_driver": rendering_driver,
        "actual_rendering_method": actual_rendering_method,
        "actual_rendering_driver": actual_rendering_driver,
        "has_rendering_device": rendering_device != null,
        "is_compatibility_renderer": is_compatibility,
        "wants_metal_renderer": wants_metal,
        "wants_vulkan_renderer": wants_vulkan,
        "driver_is_vulkan": driver_is_vulkan,
        "has_generic_driver_resources": has_generic_driver_resources,
        "has_vulkan_driver_resources": has_vulkan_driver_resources,
        "has_compositor_effect_path": has_compositor_effect_path,
        "metal_adapter_candidate": rendering_device != null and not is_compatibility and wants_metal and driver_is_metal and has_generic_driver_resources and has_compositor_effect_path,
        "vulkan_adapter_candidate": rendering_device != null and not is_compatibility and wants_vulkan and driver_is_vulkan and has_vulkan_driver_resources and has_compositor_effect_path,
    }

func _update_auto_render_camera() -> void:
    if not auto_queue_render or render_camera_path.is_empty():
        return

    var camera := get_node_or_null(render_camera_path) as Camera3D
    if camera == null:
        return

    var viewport_size: Vector2 = get_viewport().get_visible_rect().size
    var width: int = max(int(viewport_size.x), 1)
    var height: int = max(int(viewport_size.y), 1)
    if is_loaded():
        queue_render_camera_transform(camera.global_transform, width, height, camera.fov, smoke_camera_id)
    else:
        set_camera_transform(camera.global_transform)

func is_loaded() -> bool:
    return _is_loaded

func is_playing() -> bool:
    return _is_playing

func is_sequence_ready() -> bool:
    return _is_loaded

func get_document_state() -> Dictionary:
    return {
        "loading_state": 2 if _is_loaded else 0,
        "playback_state": 0 if _is_playing else 1,
        "sequence_ready": is_sequence_ready(),
        "info_flags": get_document_info_flags(),
    }

func get_document_info_flags() -> int:
    return 0

func get_bounding_box() -> Dictionary:
    return {}

func get_layer_count() -> int:
    return 0

func get_layer_info(_index: int) -> Dictionary:
    return {}

func set_layer_visible(_layer_id: int, _visible: bool) -> bool:
    return false

func clear_layer_visibility_override(_layer_id: int) -> bool:
    return false

func set_layer_opacity(_layer_id: int, _opacity: float) -> bool:
    return false

func set_layer_transform(_layer_id: int, _layer_transform: Transform3D) -> bool:
    return false

func clear_layer_transform_override(_layer_id: int) -> bool:
    return false

func get_layer_diagnostics(_layer_id: int) -> Dictionary:
    return {}

func get_background_color() -> Color:
    return _background_color

func get_spawn_area_ids() -> Array[int]:
    return _spawn_area_ids.duplicate()

func get_active_spawn_area_index() -> int:
    return _active_spawn_area_index

func get_spawn_area_info(_spawn_area_id: int) -> Dictionary:
    return {}

func get_active_spawn_area_info() -> Dictionary:
    if _active_spawn_area_index < 0 or _active_spawn_area_index >= _spawn_area_ids.size():
        return {}
    return get_spawn_area_info(_spawn_area_ids[_active_spawn_area_index])

func transform_to_matrix_array(transform: Transform3D) -> PackedFloat32Array:
    var basis := transform.basis
    var origin := transform.origin
    return PackedFloat32Array([
        basis.x.x, basis.y.x, basis.z.x, 0.0,
        basis.x.y, basis.y.y, basis.z.y, 0.0,
        basis.x.z, basis.y.z, basis.z.z, 0.0,
        origin.x, origin.y, origin.z, 1.0,
    ])

func make_perspective_projection(fov_degrees: float, aspect: float, z_near: float, z_far: float) -> PackedFloat32Array:
    var f := 1.0 / tan(deg_to_rad(fov_degrees) * 0.5)
    var depth := z_near - z_far
    return PackedFloat32Array([
        f / aspect, 0.0, 0.0, 0.0,
        0.0, f, 0.0, 0.0,
        0.0, 0.0, (z_far + z_near) / depth, -1.0,
        0.0, 0.0, (2.0 * z_far * z_near) / depth, 0.0,
    ])

func _cycle_spawn_area(offset: int) -> void:
    if _spawn_area_ids.is_empty():
        push_warning("No spawn areas available yet. Native bridge hookup will populate them.")
        return

    _active_spawn_area_index = posmod(_active_spawn_area_index + offset, _spawn_area_ids.size())
    spawn_area_changed.emit(_active_spawn_area_index)
