extends Node3D

const EXTENSION_PATH := "res://addons/imm_viewer/imm_viewer.gdextension"
const SAMPLE_DOCUMENT_PATH := "res://../../exampleImmFiles/sample1.imm"
const CAMERA_ID := 0
const IMM_RENDERER_API_VULKAN := 5
const MAX_READY_SECONDS := 20.0
const SETTLE_FRAMES := 12

@onready var xr_origin: XROrigin3D = $XROrigin3D
@onready var xr_camera: XRCamera3D = $XROrigin3D/XRCamera3D

var _viewer: Node
var _compositor_effect: Resource

func _ready() -> void:
	call_deferred("_run")

func _run() -> void:
	var failures: Array[String] = []
	print("IMM_GODOT_OPENXR_SMOKE begin")

	var xr_interface: XRInterface = XRServer.find_interface("OpenXR")
	if xr_interface == null:
		failures.append("OpenXR interface was not found")
	else:
		print("IMM_GODOT_OPENXR_SMOKE interface_name=%s initialized_before=%s" % [
			xr_interface.get_name(),
			str(xr_interface.is_initialized()),
		])
		if not xr_interface.is_initialized():
			var initialized: bool = xr_interface.initialize()
			print("IMM_GODOT_OPENXR_SMOKE initialize_result=%s" % str(initialized))
			if not initialized:
				failures.append("OpenXR interface initialize() returned false")
		if xr_interface.is_initialized():
			get_viewport().use_xr = true
			XRServer.primary_interface = xr_interface
			print("IMM_GODOT_OPENXR_SMOKE interface_initialized")

	if not get_viewport().use_xr:
		failures.append("Viewport use_xr was not enabled")
	else:
		print("IMM_GODOT_OPENXR_SMOKE viewport_use_xr=1")
	if XRServer.primary_interface == null:
		failures.append("XRServer.primary_interface was not assigned")
	else:
		print("IMM_GODOT_OPENXR_SMOKE primary_interface=%s" % XRServer.primary_interface.get_name())

	if not _setup_viewer(failures):
		_finish(failures, {}, {}, {})
		return

	for _frame in range(SETTLE_FRAMES):
		_queue_xr_camera()
		await get_tree().process_frame

	var load_result: int = int(_viewer.load_document())
	print("IMM_GODOT_OPENXR_SMOKE load_document_result=%d" % load_result)
	if load_result < 0:
		failures.append("load_document returned %d" % load_result)

	var ready: bool = false
	var deadline_msec: int = Time.get_ticks_msec() + int(MAX_READY_SECONDS * 1000.0)
	while Time.get_ticks_msec() < deadline_msec:
		_queue_xr_camera()
		if _viewer.is_loaded() and _viewer.is_sequence_ready():
			ready = true
			break
		await get_tree().create_timer(0.05).timeout
	if not _viewer.is_loaded():
		failures.append("ImmViewer did not load %s" % str(_viewer.get("document_path")))
	if not ready:
		failures.append("ImmViewer sequence was not ready after %.1f seconds" % MAX_READY_SECONDS)
	else:
		print("IMM_GODOT_OPENXR_SMOKE document_ready")

	for _frame in range(SETTLE_FRAMES):
		_queue_xr_camera()
		await get_tree().process_frame

	var render_diagnostics: Dictionary = _viewer.get_render_diagnostics()
	var compositor_diagnostics: Dictionary = _compositor_effect.get_diagnostics() if _compositor_effect != null else {}
	if int(render_diagnostics.get("last_camera_id", -1)) != CAMERA_ID:
		failures.append("render diagnostics did not observe XR camera %d" % CAMERA_ID)
	if int(render_diagnostics.get("adapter_before_render_count", 0)) <= 0:
		failures.append("render adapter before-render callback did not run")
	if int(render_diagnostics.get("adapter_after_render_count", 0)) <= 0:
		failures.append("render adapter after-render callback did not run")
	if compositor_diagnostics.is_empty():
		failures.append("ImmViewerCompositorEffect diagnostics were empty")
	else:
		if int(compositor_diagnostics.get("callback_count", 0)) <= 0:
			failures.append("ImmViewerCompositorEffect render callback did not run")
		if not bool(compositor_diagnostics.get("last_had_scene_buffers", false)):
			failures.append("ImmViewerCompositorEffect did not receive RenderSceneBuffersRD")
		if not bool(compositor_diagnostics.get("last_vulkan_frame_started", false)):
			failures.append("ImmViewerCompositorEffect did not start a Vulkan frame")
		if int(compositor_diagnostics.get("last_render_result", -1)) < 0:
			failures.append("ImmGodot_RenderCamera returned %d" % int(compositor_diagnostics.get("last_render_result", -1)))

	print("IMM_GODOT_OPENXR_SMOKE xr_camera_transform=%s" % str(xr_camera.global_transform))
	_finish(failures, render_diagnostics, _viewer.get_document_state(), compositor_diagnostics)

func _setup_viewer(failures: Array[String]) -> bool:
	if not ClassDB.class_exists("ImmViewerCompositorEffect"):
		var extension_status: int = GDExtensionManager.load_extension(EXTENSION_PATH)
		if extension_status != OK and extension_status != ERR_ALREADY_EXISTS:
			failures.append("Failed to load %s: %d" % [EXTENSION_PATH, int(extension_status)])
			return false
	if not ClassDB.class_exists("ImmViewerNode"):
		failures.append("ImmViewerNode is not available after loading %s" % EXTENSION_PATH)
		return false

	_viewer = ClassDB.instantiate("ImmViewerNode")
	if _viewer == null:
		failures.append("Failed to instantiate ImmViewerNode")
		return false
	_viewer.name = "ImmViewer"
	_viewer.document_path = SAMPLE_DOCUMENT_PATH
	_viewer.load_on_ready = false
	_viewer.auto_play = true
	_viewer.auto_queue_render = true
	_viewer.renderer_api = IMM_RENDERER_API_VULKAN
	_viewer.render_camera_path = NodePath("../XROrigin3D/XRCamera3D")
	add_child(_viewer)

	_compositor_effect = ClassDB.instantiate("ImmViewerCompositorEffect")
	if _compositor_effect == null:
		failures.append("Failed to instantiate ImmViewerCompositorEffect")
		return false
	var compositor: Compositor = Compositor.new()
	compositor.compositor_effects = [_compositor_effect]
	xr_camera.compositor = compositor
	print("IMM_GODOT_OPENXR_SMOKE viewer_initialized")
	return true

func _queue_xr_camera() -> void:
	if _viewer == null:
		return
	var viewport_size: Vector2 = get_viewport().get_visible_rect().size
	var width: int = maxi(int(viewport_size.x), 1)
	var height: int = maxi(int(viewport_size.y), 1)
	_viewer.queue_render_camera_transform(xr_camera.global_transform, width, height, xr_camera.fov, CAMERA_ID)

func _finish(
	failures: Array[String],
	render_diagnostics: Dictionary,
	document_state: Dictionary,
	compositor_diagnostics: Dictionary
) -> void:
	print("IMM_GODOT_OPENXR_SMOKE render_diagnostics=%s" % str(render_diagnostics))
	print("IMM_GODOT_OPENXR_SMOKE document_state=%s" % str(document_state))
	print("IMM_GODOT_OPENXR_SMOKE compositor_diagnostics=%s" % str(compositor_diagnostics))
	if failures.is_empty():
		print("IMM_GODOT_OPENXR_SMOKE frame_submitted")
		print("IMM Godot OpenXR VR smoke passed")
		get_tree().quit(0)
		return
	for failure in failures:
		push_error(failure)
	print("IMM_GODOT_OPENXR_SMOKE failed")
	get_tree().quit(1)
