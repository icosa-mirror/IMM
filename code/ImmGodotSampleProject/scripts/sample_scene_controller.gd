extends Node3D

const MOVE_SPEED := 4.0
const BOOST_MULTIPLIER := 3.0
const VOLUME_STEP := 0.1
const SPAWN_AREA_FRAME_WAIT_SECONDS := 2.0
const CAMERA_ID := 0
const SAMPLE_PLAY_SMOKE_PREFIX := "[IMM_GODOT_SAMPLE_PLAY_20260803]"
const SAMPLE_PLAY_SMOKE_TIMEOUT_SECONDS := 150.0
const SAMPLE_PLAY_SMOKE_SETTLE_FRAMES := 3

@onready var viewer := $ImmViewer
@onready var camera_rig: Node3D = $CameraRig
@onready var camera: Camera3D = $CameraRig/Camera3D
@onready var status_label: Label3D = $StatusLabel

var _first_layer_hidden := false
var _has_applied_background_color := false
var _last_background_color := Color.BLACK
var _initial_camera_framed := false

func _ready() -> void:
	var native_log_path := OS.get_environment("IMM_GODOT_LOG_FILE")
	if not native_log_path.is_empty():
		viewer.log_file_path = native_log_path
	viewer.document_loaded.connect(_on_document_loaded)
	viewer.document_unloaded.connect(_update_status)
	viewer.playback_changed.connect(_on_playback_changed)
	viewer.spawn_area_changed.connect(_on_spawn_area_changed)
	_update_status()
	call_deferred("_run_initial_playback")

func _process(delta: float) -> void:
	_handle_movement(delta)
	if viewer.is_loaded():
		_apply_background_color()
		if not _initial_camera_framed and viewer.is_sequence_ready():
			_initial_camera_framed = _jump_to_active_spawn_area()

func _handle_movement(delta: float) -> void:
	var move_input := Vector3.ZERO
	if Input.is_key_pressed(KEY_W):
		move_input.z -= 1.0
	if Input.is_key_pressed(KEY_S):
		move_input.z += 1.0
	if Input.is_key_pressed(KEY_A):
		move_input.x -= 1.0
	if Input.is_key_pressed(KEY_D):
		move_input.x += 1.0
	if Input.is_key_pressed(KEY_E):
		move_input.y += 1.0
	if Input.is_key_pressed(KEY_Q):
		move_input.y -= 1.0

	if move_input == Vector3.ZERO:
		return

	var speed := MOVE_SPEED
	if Input.is_key_pressed(KEY_SHIFT):
		speed *= BOOST_MULTIPLIER

	camera_rig.translate_object_local(move_input.normalized() * speed * delta)

func _unhandled_key_input(event: InputEvent) -> void:
	if not event is InputEventKey:
		return
	if not event.pressed or event.echo:
		return

	match event.keycode:
		KEY_L:
			_load_document_after_render_warmup()
		KEY_U:
			viewer.unload_document()
			_apply_background_color()
		KEY_SPACE:
			viewer.toggle_pause()
		KEY_R:
			viewer.restart()
		KEY_COMMA:
			viewer.skip_back()
			_update_status()
		KEY_PERIOD:
			viewer.skip_forward()
			_update_status()
		KEY_MINUS:
			_adjust_volume(-VOLUME_STEP)
		KEY_EQUAL:
			_adjust_volume(VOLUME_STEP)
		KEY_V:
			_toggle_first_layer_visibility()
		KEY_BRACKETRIGHT:
			viewer.next_chapter()
		KEY_BRACKETLEFT:
			viewer.previous_chapter()
		KEY_APOSTROPHE:
			viewer.next_spawn_area()
			_jump_to_active_spawn_area()
		KEY_SEMICOLON:
			viewer.previous_spawn_area()
			_jump_to_active_spawn_area()
		KEY_BACKSLASH:
			var result: int = int(viewer.queue_render_last_camera())
			print("IMM fixed-viewport diagnostic render request result: %d" % result)
			_update_status()

func _on_playback_changed(_is_playing: bool) -> void:
	_update_status()

func _on_document_loaded(_path: String) -> void:
	_initial_camera_framed = false
	_apply_background_color()
	call_deferred("_frame_initial_camera")
	_update_status()

func _on_spawn_area_changed(_active_index: int) -> void:
	_update_status()

func _run_initial_playback() -> void:
	await _load_document_after_render_warmup()
	if OS.get_environment("IMM_GODOT_SAMPLE_PLAY_SMOKE") == "1":
		await _run_sample_play_smoke()

func _load_document_after_render_warmup() -> void:
	if viewer.is_loaded():
		_apply_background_color()
		_update_status()
		return
	for _frame in range(3):
		_queue_active_camera()
		await get_tree().process_frame
	var load_result: int = int(viewer.load_document())
	if load_result < 0:
		push_error("IMM sample load_document returned %d" % load_result)
	_update_status()

func _frame_initial_camera() -> void:
	var deadline_msec: int = Time.get_ticks_msec() + int(SPAWN_AREA_FRAME_WAIT_SECONDS * 1000.0)
	while Time.get_ticks_msec() < deadline_msec:
		if _jump_to_active_spawn_area():
			_initial_camera_framed = true
			return
		await get_tree().create_timer(0.05).timeout
	_update_status()

func _jump_to_active_spawn_area() -> bool:
	var info: Dictionary = viewer.get_active_spawn_area_info()
	if info.is_empty():
		_update_status()
		return false

	var spawn_transform: Transform3D = _spawn_area_transform_from_info(info)
	var desired_basis: Basis = spawn_transform.basis.orthonormalized()
	var head_local_basis: Basis = camera_rig.global_transform.basis.inverse() * camera.global_transform.basis
	var target_basis: Basis = desired_basis * head_local_basis.inverse()

	var head_local_position: Vector3 = camera_rig.to_local(camera.global_position)
	if int(info.get("type", 0)) == 1:
		head_local_position.y = 0.0

	var target_position: Vector3 = spawn_transform.origin - (target_basis * head_local_position)
	camera_rig.global_transform = Transform3D(target_basis, target_position)
	_update_status()
	return true

func _spawn_area_transform_from_info(info: Dictionary) -> Transform3D:
	var transform: Dictionary = info.get("transform", {})
	var basis := Basis(
		transform.get("basis_x", Vector3.RIGHT),
		transform.get("basis_y", Vector3.UP),
		transform.get("basis_z", Vector3.BACK)
	)
	return Transform3D(basis, transform.get("position", Vector3.ZERO))

func _queue_active_camera() -> void:
	var viewport_size: Vector2 = get_viewport().get_visible_rect().size
	var width: int = max(int(viewport_size.x), 1)
	var height: int = max(int(viewport_size.y), 1)
	viewer.queue_render_camera_transform(camera.global_transform, width, height, camera.fov, CAMERA_ID)

func _run_sample_play_smoke() -> void:
	var deadline_msec := Time.get_ticks_msec() + int(SAMPLE_PLAY_SMOKE_TIMEOUT_SECONDS * 1000.0)
	while Time.get_ticks_msec() < deadline_msec:
		if viewer.is_loaded() and viewer.is_sequence_ready() and _initial_camera_framed and viewer.get_layer_count() > 0:
			# Keep the visual evidence deterministic without changing normal Run
			# Project playback. Asset loading duration must not select the frame.
			viewer.pause()
			for _frame in range(SAMPLE_PLAY_SMOKE_SETTLE_FRAMES):
				viewer.set_time(0, 0)
				await get_tree().process_frame
			await RenderingServer.frame_post_draw
			var capture_path := OS.get_environment("IMM_GODOT_SAMPLE_PLAY_CAPTURE")
			if capture_path.is_empty():
				_sample_play_smoke_failed("capture path is empty")
				return
			var image := get_viewport().get_texture().get_image()
			if image == null or image.is_empty():
				_sample_play_smoke_failed("viewport capture is empty")
				return
			var save_result := image.save_png(capture_path)
			if save_result != OK:
				_sample_play_smoke_failed("save_png failed with %d for %s" % [save_result, capture_path])
				return
			var success := "%s passed capture=%s layers=%d camera_ids=%s" % [
				SAMPLE_PLAY_SMOKE_PREFIX,
				capture_path,
				viewer.get_layer_count(),
				str(viewer.get_registered_render_camera_ids()),
			]
			_write_sample_play_smoke_log(success)
			print(success)
			get_tree().quit(0)
			return
		await get_tree().process_frame
	_sample_play_smoke_failed(
		"timed out loaded=%s ready=%s framed=%s layers=%d camera_ids=%s" % [
			str(viewer.is_loaded()),
			str(viewer.is_sequence_ready()),
			str(_initial_camera_framed),
			viewer.get_layer_count(),
			str(viewer.get_registered_render_camera_ids()),
		]
	)

func _sample_play_smoke_failed(reason: String) -> void:
	var failure := "%s failed: %s" % [SAMPLE_PLAY_SMOKE_PREFIX, reason]
	_write_sample_play_smoke_log(failure)
	push_error(failure)
	get_tree().quit(1)

func _write_sample_play_smoke_log(message: String) -> void:
	var log_path := OS.get_environment("IMM_GODOT_SAMPLE_PLAY_LOG")
	if log_path.is_empty():
		log_path = "user://sample_play_smoke.log"
	var log_file := FileAccess.open(log_path, FileAccess.WRITE)
	if log_file == null:
		push_error("%s could not open log path: %s" % [SAMPLE_PLAY_SMOKE_PREFIX, log_path])
		return
	log_file.store_line(message)

func _toggle_first_layer_visibility() -> void:
	if viewer.get_layer_count() <= 0:
		_update_status()
		return

	var first_layer: Dictionary = viewer.get_layer_info(0)
	var layer_id: int = int(first_layer.get("id", -1))
	if layer_id < 0:
		_update_status()
		return

	_first_layer_hidden = not _first_layer_hidden
	viewer.set_layer_visible(layer_id, not _first_layer_hidden)
	_update_status()

func _update_status(_unused: Variant = null) -> void:
	_apply_background_color()

	var lines := PackedStringArray()
	var state: Dictionary = viewer.get_document_state()
	var sequence_ready := bool(state.get("sequence_ready", false))
	var bounds: Dictionary = viewer.get_bounding_box() if sequence_ready else {}
	var spawn_info: Dictionary = viewer.get_active_spawn_area_info() if sequence_ready else {}
	var layer_count: int = int(viewer.get_layer_count()) if sequence_ready else 0
	var spawn_name: String = "none"
	if not spawn_info.is_empty():
		spawn_name = str(spawn_info.get("name", spawn_info.get("id", "unknown")))
	lines.append("IMM Godot Sample")
	lines.append("L load | U unload | Space play/pause | R restart")
	lines.append(", / . skip | [ / ] chapter | ; / ' spawn area | V layer | \\ diagnostic render | WASDQE move")
	lines.append("")
	lines.append("Document: %s" % ("loaded" if viewer.is_loaded() else "not loaded"))
	lines.append("Playback: %s" % ("playing" if viewer.is_playing() else "paused"))
	lines.append("State: load=%d playback=%d ready=%s flags=0x%08x" % [
		int(state.get("loading_state", 0)),
		int(state.get("playback_state", 0)),
		str(state.get("sequence_ready", false)),
		int(state.get("info_flags", 0)),
	])
	lines.append("Chapter: %d / %d" % [viewer.get_current_chapter(), viewer.get_chapter_count()])
	lines.append("Time: %.2fs" % viewer.get_play_time_seconds())
	lines.append("Volume: %d%%" % int(round(viewer.get_volume() * 100.0)))
	lines.append("Render Cameras: %s" % str(viewer.get_registered_render_camera_ids()))
	lines.append("Layers: %d" % layer_count)
	if layer_count > 0:
		var first_layer: Dictionary = viewer.get_layer_info(0)
		var layer_id: int = int(first_layer.get("id", -1))
		var diagnostics: Dictionary = viewer.get_layer_diagnostics(layer_id) if layer_id >= 0 else {}
		lines.append("Layer 0: %s visible=%s override=%s" % [
			str(first_layer.get("name", first_layer.get("id", "unknown"))),
			str(diagnostics.get("is_visible", first_layer.get("is_visible", false))),
			str(diagnostics.get("visibility_override_enabled", false)),
		])
	if not bounds.is_empty():
		lines.append("Bounds: center=%s size=%s" % [bounds.get("center", Vector3.ZERO), bounds.get("size", Vector3.ZERO)])
	lines.append("Path: %s" % viewer.document_path)
	lines.append("Background: %s" % viewer.get_background_color().to_html(false))
	lines.append("Spawn Area Index: %d" % viewer.get_active_spawn_area_index())
	lines.append("Spawn Area: %s" % spawn_name)
	status_label.text = "\n".join(lines)

func _apply_background_color() -> void:
	var color: Color = viewer.get_background_color()
	if color.a <= 0.0:
		color.a = 1.0
	if _has_applied_background_color and color == _last_background_color:
		return
	RenderingServer.set_default_clear_color(color)
	_last_background_color = color
	_has_applied_background_color = true

func _adjust_volume(delta: float) -> void:
	viewer.set_volume(viewer.get_volume() + delta)
	_update_status()
