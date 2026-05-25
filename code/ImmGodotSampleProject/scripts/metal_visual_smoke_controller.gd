extends Node3D

const EXTENSION_PATH := "res://addons/imm_viewer/imm_viewer.gdextension"
const SAMPLE_DOCUMENT_PATH := "res://../../exampleImmFiles/sample1.imm"
const CAMERA_ID := 0
const IMM_RENDERER_API_METAL := 4
const MAX_READY_SECONDS := 12.0
const RENDER_SETTLE_FRAMES := 30
const DEFAULT_RELOAD_CYCLES := 1
const MIN_CONTENT_PIXELS := 512
const MIN_CONTENT_BOUNDS_SIZE := 12
const MIN_LUMA_RANGE := 0.02
const SUCCESS_MARKER := "IMM Godot Metal visual smoke passed"

@onready var camera: Camera3D = $CameraRig/Camera3D
@onready var status_label: Label3D = $StatusLabel

var viewer: Node
var _compositor_effect: Resource
var _has_applied_background_color := false
var _last_background_color := Color.BLACK

func _ready() -> void:
    if OS.get_environment("IMM_GODOT_VISUAL_SMOKE") == "1":
        call_deferred("_run_visual_smoke")
        return

    if not _setup_viewer():
        return
    _setup_compositor()
    viewer.load_document()
    _apply_background_color()
    _update_status("Metal visual scene ready")

func _process(_delta: float) -> void:
    if _compositor_effect == null or viewer == null or not viewer.is_loaded():
        return
    _apply_background_color()
    _queue_active_camera()

func _setup_extension() -> bool:
    if not ClassDB.class_exists("ImmViewerCompositorEffect"):
        var extension_status := GDExtensionManager.load_extension(EXTENSION_PATH)
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
    viewer.document_path = ProjectSettings.globalize_path(SAMPLE_DOCUMENT_PATH)
    viewer.load_on_ready = false
    viewer.auto_play = true
    viewer.auto_queue_render = true
    viewer.renderer_api = IMM_RENDERER_API_METAL
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

    var compositor := Compositor.new()
    compositor.compositor_effects = [_compositor_effect]
    camera.compositor = compositor
    return true

func _run_visual_smoke() -> void:
    var failures: Array[String] = []
    status_label.visible = false

    if not _setup_viewer():
        failures.append("ImmViewerNode setup failed")
    if not _setup_compositor():
        failures.append("ImmViewerCompositorEffect setup failed")

    if viewer == null:
        failures.append("ImmViewerNode was not created")
        _finish_visual_smoke(failures, {}, {}, {}, {}, {})
        return

    if not viewer.is_class("ImmViewerNode"):
        failures.append("Expected native ImmViewerNode, got %s" % viewer.get_class())

    viewer.renderer_api = IMM_RENDERER_API_METAL
    var backend_diagnostics: Dictionary = viewer.get_render_backend_diagnostics()
    if int(backend_diagnostics.get("renderer_api", -1)) != IMM_RENDERER_API_METAL:
        failures.append("ImmViewerNode did not select the Metal renderer API")
    if not bool(backend_diagnostics.get("has_rendering_device", false)):
        failures.append("Metal visual smoke did not expose a RenderingDevice")

    if not viewer.is_render_camera_registered(CAMERA_ID):
        failures.append("camera %d was not auto-registered by ImmViewer" % CAMERA_ID)

    if failures.is_empty() and not viewer.is_loaded():
        var load_result: int = int(viewer.load_document())
        print("IMM Godot Metal visual smoke load result: %d" % load_result)
        if load_result < 0:
            failures.append("load_document returned %d" % load_result)

    var sequence_ready: bool = bool(viewer.is_sequence_ready())
    var ready_deadline_msec: int = Time.get_ticks_msec() + int(MAX_READY_SECONDS * 1000.0)
    while Time.get_ticks_msec() < ready_deadline_msec:
        if viewer.is_loaded():
            _apply_background_color()
            if viewer.is_sequence_ready():
                sequence_ready = true
                if _frame_camera_from_document():
                    _queue_active_camera()
                    break
            _queue_active_camera()
        await get_tree().create_timer(0.05).timeout

    var reload_cycles := _get_env_int("IMM_GODOT_VISUAL_SMOKE_RELOAD_CYCLES", DEFAULT_RELOAD_CYCLES)
    if failures.is_empty() and sequence_ready and reload_cycles > 0:
        sequence_ready = await _exercise_reload_cycles(reload_cycles, failures)

    for _frame in range(RENDER_SETTLE_FRAMES):
        await get_tree().process_frame
        if viewer.is_loaded():
            _apply_background_color()
            if sequence_ready:
                _frame_camera_from_document()
            _queue_active_camera()

    if not viewer.is_loaded():
        failures.append("ImmViewer did not load %s" % str(viewer.get("document_path")))
    if not sequence_ready:
        failures.append("ImmViewer sequence was not ready after %.1f seconds" % MAX_READY_SECONDS)

    var document_state: Dictionary = viewer.get_document_state()
    var document_bounds: Dictionary = viewer.get_bounding_box()
    var render_diagnostics: Dictionary = viewer.get_render_diagnostics()
    if int(render_diagnostics.get("last_camera_id", -1)) != CAMERA_ID:
        failures.append("render diagnostics did not observe camera %d" % CAMERA_ID)

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
        if not bool(compositor_diagnostics.get("last_metal_frame_started", false)):
            failures.append("ImmViewerCompositorEffect did not start a Metal frame")
        if int(compositor_diagnostics.get("last_render_result", -1)) < 0:
            failures.append("ImmGodot_RenderCamera returned %d" % int(compositor_diagnostics.get("last_render_result", -1)))

    var screenshot_path := OS.get_environment("IMM_GODOT_VISUAL_SMOKE_PNG")
    if not screenshot_path.is_empty():
        _apply_background_color()
        await RenderingServer.frame_post_draw
        var image := get_viewport().get_texture().get_image()
        var content_diagnostics := _analyze_content_pixels(image, viewer.get_background_color())
        print("IMM Godot Metal visual smoke content diagnostics: %s" % str(content_diagnostics))
        if float(content_diagnostics.get("luma_range", 0.0)) < MIN_LUMA_RANGE:
            failures.append("visual smoke PNG was too flat: luma range %.5f" % float(content_diagnostics.get("luma_range", 0.0)))
        if int(content_diagnostics.get("content_pixels", 0)) < MIN_CONTENT_PIXELS:
            failures.append("visual smoke PNG had only %d content pixels" % int(content_diagnostics.get("content_pixels", 0)))
        if int(content_diagnostics.get("content_bounds_width", 0)) < MIN_CONTENT_BOUNDS_SIZE or int(content_diagnostics.get("content_bounds_height", 0)) < MIN_CONTENT_BOUNDS_SIZE:
            failures.append("visual smoke PNG content bounds were too small: %sx%s" % [
                str(content_diagnostics.get("content_bounds_width", 0)),
                str(content_diagnostics.get("content_bounds_height", 0)),
            ])
        var save_result := image.save_png(screenshot_path)
        if save_result != OK:
            failures.append("Failed to save visual smoke PNG %s: %d" % [screenshot_path, int(save_result)])
        else:
            print("IMM Godot Metal visual smoke PNG: %s" % screenshot_path)

    _finish_visual_smoke(failures, backend_diagnostics, document_state, document_bounds, render_diagnostics, compositor_diagnostics)

func _finish_visual_smoke(
    failures: Array[String],
    backend_diagnostics: Dictionary,
    document_state: Dictionary,
    document_bounds: Dictionary,
    render_diagnostics: Dictionary,
    compositor_diagnostics: Dictionary
) -> void:
    print("IMM Godot Metal visual smoke backend diagnostics: %s" % str(backend_diagnostics))
    print("IMM Godot Metal visual smoke document state: %s" % str(document_state))
    print("IMM Godot Metal visual smoke document bounds: %s" % str(document_bounds))
    print("IMM Godot Metal visual smoke render diagnostics: %s" % str(render_diagnostics))
    print("IMM Godot Metal visual smoke compositor diagnostics: %s" % str(compositor_diagnostics))

    if failures.is_empty():
        print(SUCCESS_MARKER)
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

func _exercise_reload_cycles(cycle_count: int, failures: Array[String]) -> bool:
    var stayed_ready := true
    for cycle_index in range(cycle_count):
        viewer.unload_document()
        await get_tree().process_frame
        if viewer.is_loaded():
            failures.append("Metal visual smoke reload cycle %d did not unload the document" % [cycle_index + 1])
            stayed_ready = false
            continue

        _queue_active_camera()
        var load_result: int = int(viewer.load_document())
        print("IMM Godot Metal visual smoke reload cycle %d load result: %d" % [cycle_index + 1, load_result])
        if load_result < 0:
            failures.append("Metal visual smoke reload cycle %d load_document returned %d" % [cycle_index + 1, load_result])
            stayed_ready = false
            continue

        var ready := false
        var ready_deadline_msec: int = Time.get_ticks_msec() + int(MAX_READY_SECONDS * 1000.0)
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
            failures.append("Metal visual smoke reload cycle %d sequence was not ready after %.1f seconds" % [
                cycle_index + 1,
                MAX_READY_SECONDS,
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

func _frame_camera_from_document() -> bool:
    var spawn_info: Dictionary = viewer.get_active_spawn_area_info()
    if not spawn_info.is_empty():
        var spawn_transform := _spawn_area_transform_from_info(spawn_info)
        camera.global_transform = spawn_transform
        camera.near = 0.01
        camera.far = 10000.0
        camera.force_update_transform()
        print("IMM Godot Metal visual smoke camera framed from spawn area: position=%s basis=%s" % [
            str(camera.global_position),
            str(camera.global_transform.basis),
        ])
        return true

    var bounds: Dictionary = viewer.get_bounding_box()
    if bounds.is_empty():
        return false

    var center: Vector3 = bounds.get("center", Vector3.ZERO)
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
    print("IMM Godot Metal visual smoke camera framed: position=%s target=%s radius=%.3f far=%.3f" % [
        str(camera.global_position),
        str(center),
        radius,
        camera.far,
    ])
    return true

func _spawn_area_transform_from_info(info: Dictionary) -> Transform3D:
    var transform: Dictionary = info.get("transform", {})
    var basis := Basis(
        transform.get("basis_x", Vector3.RIGHT),
        transform.get("basis_y", Vector3.UP),
        transform.get("basis_z", Vector3.BACK)
    )
    return Transform3D(basis.orthonormalized(), transform.get("position", Vector3.ZERO))

func _analyze_content_pixels(image: Image, background: Color) -> Dictionary:
    var width := image.get_width()
    var height := image.get_height()
    var background_rgb := Vector3(background.r, background.g, background.b)
    var content_pixels := 0
    var min_x := width
    var min_y := height
    var max_x := -1
    var max_y := -1
    var min_luma := 1.0
    var max_luma := 0.0

    for y in range(height):
        for x in range(width):
            var color := image.get_pixel(x, y)
            var rgb := Vector3(color.r, color.g, color.b)
            var distance := (rgb - background_rgb).length()
            var luma := color.r * 0.2126 + color.g * 0.7152 + color.b * 0.0722
            min_luma = min(min_luma, luma)
            max_luma = max(max_luma, luma)
            if distance > 0.08:
                content_pixels += 1
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)

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
    }

func _update_status(message: String) -> void:
    status_label.text = "%s\nRenderer API: Metal\nDocument: %s" % [
        message,
        "loaded" if viewer.is_loaded() else "not loaded",
    ]

func _get_env_int(name: String, default_value: int) -> int:
    var value := OS.get_environment(name)
    if value.is_empty():
        return default_value
    if not value.is_valid_int():
        return default_value
    return max(int(value), 0)
