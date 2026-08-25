extends Node3D

const LOG_PREFIX := "[IMM_GODOT_WINDOWS_XR_SAMPLE_20260825]"
const EXTENSION_PATH := "res://addons/imm_viewer/imm_viewer.gdextension"
const REPOSITORY_SAMPLE_PATH := "res://../../exampleImmFiles/sample1.imm"
const PROJECT_SAMPLE_PATH := "res://sample1.imm"
const CAMERA_ID := 0
const IMM_RENDERER_API_VULKAN := 5
const WARMUP_FRAMES := 12
const READY_TIMEOUT_SECONDS := 30.0

@onready var xr_origin: XROrigin3D = $XROrigin3D
@onready var xr_camera: XRCamera3D = $XROrigin3D/XRCamera3D
@onready var status_label: Label = $StatusLayer/StatusPanel/StatusLabel

var _viewer: Node
var _compositor_effect: Resource
var _spawn_applied := false
var _last_status := ""
var _startup_failed := false

func _ready() -> void:
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
	if not _load_extension() or not _initialize_openxr() or not _create_viewer_and_compositor():
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
			_set_status("IMM is playing in OpenXR\nR: recenter  Space: pause/play  [ / ]: spawn area")
			print("%s ready document=%s stereo_multipass=1 rendered_eye_mask=3" % [LOG_PREFIX, document_path])
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
	print("%s openxr_initialized interface=%s" % [LOG_PREFIX, xr_interface.get_name()])
	return true

func _create_viewer_and_compositor() -> bool:
	_viewer = ClassDB.instantiate("ImmViewerNode")
	if _viewer == null:
		_fail("Could not create ImmViewerNode.")
		return false
	_viewer.name = "ImmViewer"
	_viewer.load_on_ready = false
	_viewer.auto_play = true
	_viewer.auto_queue_render = true
	_viewer.renderer_api = IMM_RENDERER_API_VULKAN
	_viewer.render_camera_path = NodePath("../XROrigin3D/XRCamera3D")
	_viewer.log_file_path = "user://imm_windows_xr_sample.log"
	add_child(_viewer)

	_compositor_effect = ClassDB.instantiate("ImmViewerCompositorEffect")
	if _compositor_effect == null:
		_fail("Could not create ImmViewerCompositorEffect.")
		return false
	var compositor := Compositor.new()
	compositor.compositor_effects = [_compositor_effect]
	xr_camera.compositor = compositor
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

func _find_sample_document() -> String:
	for candidate in [PROJECT_SAMPLE_PATH, REPOSITORY_SAMPLE_PATH]:
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
	var target_basis := desired_basis * head_local_basis.inverse()
	var head_local_position := xr_origin.to_local(xr_camera.global_position)
	if int(info.get("type", 0)) == 1:
		head_local_position.y = 0.0
	var desired_position: Vector3 = transform.get("position", Vector3.ZERO)
	xr_origin.global_transform = Transform3D(target_basis, desired_position - target_basis * head_local_position)
	print("%s applied_spawn_area id=%s" % [LOG_PREFIX, str(info.get("id", -1))])
	return true

func _fail(message: String) -> void:
	_startup_failed = true
	_set_status("XR sample could not start:\n%s" % message, true)

func _set_status(message: String, is_error := false) -> void:
	status_label.text = message
	status_label.modulate = Color(1.0, 0.55, 0.55) if is_error else Color.WHITE
	if message == _last_status:
		return
	_last_status = message
	if is_error:
		push_error("%s %s" % [LOG_PREFIX, message.replace("\n", " ")])
	else:
		print("%s %s" % [LOG_PREFIX, message.replace("\n", " ")])
