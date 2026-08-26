extends Node3D

const LOG_PREFIX := "[IMM_GODOT_WINDOWS_XR_SAMPLE_20260825]"
const DEBUG_LOG_PATH := "user://xr_debug.log"
const EXTENSION_PATH := "res://addons/imm_viewer/imm_viewer.gdextension"
const REPOSITORY_SAMPLE_PATH := "res://../../exampleImmFiles/sample1.imm"
const PROJECT_SAMPLE_PATH := "res://sample1.imm"
const CAMERA_ID := 0
const WARMUP_FRAMES := 12
const READY_TIMEOUT_SECONDS := 30.0
const XR_CAPTURE_ENV := "IMM_GODOT_XR_FRAME_CAPTURE_PATH"
const XR_MIRROR_CAPTURE_ENV := "IMM_GODOT_XR_MIRROR_CAPTURE_PATH"

@onready var xr_origin: XROrigin3D = $XROrigin3D
@onready var xr_camera: XRCamera3D = $XROrigin3D/XRCamera3D
@onready var status_label: Label = $StatusLayer/StatusPanel/StatusLabel
@onready var _viewer: Node = get_node_or_null("ImmViewer")

var _compositor_effect: Resource
var _spawn_applied := false
var _last_status := ""
var _startup_failed := false

func _ready() -> void:
	var debug_log := FileAccess.open(DEBUG_LOG_PATH, FileAccess.WRITE)
	if debug_log != null:
		debug_log.store_line("%s started_unix=%d" % [LOG_PREFIX, Time.get_unix_time_from_system()])
		debug_log.close()
	_set_status("Starting Windows OpenXR…")
	call_deferred("_start_xr_sample")

func _process(_delta: float) -> void:
	if _viewer == null or _startup_failed:
		return
	_queue_xr_camera()
	if _viewer.is_loaded():
		RenderingServer.set_default_clear_color(_viewer.get_background_color())
	if _viewer.is_sequence_ready() and not _spawn_applied:
		_spawn_applied = _move_origin_to_active_spawn()

func _unhandled_key_input(event: InputEvent) -> void:
	if not event is InputEventKey or not event.pressed or event.echo or _viewer == null:
		return
	match event.keycode:
		KEY_R:
			_spawn_applied = _move_origin_to_active_spawn()
		KEY_SPACE:
			_viewer.toggle_pause()
		KEY_BRACKETLEFT:
			_viewer.previous_spawn_area()
			_spawn_applied = _move_origin_to_active_spawn()
		KEY_BRACKETRIGHT:
			_viewer.next_spawn_area()
			_spawn_applied = _move_origin_to_active_spawn()

func _start_xr_sample() -> void:
	if not _load_extension() or not _bind_authored_imm_resources() or not _initialize_openxr():
		return

	for _frame in range(WARMUP_FRAMES):
		_queue_xr_camera()
		await get_tree().process_frame

	var document_path := _find_sample_document()
	if document_path.is_empty():
		_fail("sample1.imm was not found. Put it beside project.godot and run again.")
		return
	_viewer.document_path = document_path
	var load_result: int = int(_viewer.load_document())
	if load_result < 0:
		_fail("IMM load failed with result %d for %s" % [load_result, document_path])
		return

	_set_status("OpenXR started; loading IMM content…")
	var deadline_msec := Time.get_ticks_msec() + int(READY_TIMEOUT_SECONDS * 1000.0)
	while Time.get_ticks_msec() < deadline_msec:
		_queue_xr_camera()
		if _viewer.is_loaded() and _viewer.is_sequence_ready() and _xr_stereo_rendering_is_ready():
			_spawn_applied = _move_origin_to_active_spawn()
			RenderingServer.set_default_clear_color(_viewer.get_background_color())
			if not OS.get_environment(XR_CAPTURE_ENV).is_empty():
				for _frame in range(3):
					_queue_xr_camera()
					await get_tree().process_frame
				await RenderingServer.frame_post_draw
				var capture_succeeded := _write_xr_frame_capture()
				get_tree().quit(0 if capture_succeeded else 1)
				return
			_set_status("IMM is playing in OpenXR\nR: recenter  Space: pause/play  [ / ]: spawn area")
			_log("ready document=%s stereo_multipass=1 rendered_eye_mask=3" % document_path)
			return
		await get_tree().create_timer(0.05).timeout

	var diagnostics: Dictionary = _compositor_effect.get_diagnostics() if _compositor_effect != null else {}
	_fail("IMM did not become stereo-ready within %.1f seconds. Diagnostics: %s" % [READY_TIMEOUT_SECONDS, str(diagnostics)])

func _load_extension() -> bool:
	if not ClassDB.class_exists("ImmViewerCompositorEffect"):
		var extension_status: int = GDExtensionManager.load_extension(EXTENSION_PATH)
		if extension_status != OK and extension_status != ERR_ALREADY_EXISTS:
			_fail("Could not load the IMM GDExtension (%s). Build the Windows addon first." % error_string(extension_status))
			return false
	if not ClassDB.class_exists("ImmViewerNode") or not ClassDB.class_exists("ImmViewerCompositorEffect"):
		_fail("The IMM native classes were not registered by the GDExtension.")
		return false
	return true

func _initialize_openxr() -> bool:
	var xr_interface := XRServer.find_interface("OpenXR")
	if xr_interface == null:
		_fail("Godot could not find OpenXR. Use a standard Godot build with OpenXR support.")
		return false
	if not xr_interface.is_initialized() and not xr_interface.initialize():
		_fail("OpenXR initialization failed. Check the active PC OpenXR runtime and headset state.")
		return false
	XRServer.primary_interface = xr_interface
	get_viewport().use_xr = true
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	_log("openxr_initialized interface=%s" % xr_interface.get_name())
	return true

func _bind_authored_imm_resources() -> bool:
	if _viewer == null or not _viewer.is_class("ImmViewerNode"):
		_fail("XRSampleScene must contain an ImmViewerNode named ImmViewer.")
		return false
	if xr_camera.compositor == null:
		_fail("XRCamera3D must have a Compositor resource.")
		return false
	for effect in xr_camera.compositor.compositor_effects:
		if effect != null and effect.is_class("ImmViewerCompositorEffect"):
			_compositor_effect = effect
			break
	if _compositor_effect == null:
		_fail("XRCamera3D's Compositor must contain an ImmViewerCompositorEffect.")
		return false
	return true

func _xr_stereo_rendering_is_ready() -> bool:
	if _compositor_effect == null:
		return false
	var diagnostics: Dictionary = _compositor_effect.get_diagnostics()
	return (
		int(diagnostics.get("last_view_count", 0)) >= 2
		and bool(diagnostics.get("last_used_xr_render_data", false))
		and bool(diagnostics.get("last_submitted_stereo_matrices", false))
		and int(diagnostics.get("last_rendered_eye_mask", 0)) == 3
	)

func _write_xr_frame_capture() -> bool:
	var capture_path := OS.get_environment(XR_CAPTURE_ENV).replace("\\", "/")
	var capture: Dictionary = _compositor_effect.get_last_xr_frame_capture()
	if not bool(capture.get("available", false)):
		_fail("XR frame capture was requested but no XR matrices were available.")
		return false
	var serializable := {
		"status": "captured",
		"captured_unix": Time.get_unix_time_from_system(),
		"source": "Godot RenderSceneData OpenXR frame",
	}
	for key in capture.keys():
		var value: Variant = capture[key]
		serializable[key] = Array(value) if value is PackedFloat32Array else value
	var capture_directory := capture_path.get_base_dir()
	if not capture_directory.is_empty() and DirAccess.make_dir_recursive_absolute(capture_directory) != OK:
		_fail("Could not create XR capture directory: %s" % capture_directory)
		return false
	var capture_file := FileAccess.open(capture_path, FileAccess.WRITE)
	if capture_file == null:
		_fail("Could not write XR frame capture: %s" % capture_path)
		return false
	capture_file.store_string(JSON.stringify(serializable, "\t"))
	capture_file.close()
	var mirror_path := OS.get_environment(XR_MIRROR_CAPTURE_ENV).replace("\\", "/")
	if not mirror_path.is_empty():
		var mirror_image := get_viewport().get_texture().get_image()
		if mirror_image == null or mirror_image.is_empty() or mirror_image.save_png(mirror_path) != OK:
			_fail("Could not write XR mirror capture: %s" % mirror_path)
			return false
	_log("captured_xr_frame path=%s" % capture_path)
	_set_status("Captured one OpenXR frame for offline replay")
	return true

func _find_sample_document() -> String:
	var configured_path: String = str(_viewer.document_path) if _viewer != null else ""
	for candidate in [configured_path, PROJECT_SAMPLE_PATH, REPOSITORY_SAMPLE_PATH]:
		if candidate.is_empty():
			continue
		if FileAccess.file_exists(candidate):
			return candidate
	return ""

func _queue_xr_camera() -> void:
	if _viewer == null:
		return
	var viewport_size := get_viewport().get_visible_rect().size
	_viewer.queue_render_camera_transform(
		xr_camera.global_transform,
		maxi(int(viewport_size.x), 1),
		maxi(int(viewport_size.y), 1),
		xr_camera.fov,
		CAMERA_ID
	)

func _move_origin_to_active_spawn() -> bool:
	if _viewer == null or not _viewer.is_sequence_ready():
		return false
	var info: Dictionary = _viewer.get_active_spawn_area_info()
	if info.is_empty():
		return false
	var transform: Dictionary = info.get("transform", {})
	var desired_basis := Basis(
		transform.get("basis_x", Vector3.RIGHT),
		transform.get("basis_y", Vector3.UP),
		transform.get("basis_z", Vector3.BACK)
	).orthonormalized()
	var head_local_basis := xr_origin.global_transform.basis.inverse() * xr_camera.global_transform.basis
	var is_floor_spawn := int(info.get("type", 0)) == 1
	# Recenter only around Godot's vertical axis. Applying the headset's current
	# pitch or roll to XROrigin3D tilts the world whenever recentering occurs while
	# the user is not looking exactly level.
	var target_basis := _yaw_basis(desired_basis) * _yaw_basis(head_local_basis).inverse()
	var head_local_position := xr_origin.to_local(xr_camera.global_position)
	if is_floor_spawn:
		head_local_position.y = 0.0
	var local_spawn_position: Vector3 = transform.get("position", Vector3.ZERO)
	# Spawn-area info is document-local. Rendering applies document_transform to
	# the IMM world, so recentering must resolve the spawn position through it too.
	var desired_position: Vector3 = _viewer.get_document_transform() * local_spawn_position
	var target_transform := Transform3D(
		target_basis,
		desired_position - target_basis * head_local_position
	)
	xr_origin.global_transform = target_transform
	var resulting_head_position := target_transform * head_local_position
	var residual_millimetres := (resulting_head_position - desired_position) * 1000.0
	_log(
		"applied_spawn_area id=%s rotation_mode=%s local_position=%s world_position=%s head_local_position=%s residual_mm=%s"
		% [
			str(info.get("id", -1)),
			"yaw_only",
			str(local_spawn_position),
			str(desired_position),
			str(head_local_position),
			str(residual_millimetres),
		]
	)
	return true

func _yaw_basis(source: Basis) -> Basis:
	var back := Vector3(source.z.x, 0.0, source.z.z)
	if back.length_squared() < 0.000001:
		back = Vector3.BACK
	else:
		back = back.normalized()
	var right := Vector3.UP.cross(back).normalized()
	return Basis(right, Vector3.UP, back)

func _fail(message: String) -> void:
	_startup_failed = true
	_set_status("XR sample could not start:\n%s" % message, true)

func _set_status(message: String, is_error := false) -> void:
	status_label.text = message
	status_label.modulate = Color(1.0, 0.55, 0.55) if is_error else Color.WHITE
	if message == _last_status:
		return
	_last_status = message
	_log(message.replace("\n", " "))
	if is_error:
		push_error("%s %s" % [LOG_PREFIX, message.replace("\n", " ")])

func _log(message: String) -> void:
	var line := "%s %s" % [LOG_PREFIX, message]
	print(line)
	var debug_log := FileAccess.open(DEBUG_LOG_PATH, FileAccess.READ_WRITE)
	if debug_log == null:
		return
	debug_log.seek_end()
	debug_log.store_line(line)
