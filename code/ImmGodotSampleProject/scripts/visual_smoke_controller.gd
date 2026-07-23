extends Node3D

const EXTENSION_PATH := "res://addons/imm_viewer/imm_viewer.gdextension"
const SAMPLE_DOCUMENT_PATH := "res://../../exampleImmFiles/sample1.imm"
const ANDROID_SAMPLE_DOCUMENT_PATH := "res://sample1.imm"
const ANDROID_USER_SAMPLE_DOCUMENT_PATH := "user://sample1.imm"
const CAMERA_ID := 0
const IMM_RENDERER_API_METAL := 4
const IMM_RENDERER_API_VULKAN := 5
const MAX_READY_SECONDS := 12.0
const ANDROID_MAX_READY_SECONDS := 30.0
const SPAWN_AREA_FRAME_WAIT_SECONDS := 2.0
const RENDER_SETTLE_FRAMES := 30
const DEFAULT_RELOAD_CYCLES := 1
const DEFAULT_VISUAL_SMOKE_WIDTH := 1280
const DEFAULT_VISUAL_SMOKE_HEIGHT := 720
const MIN_CONTENT_PIXELS := 512
const MIN_CONTENT_BOUNDS_SIZE := 12
const MIN_LUMA_RANGE := 0.02
const MIN_ORIENTATION_LUMA_DELTA := 0.05
const MIN_SCENE_PROBE_REGION_PIXELS := 64
const MIN_SCENE_PROBE_DOMINANT_SHARE := 0.80
const MAX_SCENE_PROBE_OCCLUDED_SHARE := 0.08
const MIN_ORDERED_OVERLAY_IMM_PIXELS := 4096
const IMM_TICKS_PER_SECOND := 12600
const DEFAULT_VISUAL_SMOKE_PLAYER_FRAME := -1
const VISUAL_SMOKE_FRAME_RATE := 30
const SUCCESS_MARKER_METAL := "IMM Godot Metal visual smoke passed"
const SUCCESS_MARKER_VULKAN := "IMM Godot Vulkan visual smoke passed"
const COMPOSITION_MODE_FULL_DEPTH := "full_depth"
const COMPOSITION_MODE_ORDERED_OVERLAY := "ordered_overlay"
const SCENE_FRONT_PROBE_COLOR := Color(1.0, 0.0, 1.0, 1.0)
const SCENE_REAR_OCCLUDED_PROBE_COLOR := Color(0.0, 1.0, 1.0, 1.0)
const SCENE_REAR_VISIBLE_PROBE_COLOR := Color(1.0, 1.0, 0.0, 1.0)

@onready var camera: Camera3D = $CameraRig/Camera3D
@onready var status_label: Label3D = $StatusLabel

var viewer: Node
var _compositor_effect: Resource
var _world_environment: WorldEnvironment
var _has_applied_background_color := false
var _last_background_color := Color.BLACK
var _interactive_camera_framed := false
var _front_scene_probe: MeshInstance3D
var _rear_occluded_scene_probe: MeshInstance3D
var _rear_visible_scene_probe: MeshInstance3D

func _ready() -> void:
	if _should_run_visual_smoke():
		call_deferred("_run_visual_smoke")
		return

	if not _setup_viewer():
		return
	_setup_compositor()
	_apply_background_color()
	_update_status("%s visual scene loading" % _selected_renderer_name())
	call_deferred("_run_interactive_playback")

func _process(_delta: float) -> void:
	if _compositor_effect == null or viewer == null:
		return
	if viewer.is_loaded():
		_apply_background_color()
		if not _interactive_camera_framed and viewer.is_sequence_ready():
			_interactive_camera_framed = _frame_camera_from_spawn_area()
		_queue_active_camera()

func _run_interactive_playback() -> void:
	await get_tree().process_frame
	if viewer == null:
		return
	for _frame in range(3):
		_queue_active_camera()
		await get_tree().process_frame

	var load_result: int = int(viewer.load_document())
	if load_result < 0:
		push_error("Interactive %s load_document returned %d" % [_selected_renderer_name(), load_result])
		_update_status("%s visual scene load failed" % _selected_renderer_name())
		return

	var ready_deadline_msec: int = Time.get_ticks_msec() + int(_max_ready_seconds() * 1000.0)
	while Time.get_ticks_msec() < ready_deadline_msec:
		if viewer.is_loaded():
			_apply_background_color()
			if viewer.is_sequence_ready():
				_interactive_camera_framed = await _frame_interactive_camera()
				_queue_active_camera()
				_update_status("%s visual scene playing" % _selected_renderer_name())
				return
			_queue_active_camera()
		await get_tree().create_timer(0.05).timeout

	push_error("Interactive %s scene did not become ready after %.1f seconds" % [_selected_renderer_name(), _max_ready_seconds()])
	_update_status("%s visual scene not ready" % _selected_renderer_name())

func _setup_extension() -> bool:
	if not ClassDB.class_exists("ImmViewerCompositorEffect"):
		var extension_status: int = GDExtensionManager.load_extension(EXTENSION_PATH)
		if extension_status != OK and extension_status != ERR_ALREADY_EXISTS:
			push_error("Failed to load %s: %d" % [EXTENSION_PATH, int(extension_status)])
			return false
	return true

func _setup_viewer() -> bool:
	if viewer != null:
		return true
	if not _setup_extension() or not ClassDB.class_exists("ImmViewerNode"):
		push_error("ImmViewerNode is not available after loading %s" % EXTENSION_PATH)
		return false

	viewer = ClassDB.instantiate("ImmViewerNode")
	if viewer == null:
		push_error("Failed to instantiate ImmViewerNode")
		return false

	viewer.name = "ImmViewer"
	viewer.document_path = _sample_document_path()
	viewer.load_on_ready = false
	viewer.auto_play = true
	viewer.auto_queue_render = true
	viewer.renderer_api = _selected_renderer_api()
	viewer.render_camera_path = NodePath("../CameraRig/Camera3D")
	add_child(viewer)
	return true

func _setup_compositor() -> bool:
	if not _setup_extension():
		return false

	_compositor_effect = ClassDB.instantiate("ImmViewerCompositorEffect")
	if _compositor_effect == null:
		push_error("Failed to instantiate ImmViewerCompositorEffect")
		return false

	var compositor: Compositor = Compositor.new()
	compositor.compositor_effects = [_compositor_effect]
	if _visual_smoke_composition_mode() == COMPOSITION_MODE_ORDERED_OVERLAY:
		_compositor_effect.set("effect_callback_type", CompositorEffect.EFFECT_CALLBACK_TYPE_PRE_TRANSPARENT)
		print("IMM Godot %s visual smoke ordered overlay compositor callback: PRE_TRANSPARENT" % _selected_renderer_name())
	var callback_override: int = _get_env_int("IMM_GODOT_VISUAL_SMOKE_CALLBACK_TYPE", -1)
	if callback_override >= 0 and callback_override < CompositorEffect.EFFECT_CALLBACK_TYPE_MAX:
		_compositor_effect.set("effect_callback_type", callback_override)
		print("IMM Godot %s visual smoke compositor callback override: %d" % [_selected_renderer_name(), callback_override])
	camera.compositor = compositor
	if _world_environment == null:
		_world_environment = WorldEnvironment.new()
		_world_environment.name = "ImmViewerWorldEnvironment"
		add_child(_world_environment)
	_world_environment.compositor = compositor
	return true

func _run_visual_smoke() -> void:
	var failures: Array[String] = []
	await _apply_visual_smoke_viewport_size()
	status_label.visible = false
	var prefer_spawn_area := _visual_smoke_prefers_spawn_area()

	if not _setup_viewer():
		failures.append("ImmViewerNode setup failed")
	if not _setup_compositor():
		failures.append("ImmViewerCompositorEffect setup failed")

	if viewer == null:
		failures.append("ImmViewerNode was not created")
		_finish_visual_smoke(failures, {}, {}, {}, {}, {})
		return
	await get_tree().process_frame

	if not viewer.is_class("ImmViewerNode"):
		failures.append("Expected native ImmViewerNode, got %s" % viewer.get_class())

	var selected_renderer_api := _selected_renderer_api()
	var selected_renderer_name := _selected_renderer_name(selected_renderer_api)
	var forced_player_frame: int = _get_env_int("IMM_GODOT_VISUAL_SMOKE_PLAYER_FRAME", DEFAULT_VISUAL_SMOKE_PLAYER_FRAME)
	viewer.renderer_api = selected_renderer_api
	var backend_diagnostics: Dictionary = viewer.get_render_backend_diagnostics()
	if int(backend_diagnostics.get("renderer_api", -1)) != selected_renderer_api:
		failures.append("ImmViewerNode did not select the %s renderer API" % selected_renderer_name)
	if not bool(backend_diagnostics.get("has_rendering_device", false)):
		failures.append("%s visual smoke did not expose a RenderingDevice" % selected_renderer_name)
	if selected_renderer_api == IMM_RENDERER_API_VULKAN and not bool(backend_diagnostics.get("vulkan_adapter_candidate", false)):
		failures.append("Vulkan visual smoke did not report a Vulkan adapter candidate")
	for _frame in range(3):
		_queue_active_camera()
		await get_tree().process_frame

	if not viewer.is_render_camera_registered(CAMERA_ID):
		failures.append("camera %d was not auto-registered by ImmViewer" % CAMERA_ID)

	if failures.is_empty() and not viewer.is_loaded():
		var load_result: int = int(viewer.load_document())
		print("IMM Godot %s visual smoke load result: %d" % [selected_renderer_name, load_result])
		if load_result < 0:
			failures.append("load_document returned %d" % load_result)

	var sequence_ready: bool = bool(viewer.is_sequence_ready())
	var max_ready_seconds: float = _max_ready_seconds()
	var ready_deadline_msec: int = Time.get_ticks_msec() + int(max_ready_seconds * 1000.0)
	while Time.get_ticks_msec() < ready_deadline_msec:
		if viewer.is_loaded():
			_apply_background_color()
			if viewer.is_sequence_ready():
				sequence_ready = true
				_apply_forced_player_frame(forced_player_frame)
				_select_visual_smoke_spawn_area()
				if _frame_camera_from_document(prefer_spawn_area):
					_queue_active_camera()
					break
			_queue_active_camera()
		await get_tree().create_timer(0.05).timeout

	var reload_cycles: int = _get_env_int("IMM_GODOT_VISUAL_SMOKE_RELOAD_CYCLES", DEFAULT_RELOAD_CYCLES)
	if failures.is_empty() and sequence_ready and reload_cycles > 0:
		sequence_ready = await _exercise_reload_cycles(reload_cycles, failures)

	for _frame in range(RENDER_SETTLE_FRAMES):
		await get_tree().process_frame
		if viewer.is_loaded():
			_apply_background_color()
			if viewer.is_sequence_ready():
				sequence_ready = true
				_apply_forced_player_frame(forced_player_frame)
				_select_visual_smoke_spawn_area()
				if sequence_ready:
					_frame_camera_from_document(prefer_spawn_area)
				_queue_active_camera()

	if sequence_ready:
		await _wait_for_forced_player_frame(forced_player_frame)
		_setup_scene_composition_probe()
		for _frame in range(6):
			_queue_active_camera()
			await get_tree().process_frame

	if not viewer.is_loaded():
		failures.append("ImmViewer did not load %s" % str(viewer.get("document_path")))
	if not sequence_ready:
		failures.append("ImmViewer sequence was not ready after %.1f seconds" % max_ready_seconds)

	var document_state: Dictionary = viewer.get_document_state()
	var document_bounds: Dictionary = viewer.get_bounding_box()
	var render_diagnostics: Dictionary = viewer.get_render_diagnostics()
	if int(render_diagnostics.get("last_camera_id", -1)) != CAMERA_ID:
		failures.append("render diagnostics did not observe camera %d" % CAMERA_ID)
	if int(render_diagnostics.get("num_paint_draw_calls", 0)) <= 0:
		failures.append("render diagnostics did not report foreground paint draw calls")
	if int(render_diagnostics.get("num_draw_calls", 0)) <= 0:
		failures.append("render diagnostics did not report IMM draw calls")

	var compositor_diagnostics: Dictionary = _compositor_effect.get_diagnostics() if _compositor_effect != null else {}
	if compositor_diagnostics.is_empty():
		failures.append("ImmViewerCompositorEffect diagnostics were empty")
	else:
		if int(compositor_diagnostics.get("callback_count", 0)) <= 0:
			failures.append("ImmViewerCompositorEffect render callback did not run")
		if not bool(compositor_diagnostics.get("last_had_scene_buffers", false)):
			failures.append("ImmViewerCompositorEffect did not receive RenderSceneBuffersRD")
		if int(compositor_diagnostics.get("last_color_texture_handle", 0)) == 0:
			failures.append("ImmViewerCompositorEffect did not receive a native color texture handle")
		if int(compositor_diagnostics.get("last_command_queue_handle", 0)) == 0:
			failures.append("ImmViewerCompositorEffect did not receive a native command queue handle")
		if selected_renderer_api == IMM_RENDERER_API_VULKAN:
			if not bool(compositor_diagnostics.get("last_vulkan_frame_started", false)):
				failures.append("ImmViewerCompositorEffect did not start a Vulkan frame")
			if _visual_smoke_composition_mode() == COMPOSITION_MODE_FULL_DEPTH:
				if not bool(compositor_diagnostics.get("last_depth_aware_vulkan_composite", false)):
					failures.append("full-depth Vulkan composition did not run the Godot render-graph depth composite path")
				if not bool(compositor_diagnostics.get("last_depth_aware_vulkan_composite_result", false)):
					failures.append("full-depth Vulkan composition render-graph depth composite failed")
				if not bool(compositor_diagnostics.get("last_had_intermediate_texture", false)) or not bool(compositor_diagnostics.get("last_had_intermediate_depth_texture", false)):
					failures.append("full-depth Vulkan composition did not render IMM into color and depth intermediate textures")
				if int(compositor_diagnostics.get("last_vulkan_depth_image_handle", 0)) == 0 or int(compositor_diagnostics.get("last_vulkan_depth_image_view_handle", 0)) == 0:
					failures.append("full-depth Vulkan composition did not receive Godot depth image handles")
		elif not bool(compositor_diagnostics.get("last_metal_frame_started", false)):
			failures.append("ImmViewerCompositorEffect did not start a Metal frame")
		if int(compositor_diagnostics.get("last_render_result", -1)) < 0:
			failures.append("ImmGodot_RenderCamera returned %d" % int(compositor_diagnostics.get("last_render_result", -1)))

	var screenshot_path: String = _get_env_string("IMM_GODOT_VISUAL_SMOKE_PNG", "")
	var ppm_path: String = _get_env_string("IMM_GODOT_VISUAL_SMOKE_PPM", "")
	if not screenshot_path.is_empty():
		_apply_background_color()
		await RenderingServer.frame_post_draw
		var image: Image = get_viewport().get_texture().get_image()
		var content_diagnostics := _analyze_content_pixels(image, viewer.get_background_color())
		var ordered_overlay_imm_diagnostics := _analyze_ordered_overlay_imm_pixels(image, viewer.get_background_color())
		var scene_composition_diagnostics := _analyze_scene_composition_pixels(image)
		print("IMM Godot %s visual smoke content diagnostics: %s" % [selected_renderer_name, str(content_diagnostics)])
		print("IMM Godot %s visual smoke ordered overlay IMM diagnostics: %s" % [selected_renderer_name, str(ordered_overlay_imm_diagnostics)])
		print("IMM Godot %s visual smoke scene composition diagnostics: %s" % [selected_renderer_name, str(scene_composition_diagnostics)])
		if float(content_diagnostics.get("luma_range", 0.0)) < MIN_LUMA_RANGE:
			failures.append("visual smoke PNG was too flat: luma range %.5f" % float(content_diagnostics.get("luma_range", 0.0)))
		if int(content_diagnostics.get("content_pixels", 0)) < MIN_CONTENT_PIXELS:
			failures.append("visual smoke PNG had only %d content pixels" % int(content_diagnostics.get("content_pixels", 0)))
		if int(content_diagnostics.get("content_bounds_width", 0)) < MIN_CONTENT_BOUNDS_SIZE or int(content_diagnostics.get("content_bounds_height", 0)) < MIN_CONTENT_BOUNDS_SIZE:
			failures.append("visual smoke PNG content bounds were too small: %sx%s" % [
				str(content_diagnostics.get("content_bounds_width", 0)),
				str(content_diagnostics.get("content_bounds_height", 0)),
			])
		var orientation_luma_delta: float = float(content_diagnostics.get("orientation_luma_delta", 0.0))
		if selected_renderer_api == IMM_RENDERER_API_METAL and orientation_luma_delta < MIN_ORIENTATION_LUMA_DELTA:
			failures.append("visual smoke PNG orientation check failed: upper/lower luma delta %.5f" % orientation_luma_delta)
		_append_imm_visibility_failures(ordered_overlay_imm_diagnostics, failures, "PNG")
		_append_scene_composition_failures(scene_composition_diagnostics, failures, "PNG")
		var screenshot_dir: String = screenshot_path.get_base_dir()
		if not screenshot_dir.is_empty():
			DirAccess.make_dir_recursive_absolute(screenshot_dir)
		var save_result := image.save_png(screenshot_path)
		if save_result != OK:
			failures.append("Failed to save visual smoke PNG %s: %d" % [screenshot_path, int(save_result)])
		else:
			print("IMM Godot %s visual smoke PNG: %s" % [selected_renderer_name, screenshot_path])

	if not ppm_path.is_empty():
		_apply_background_color()
		await RenderingServer.frame_post_draw
		var image: Image = get_viewport().get_texture().get_image()
		var content_diagnostics := _analyze_content_pixels(image, viewer.get_background_color())
		var ordered_overlay_imm_diagnostics := _analyze_ordered_overlay_imm_pixels(image, viewer.get_background_color())
		var scene_composition_diagnostics := _analyze_scene_composition_pixels(image)
		print("IMM Godot %s visual smoke PPM content diagnostics: %s" % [selected_renderer_name, str(content_diagnostics)])
		print("IMM Godot %s visual smoke PPM ordered overlay IMM diagnostics: %s" % [selected_renderer_name, str(ordered_overlay_imm_diagnostics)])
		print("IMM Godot %s visual smoke PPM scene composition diagnostics: %s" % [selected_renderer_name, str(scene_composition_diagnostics)])
		if int(content_diagnostics.get("content_pixels", 0)) < MIN_CONTENT_PIXELS:
			failures.append("visual smoke PPM had only %d content pixels" % int(content_diagnostics.get("content_pixels", 0)))
		_append_imm_visibility_failures(ordered_overlay_imm_diagnostics, failures, "PPM")
		_append_scene_composition_failures(scene_composition_diagnostics, failures, "PPM")
		var save_result := _save_ppm(image, ppm_path)
		if save_result != OK:
			failures.append("Failed to save visual smoke PPM %s: %d" % [ppm_path, int(save_result)])
		else:
			print("IMM Godot %s visual smoke PPM: %s" % [selected_renderer_name, ppm_path])

	_finish_visual_smoke(failures, backend_diagnostics, document_state, document_bounds, render_diagnostics, compositor_diagnostics)

func _finish_visual_smoke(
	failures: Array[String],
	backend_diagnostics: Dictionary,
	document_state: Dictionary,
	document_bounds: Dictionary,
	render_diagnostics: Dictionary,
	compositor_diagnostics: Dictionary
) -> void:
	var selected_renderer_api := _selected_renderer_api()
	var selected_renderer_name := _selected_renderer_name(selected_renderer_api)
	print("IMM Godot %s visual smoke backend diagnostics: %s" % [selected_renderer_name, str(backend_diagnostics)])
	print("IMM Godot %s visual smoke document state: %s" % [selected_renderer_name, str(document_state)])
	print("IMM Godot %s visual smoke document bounds: %s" % [selected_renderer_name, str(document_bounds)])
	print("IMM Godot %s visual smoke render diagnostics: %s" % [selected_renderer_name, str(render_diagnostics)])
	print("IMM Godot %s visual smoke compositor diagnostics: %s" % [selected_renderer_name, str(compositor_diagnostics)])

	if failures.is_empty():
		print(SUCCESS_MARKER_VULKAN if selected_renderer_api == IMM_RENDERER_API_VULKAN else SUCCESS_MARKER_METAL)
		get_tree().quit(0)
		return

	for failure in failures:
		push_error(failure)
	get_tree().quit(1)

func _queue_active_camera() -> void:
	var viewport_size: Vector2 = get_viewport().get_visible_rect().size
	var width: int = max(int(viewport_size.x), 1)
	var height: int = max(int(viewport_size.y), 1)
	viewer.queue_render_camera_transform(camera.global_transform, width, height, camera.fov, CAMERA_ID)

func _apply_visual_smoke_viewport_size() -> void:
	var width: int = max(_get_env_int("IMM_GODOT_VISUAL_SMOKE_WIDTH", DEFAULT_VISUAL_SMOKE_WIDTH), 1)
	var height: int = max(_get_env_int("IMM_GODOT_VISUAL_SMOKE_HEIGHT", DEFAULT_VISUAL_SMOKE_HEIGHT), 1)
	var target_size := Vector2i(width, height)
	DisplayServer.window_set_size(target_size)
	var window := get_window()
	if window != null:
		window.size = target_size
	for _frame in range(3):
		await get_tree().process_frame
	var actual_size: Vector2 = get_viewport().get_visible_rect().size
	print("IMM Godot %s visual smoke viewport size requested=%s actual=%s" % [
		_selected_renderer_name(),
		str(target_size),
		str(actual_size),
	])

func _apply_forced_player_frame(player_frame: int) -> void:
	if player_frame < 0:
		return
	var ticks_per_frame: int = IMM_TICKS_PER_SECOND / VISUAL_SMOKE_FRAME_RATE
	viewer.set_time(int(player_frame * ticks_per_frame), 0)

func _wait_for_forced_player_frame(player_frame: int) -> void:
	if player_frame < 0:
		return
	var deadline_msec: int = Time.get_ticks_msec() + int(_max_ready_seconds() * 1000.0)
	while Time.get_ticks_msec() < deadline_msec:
		_apply_forced_player_frame(player_frame)
		_queue_active_camera()
		await get_tree().process_frame
		var render_diagnostics: Dictionary = viewer.get_render_diagnostics()
		if int(render_diagnostics.get("validation_time_frame", 0)) >= player_frame:
			print("IMM Godot %s visual smoke reached validation player frame %d" % [
				_selected_renderer_name(),
				player_frame,
			])
			return
	print("IMM Godot %s visual smoke did not observe validation player frame %d before capture" % [
		_selected_renderer_name(),
		player_frame,
	])

func _frame_interactive_camera() -> bool:
	var deadline_msec: int = Time.get_ticks_msec() + int(SPAWN_AREA_FRAME_WAIT_SECONDS * 1000.0)
	while Time.get_ticks_msec() < deadline_msec:
		if _frame_camera_from_spawn_area():
			return true
		_queue_active_camera()
		await get_tree().create_timer(0.05).timeout
	return _frame_camera_from_document(false)

func _frame_camera_from_spawn_area() -> bool:
	var spawn_info: Dictionary = viewer.get_active_spawn_area_info()
	if spawn_info.is_empty():
		return false
	var spawn_transform: Transform3D = _spawn_area_transform_from_info(spawn_info)
	camera.global_transform = spawn_transform
	camera.near = 0.01
	camera.far = 10000.0
	camera.force_update_transform()
	print("IMM Godot %s visual smoke camera framed from spawn area: position=%s basis=%s" % [
		_selected_renderer_name(),
		str(camera.global_position),
		str(camera.global_transform.basis),
	])
	return true

func _exercise_reload_cycles(cycle_count: int, failures: Array[String]) -> bool:
	var stayed_ready: bool = true
	for cycle_index in range(cycle_count):
		var selected_renderer_name: String = _selected_renderer_name()
		viewer.unload_document()
		await get_tree().process_frame
		if viewer.is_loaded():
			failures.append("%s visual smoke reload cycle %d did not unload the document" % [selected_renderer_name, cycle_index + 1])
			stayed_ready = false
			continue

		_queue_active_camera()
		var load_result: int = int(viewer.load_document())
		print("IMM Godot %s visual smoke reload cycle %d load result: %d" % [selected_renderer_name, cycle_index + 1, load_result])
		if load_result < 0:
			failures.append("%s visual smoke reload cycle %d load_document returned %d" % [selected_renderer_name, cycle_index + 1, load_result])
			stayed_ready = false
			continue

		var ready: bool = false
		var max_ready_seconds: float = _max_ready_seconds()
		var ready_deadline_msec: int = Time.get_ticks_msec() + int(max_ready_seconds * 1000.0)
		while Time.get_ticks_msec() < ready_deadline_msec:
			if viewer.is_loaded():
				_apply_background_color()
				if viewer.is_sequence_ready():
					ready = true
					_frame_camera_from_document()
					_queue_active_camera()
					break
				_queue_active_camera()
			await get_tree().create_timer(0.05).timeout

		if not ready:
			failures.append("%s visual smoke reload cycle %d sequence was not ready after %.1f seconds" % [
				selected_renderer_name,
				cycle_index + 1,
				max_ready_seconds,
			])
			stayed_ready = false
	return stayed_ready

func _apply_background_color() -> void:
	var color: Color = viewer.get_background_color()
	if color.a <= 0.0:
		color.a = 1.0
	if _has_applied_background_color and color == _last_background_color:
		return
	RenderingServer.set_default_clear_color(color)
	_last_background_color = color
	_has_applied_background_color = true

func _frame_camera_from_document(prefer_spawn_area: bool = true) -> bool:
	if prefer_spawn_area and _frame_camera_from_spawn_area():
		return true

	var bounds: Dictionary = viewer.get_bounding_box()
	if bounds.is_empty():
		return false

	var center: Vector3 = _native_point_to_godot(bounds.get("center", Vector3.ZERO))
	var size: Vector3 = bounds.get("size", Vector3.ONE)
	var radius: float = max(max(size.x, size.y), size.z) * 0.5
	if radius <= 0.001:
		radius = 1.0

	var distance: float = max(radius * 1.4, 2.5)
	camera.global_position = center + Vector3(0.0, max(radius * 0.25, 0.4), distance)
	camera.look_at(center, Vector3.UP)
	camera.near = 0.01
	camera.far = max(distance + radius * 4.0, 100.0)
	camera.force_update_transform()
	print("IMM Godot %s visual smoke camera framed: position=%s target=%s radius=%.3f far=%.3f" % [
		_selected_renderer_name(),
		str(camera.global_position),
		str(center),
		radius,
		camera.far,
	])
	return true

func _visual_smoke_prefers_spawn_area() -> bool:
	return OS.get_environment("IMM_GODOT_VISUAL_SMOKE_USE_SPAWN_AREA") == "1"

func _visual_smoke_composition_mode() -> String:
	var mode: String = _get_env_string("IMM_GODOT_VISUAL_SMOKE_COMPOSITION_MODE", COMPOSITION_MODE_FULL_DEPTH)
	if mode == COMPOSITION_MODE_ORDERED_OVERLAY:
		return COMPOSITION_MODE_ORDERED_OVERLAY
	return COMPOSITION_MODE_FULL_DEPTH

func _select_visual_smoke_spawn_area() -> void:
	var requested_index: int = _get_env_int("IMM_GODOT_VISUAL_SMOKE_SPAWN_INDEX", -1)
	if requested_index < 0:
		return
	var spawn_ids: PackedInt32Array = PackedInt32Array(viewer.get_spawn_area_ids())
	if spawn_ids.is_empty():
		return
	var current_index: int = int(viewer.get_active_spawn_area_index())
	var target_index: int = clampi(requested_index, 0, spawn_ids.size() - 1)
	while current_index >= 0 and current_index != target_index:
		viewer.next_spawn_area()
		current_index = int(viewer.get_active_spawn_area_index())
	var spawn_info: Dictionary = viewer.get_active_spawn_area_info()
	print("IMM Godot %s visual smoke selected spawn index=%d id=%s name=%s" % [
		_selected_renderer_name(),
		target_index,
		str(spawn_info.get("id", "")),
		str(spawn_info.get("name", "")),
	])

func _spawn_area_transform_from_info(info: Dictionary) -> Transform3D:
	var transform: Dictionary = info.get("transform", {})
	var basis: Basis = Basis(
		transform.get("basis_x", Vector3.RIGHT),
		transform.get("basis_y", Vector3.UP),
		transform.get("basis_z", Vector3.BACK)
	)
	return Transform3D(basis.orthonormalized(), transform.get("position", Vector3.ZERO))

func _native_point_to_godot(point: Vector3) -> Vector3:
	return Vector3(point.x, point.y, -point.z)

func _setup_scene_composition_probe() -> void:
	if _front_scene_probe != null and _rear_occluded_scene_probe != null and _rear_visible_scene_probe != null:
		return
	if viewer == null or not viewer.is_sequence_ready():
		return

	var bounds: Dictionary = viewer.get_bounding_box()
	if bounds.is_empty():
		return

	var forward: Vector3 = -camera.global_transform.basis.z.normalized()
	var right: Vector3 = camera.global_transform.basis.x.normalized()
	var up: Vector3 = camera.global_transform.basis.y.normalized()
	var center: Vector3 = camera.global_position + forward * 3.0
	var probe_size: float = 0.55
	var probe_depth: float = 0.06

	var ordered_overlay := _visual_smoke_composition_mode() == COMPOSITION_MODE_ORDERED_OVERLAY

	_front_scene_probe = _create_scene_probe("IMMSceneFrontOccluderProbe", SCENE_FRONT_PROBE_COLOR, Vector3(0.50, 0.50, probe_depth), ordered_overlay)
	add_child(_front_scene_probe)
	_front_scene_probe.global_position = center - right * 0.70 - up * 0.35 - forward * 1.00
	_front_scene_probe.look_at(camera.global_position, Vector3.UP)

	_rear_occluded_scene_probe = _create_scene_probe("IMMSceneRearOccludedProbe", SCENE_REAR_OCCLUDED_PROBE_COLOR, Vector3(0.75, 0.75, probe_depth), ordered_overlay)
	add_child(_rear_occluded_scene_probe)
	_rear_occluded_scene_probe.global_position = center + forward * 0.95 + right * 0.25
	_rear_occluded_scene_probe.look_at(camera.global_position, Vector3.UP)

	_rear_visible_scene_probe = _create_scene_probe("IMMSceneRearVisibleProbe", SCENE_REAR_VISIBLE_PROBE_COLOR, Vector3(0.65, 0.65, probe_depth), ordered_overlay)
	add_child(_rear_visible_scene_probe)
	_rear_visible_scene_probe.global_position = center + right * 1.30 + up * 0.85 + forward * 0.35
	_rear_visible_scene_probe.look_at(camera.global_position, Vector3.UP)
	print("IMM Godot %s visual smoke scene composition probes: front=%s rear_occluded=%s rear_visible=%s size=%.3f" % [
		_selected_renderer_name(),
		str(_front_scene_probe.global_position),
		str(_rear_occluded_scene_probe.global_position),
		str(_rear_visible_scene_probe.global_position),
		probe_size,
	])

func _create_scene_probe(name: String, color: Color, size: Vector3, ordered_overlay: bool) -> MeshInstance3D:
	var mesh: BoxMesh = BoxMesh.new()
	mesh.size = size
	var material: StandardMaterial3D = StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = color
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	if ordered_overlay:
		material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		material.no_depth_test = true
	var probe: MeshInstance3D = MeshInstance3D.new()
	probe.name = name
	probe.mesh = mesh
	probe.material_override = material
	probe.set_meta("imm_probe_half_extents", size * 0.5)
	return probe

func _analyze_scene_composition_pixels(image: Image) -> Dictionary:
	var front: Dictionary = _analyze_scene_probe_region(image, _front_scene_probe, SCENE_FRONT_PROBE_COLOR)
	var rear_visible: Dictionary = _analyze_scene_probe_region(image, _rear_visible_scene_probe, SCENE_REAR_VISIBLE_PROBE_COLOR)
	var rear_occluded: Dictionary = _analyze_scene_probe_region(image, _rear_occluded_scene_probe, SCENE_REAR_OCCLUDED_PROBE_COLOR)
	return {
		"front_probe": front,
		"rear_visible_probe": rear_visible,
		"rear_occluded_probe": rear_occluded,
		"minimum_region_pixels": MIN_SCENE_PROBE_REGION_PIXELS,
		"minimum_dominant_share": MIN_SCENE_PROBE_DOMINANT_SHARE,
		"maximum_occluded_share": MAX_SCENE_PROBE_OCCLUDED_SHARE,
	}

func _append_scene_composition_failures(diagnostics: Dictionary, failures: Array[String], label: String) -> void:
	var front: Dictionary = diagnostics.get("front_probe", {})
	var rear_visible: Dictionary = diagnostics.get("rear_visible_probe", {})
	var rear_occluded: Dictionary = diagnostics.get("rear_occluded_probe", {})
	var ordered_overlay: bool = _visual_smoke_composition_mode() == COMPOSITION_MODE_ORDERED_OVERLAY
	if int(front.get("total_pixels", 0)) < MIN_SCENE_PROBE_REGION_PIXELS or float(front.get("target_share", 0.0)) < MIN_SCENE_PROBE_DOMINANT_SHARE:
		failures.append("scene composition %s front occluder probe failed: %s" % [label, str(front)])
	if int(rear_visible.get("total_pixels", 0)) < MIN_SCENE_PROBE_REGION_PIXELS or float(rear_visible.get("target_share", 0.0)) < MIN_SCENE_PROBE_DOMINANT_SHARE:
		failures.append("scene composition %s rear visible probe failed: %s" % [label, str(rear_visible)])
	if ordered_overlay:
		if int(rear_occluded.get("total_pixels", 0)) < MIN_SCENE_PROBE_REGION_PIXELS or float(rear_occluded.get("target_share", 0.0)) < MIN_SCENE_PROBE_DOMINANT_SHARE:
			failures.append("scene composition %s ordered overlay rear probe failed: %s" % [label, str(rear_occluded)])
	elif int(rear_occluded.get("total_pixels", 0)) < MIN_SCENE_PROBE_REGION_PIXELS or float(rear_occluded.get("target_share", 0.0)) > MAX_SCENE_PROBE_OCCLUDED_SHARE:
		failures.append("scene composition %s rear occlusion leakage probe failed: %s" % [label, str(rear_occluded)])

func _analyze_scene_probe_region(image: Image, probe: MeshInstance3D, target: Color) -> Dictionary:
	if probe == null:
		return {
			"name": "missing",
			"total_pixels": 0,
			"target_pixels": 0,
			"target_share": 0.0,
			"rect": Rect2i(),
		}

	var rect: Rect2i = _project_probe_rect(image, probe)
	var total_pixels: int = 0
	var target_pixels: int = 0
	if rect.size.x > 0 and rect.size.y > 0:
		for y in range(rect.position.y, rect.position.y + rect.size.y):
			for x in range(rect.position.x, rect.position.x + rect.size.x):
				total_pixels += 1
				if _color_near(image.get_pixel(x, y), target):
					target_pixels += 1
	var target_share: float = float(target_pixels) / float(maxi(total_pixels, 1))
	return {
		"name": probe.name,
		"total_pixels": total_pixels,
		"target_pixels": target_pixels,
		"target_share": target_share,
		"rect": rect,
	}

func _project_probe_rect(image: Image, probe: MeshInstance3D) -> Rect2i:
	var half_extents: Vector3 = probe.get_meta("imm_probe_half_extents", Vector3.ONE * 0.5)
	var basis: Basis = probe.global_transform.basis
	var center: Vector3 = probe.global_position
	var corners: Array[Vector3] = [
		center + basis.x * -half_extents.x + basis.y * -half_extents.y + basis.z * -half_extents.z,
		center + basis.x * -half_extents.x + basis.y * half_extents.y + basis.z * -half_extents.z,
		center + basis.x * half_extents.x + basis.y * -half_extents.y + basis.z * -half_extents.z,
		center + basis.x * half_extents.x + basis.y * half_extents.y + basis.z * -half_extents.z,
	]
	var min_x: int = image.get_width()
	var min_y: int = image.get_height()
	var max_x: int = -1
	var max_y: int = -1
	for world_corner in corners:
		if camera.is_position_behind(world_corner):
			continue
		var screen: Vector2 = camera.unproject_position(world_corner)
		min_x = min(min_x, int(floor(screen.x)))
		min_y = min(min_y, int(floor(screen.y)))
		max_x = max(max_x, int(ceil(screen.x)))
		max_y = max(max_y, int(ceil(screen.y)))
	if max_x < min_x or max_y < min_y:
		return Rect2i()

	var inset: int = 3
	min_x = clampi(min_x + inset, 0, image.get_width() - 1)
	max_x = clampi(max_x - inset, 0, image.get_width() - 1)
	min_y = clampi(min_y + inset, 0, image.get_height() - 1)
	max_y = clampi(max_y - inset, 0, image.get_height() - 1)
	if max_x < min_x or max_y < min_y:
		return Rect2i()
	return Rect2i(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1)

func _color_near(value: Color, target: Color) -> bool:
	if target.r > 0.5 and target.g < 0.5 and target.b > 0.5:
		return value.r > value.g + 0.12 and value.b > value.g + 0.12 and maxf(value.r, value.b) > 0.25
	if target.r < 0.5 and target.g > 0.5 and target.b > 0.5:
		return value.g > value.r + 0.12 and value.b > value.r + 0.12 and maxf(value.g, value.b) > 0.25
	if target.r > 0.5 and target.g > 0.5 and target.b < 0.5:
		return value.r > value.b + 0.12 and value.g > value.b + 0.12 and maxf(value.r, value.g) > 0.25
	return absf(value.r - target.r) <= 0.20 and absf(value.g - target.g) <= 0.20 and absf(value.b - target.b) <= 0.20

func _is_scene_probe_color(value: Color) -> bool:
	return (
		_color_near(value, SCENE_FRONT_PROBE_COLOR)
		or _color_near(value, SCENE_REAR_OCCLUDED_PROBE_COLOR)
		or _color_near(value, SCENE_REAR_VISIBLE_PROBE_COLOR)
	)

func _analyze_ordered_overlay_imm_pixels(image: Image, background: Color) -> Dictionary:
	var width: int = image.get_width()
	var height: int = image.get_height()
	var background_rgb: Vector3 = Vector3(background.r, background.g, background.b)
	var imm_like_pixels: int = 0
	var scene_probe_pixels: int = 0
	for y in range(height):
		for x in range(width):
			var color: Color = image.get_pixel(x, y)
			var rgb: Vector3 = Vector3(color.r, color.g, color.b)
			if (rgb - background_rgb).length() <= 0.08:
				continue
			if _is_scene_probe_color(color):
				scene_probe_pixels += 1
				continue
			imm_like_pixels += 1
	return {
		"width": width,
		"height": height,
		"imm_like_pixels": imm_like_pixels,
		"scene_probe_pixels": scene_probe_pixels,
		"minimum_imm_like_pixels": MIN_ORDERED_OVERLAY_IMM_PIXELS,
	}

func _append_imm_visibility_failures(diagnostics: Dictionary, failures: Array[String], label: String) -> void:
	var composition_mode: String = _visual_smoke_composition_mode()
	if composition_mode != COMPOSITION_MODE_ORDERED_OVERLAY and composition_mode != COMPOSITION_MODE_FULL_DEPTH:
		return
	if int(diagnostics.get("imm_like_pixels", 0)) < MIN_ORDERED_OVERLAY_IMM_PIXELS:
		failures.append("scene composition %s %s IMM visibility failed: %s" % [label, composition_mode, str(diagnostics)])

func _analyze_content_pixels(image: Image, background: Color) -> Dictionary:
	var width: int = image.get_width()
	var height: int = image.get_height()
	var background_rgb: Vector3 = Vector3(background.r, background.g, background.b)
	var content_pixels: int = 0
	var min_x: int = width
	var min_y: int = height
	var max_x: int = -1
	var max_y: int = -1
	var min_luma: float = 1.0
	var max_luma: float = 0.0
	var upper_luma_sum: float = 0.0
	var upper_luma_count: int = 0
	var lower_luma_sum: float = 0.0
	var lower_luma_count: int = 0

	for y in range(height):
		for x in range(width):
			var color: Color = image.get_pixel(x, y)
			var rgb: Vector3 = Vector3(color.r, color.g, color.b)
			var distance: float = (rgb - background_rgb).length()
			var luma: float = color.r * 0.2126 + color.g * 0.7152 + color.b * 0.0722
			min_luma = min(min_luma, luma)
			max_luma = max(max_luma, luma)
			if distance > 0.08:
				content_pixels += 1
				min_x = min(min_x, x)
				min_y = min(min_y, y)
				max_x = max(max_x, x)
				max_y = max(max_y, y)

	if content_pixels > 0:
		var split_y: int = int(floor(float(min_y + max_y) * 0.5))
		for y in range(min_y, max_y + 1):
			for x in range(min_x, max_x + 1):
				var color: Color = image.get_pixel(x, y)
				var rgb: Vector3 = Vector3(color.r, color.g, color.b)
				var distance: float = (rgb - background_rgb).length()
				if distance <= 0.08:
					continue
				var luma: float = color.r * 0.2126 + color.g * 0.7152 + color.b * 0.0722
				if y <= split_y:
					upper_luma_sum += luma
					upper_luma_count += 1
				else:
					lower_luma_sum += luma
					lower_luma_count += 1

	var upper_content_luma: float = upper_luma_sum / float(maxi(upper_luma_count, 1))
	var lower_content_luma: float = lower_luma_sum / float(maxi(lower_luma_count, 1))

	return {
		"width": width,
		"height": height,
		"content_pixels": content_pixels,
		"content_bounds_min": Vector2i(min_x, min_y) if content_pixels > 0 else Vector2i.ZERO,
		"content_bounds_max": Vector2i(max_x, max_y) if content_pixels > 0 else Vector2i.ZERO,
		"content_bounds_width": max_x - min_x + 1 if content_pixels > 0 else 0,
		"content_bounds_height": max_y - min_y + 1 if content_pixels > 0 else 0,
		"min_luma": min_luma,
		"max_luma": max_luma,
		"luma_range": max_luma - min_luma,
		"upper_content_luma": upper_content_luma,
		"lower_content_luma": lower_content_luma,
		"orientation_luma_delta": upper_content_luma - lower_content_luma,
	}

func _save_ppm(image: Image, path: String) -> int:
	var file: FileAccess = FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		return FileAccess.get_open_error()
	file.store_buffer(("P6\n%d %d\n255\n" % [image.get_width(), image.get_height()]).to_ascii_buffer())
	for y in range(image.get_height()):
		for x in range(image.get_width()):
			var color: Color = image.get_pixel(x, y)
			file.store_8(clampi(int(round(color.r * 255.0)), 0, 255))
			file.store_8(clampi(int(round(color.g * 255.0)), 0, 255))
			file.store_8(clampi(int(round(color.b * 255.0)), 0, 255))
	return OK

func _update_status(message: String) -> void:
	status_label.text = "%s\nRenderer API: %s\nDocument: %s" % [
		message,
		_selected_renderer_name(),
		"loaded" if viewer.is_loaded() else "not loaded",
	]

func _selected_renderer_api() -> int:
	var requested_api: int = _get_env_int("IMM_GODOT_VISUAL_RENDERER_API", -1)
	if requested_api >= 0:
		return requested_api
	return IMM_RENDERER_API_METAL if OS.get_name() == "macOS" else IMM_RENDERER_API_VULKAN

func _selected_renderer_name(renderer_api: int = -1) -> String:
	var api: int = renderer_api if renderer_api >= 0 else _selected_renderer_api()
	if api == IMM_RENDERER_API_VULKAN:
		return "Vulkan"
	if api == IMM_RENDERER_API_METAL:
		return "Metal"
	return "API%d" % api

func _sample_document_path() -> String:
	if OS.get_name() != "Android":
		return SAMPLE_DOCUMENT_PATH
	return _prepare_android_sample_document()

func _max_ready_seconds() -> float:
	return ANDROID_MAX_READY_SECONDS if OS.get_name() == "Android" else MAX_READY_SECONDS

func _prepare_android_sample_document() -> String:
	var source_file: FileAccess = FileAccess.open(ANDROID_SAMPLE_DOCUMENT_PATH, FileAccess.READ)
	if source_file == null:
		push_error("Failed to open Android sample document %s: %s" % [
			ANDROID_SAMPLE_DOCUMENT_PATH,
			error_string(FileAccess.get_open_error()),
		])
		return ANDROID_SAMPLE_DOCUMENT_PATH

	var data: PackedByteArray = source_file.get_buffer(source_file.get_length())
	var target_file: FileAccess = FileAccess.open(ANDROID_USER_SAMPLE_DOCUMENT_PATH, FileAccess.WRITE)
	if target_file == null:
		push_error("Failed to create Android sample document %s: %s" % [
			ANDROID_USER_SAMPLE_DOCUMENT_PATH,
			error_string(FileAccess.get_open_error()),
		])
		return ANDROID_SAMPLE_DOCUMENT_PATH

	target_file.store_buffer(data)
	target_file.flush()
	return OS.get_user_data_dir().path_join("sample1.imm")

func _should_run_visual_smoke() -> bool:
	if OS.get_environment("IMM_GODOT_VISUAL_SMOKE") == "1":
		return true
	return OS.get_cmdline_args().has("--imm-godot-visual-smoke")

func _get_env_int(name: String, default_value: int) -> int:
	var value: String = _get_env_string(name, "")
	if value.is_empty():
		return default_value
	if not value.is_valid_int():
		return default_value
	return max(int(value), 0)

func _get_env_string(name: String, default_value: String) -> String:
	var value: String = OS.get_environment(name)
	if value.is_empty():
		for argument in OS.get_cmdline_args():
			var prefix: String = "--%s=" % name.to_lower().replace("_", "-")
			if argument.begins_with(prefix):
				value = argument.substr(prefix.length())
				break
	if value.is_empty():
		return default_value
	return value
