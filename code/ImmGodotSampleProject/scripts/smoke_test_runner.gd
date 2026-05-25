extends SceneTree

const EXPECTED_RENDERER := "gl_compatibility"
const EXTENSION_PATH := "res://addons/imm_viewer/imm_viewer.gdextension"
const SAMPLE_SCENE := "res://scenes/SampleScene.tscn"
const NATIVE_SCENE := "res://scenes/NativeSmokeScene.tscn"
const CAMERA_ID := 0
const MAX_READY_SECONDS := 12.0

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var failures: Array[String] = []

    var renderer := str(ProjectSettings.get_setting("rendering/renderer/rendering_method", ""))
    if renderer != EXPECTED_RENDERER:
        failures.append("Expected renderer %s, got %s" % [EXPECTED_RENDERER, renderer])

    var expected_native := OS.get_environment("IMM_GODOT_EXPECT_NATIVE") == "1"
    if expected_native and not ClassDB.class_exists("ImmViewerNode"):
        var extension_status := GDExtensionManager.load_extension(EXTENSION_PATH)
        if extension_status != OK and extension_status != ERR_ALREADY_EXISTS:
            failures.append("Failed to load %s: %d" % [EXTENSION_PATH, int(extension_status)])

    var scene_path := OS.get_environment("IMM_GODOT_SMOKE_SCENE")
    if scene_path.is_empty():
        scene_path = NATIVE_SCENE if expected_native else SAMPLE_SCENE

    var packed_scene := load(scene_path)
    if packed_scene == null:
        failures.append("Failed to load %s" % scene_path)
        _finish(failures)
        return

    var scene: Node = packed_scene.instantiate()
    root.add_child(scene)
    await process_frame

    var viewer := scene.get_node_or_null("ImmViewer")
    var camera := scene.get_node_or_null("CameraRig/Camera3D")
    if viewer == null:
        failures.append("Sample scene is missing ImmViewer")
    if camera == null:
        failures.append("Sample scene is missing CameraRig/Camera3D")
    if viewer == null or camera == null:
        _finish(failures)
        return

    if expected_native and not viewer.is_class("ImmViewerNode"):
        failures.append("Expected native ImmViewerNode, got %s" % viewer.get_class())

    var required_methods := [
        "register_render_camera",
        "unregister_render_camera",
        "is_render_camera_registered",
        "get_registered_render_camera_ids",
        "get_render_diagnostics",
        "get_render_backend_diagnostics",
        "queue_render_camera_transform",
        "queue_render_last_camera",
        "load_document",
        "is_loaded",
        "is_playing",
        "pause",
        "play",
        "toggle_pause",
        "restart",
        "skip_forward",
        "skip_back",
        "set_volume",
        "get_volume",
        "set_document_transform",
        "get_document_transform",
        "get_background_color",
        "get_document_state",
        "is_sequence_ready",
        "get_chapter_count",
        "get_current_chapter",
        "get_bounding_box",
        "get_layer_count",
        "get_layer_info",
        "get_layer_diagnostics",
        "set_layer_visible",
        "clear_layer_visibility_override",
        "set_layer_opacity",
        "set_layer_transform",
        "clear_layer_transform_override",
        "get_spawn_area_ids",
        "get_active_spawn_area_index",
        "get_active_spawn_area_info",
    ]
    for method_name in required_methods:
        if not viewer.has_method(method_name):
            failures.append("ImmViewer is missing method %s" % method_name)

    if not failures.is_empty():
        _finish(failures)
        return

    if not bool(viewer.get("auto_queue_render")):
        failures.append("ImmViewer auto_queue_render is not enabled")
    if str(viewer.get("render_camera_path")) != "../CameraRig/Camera3D":
        failures.append("ImmViewer render_camera_path does not target ../CameraRig/Camera3D")
    if not viewer.is_render_camera_registered(CAMERA_ID):
        failures.append("camera %d was not auto-registered by ImmViewer" % CAMERA_ID)

    var backend_diagnostics: Dictionary = viewer.get_render_backend_diagnostics()
    if backend_diagnostics.is_empty():
        failures.append("get_render_backend_diagnostics returned an empty Dictionary")
    if str(backend_diagnostics.get("project_rendering_method", "")) != EXPECTED_RENDERER:
        failures.append("render backend diagnostics reported renderer %s" % str(backend_diagnostics.get("project_rendering_method", "")))
    if bool(backend_diagnostics.get("metal_adapter_candidate", true)):
        failures.append("Compatibility smoke path should not report a Metal adapter candidate")
    if int(backend_diagnostics.get("renderer_api", -1)) < 0:
        failures.append("render backend diagnostics did not report renderer_api")

    if not viewer.is_loaded():
        var load_result: int = int(viewer.load_document())
        await process_frame
        await process_frame
        if load_result < 0:
            failures.append("load_document returned %d" % load_result)
    if not viewer.is_loaded():
        failures.append("ImmViewer did not load %s" % str(viewer.get("document_path")))

    var document_state: Dictionary = viewer.get_document_state()
    if document_state.is_empty():
        failures.append("get_document_state returned an empty Dictionary")
    if not document_state.has("loading_state") or not document_state.has("playback_state"):
        failures.append("get_document_state is missing loading/playback state keys")

    var test_document_transform := Transform3D(Basis.IDENTITY, Vector3(1.25, -0.5, 2.0))
    viewer.set_document_transform(test_document_transform)
    var observed_document_transform: Transform3D = viewer.get_document_transform()
    if not _vectors_close(observed_document_transform.origin, test_document_transform.origin):
        failures.append("get_document_transform origin %s did not match set_document_transform origin %s" % [
            str(observed_document_transform.origin),
            str(test_document_transform.origin),
        ])
    viewer.set_document_transform(Transform3D.IDENTITY)

    var background_color: Color = viewer.get_background_color()
    if background_color.a <= 0.0:
        failures.append("get_background_color returned a fully transparent color")
        background_color.a = 1.0
    RenderingServer.set_default_clear_color(background_color)
    var clear_color: Color = RenderingServer.get_default_clear_color()
    if not _colors_close(clear_color, background_color):
        failures.append("Godot default clear color %s did not match IMM background color %s" % [
            clear_color.to_html(false),
            background_color.to_html(false),
        ])

    var chapter_count: int = int(viewer.get_chapter_count())
    var current_chapter: int = int(viewer.get_current_chapter())
    if chapter_count < 0:
        failures.append("get_chapter_count returned %d" % chapter_count)
    if current_chapter < 0:
        failures.append("get_current_chapter returned %d" % current_chapter)

    var bounds: Dictionary = viewer.get_bounding_box()
    if not bounds.is_empty():
        for key in ["min", "max", "center", "size"]:
            if not bounds.has(key):
                failures.append("get_bounding_box result is missing %s" % key)

    var layer_count: int = int(viewer.get_layer_count())
    if layer_count < 0:
        failures.append("get_layer_count returned %d" % layer_count)
    if layer_count > 0:
        var layer_info: Dictionary = viewer.get_layer_info(0)
        if layer_info.is_empty():
            failures.append("get_layer_info(0) returned an empty Dictionary")
        else:
            var layer_id: int = int(layer_info.get("id", -1))
            if layer_id < 0:
                failures.append("get_layer_info(0) did not include a valid id")
            else:
                var layer_diagnostics: Dictionary = viewer.get_layer_diagnostics(layer_id)
                if layer_diagnostics.is_empty():
                    failures.append("get_layer_diagnostics(%d) returned an empty Dictionary" % layer_id)
                if not bool(viewer.set_layer_visible(layer_id, false)):
                    failures.append("set_layer_visible(%d, false) failed" % layer_id)
                layer_diagnostics = viewer.get_layer_diagnostics(layer_id)
                if not bool(layer_diagnostics.get("visibility_override_enabled", false)):
                    failures.append("set_layer_visible(%d, false) did not enable visibility override" % layer_id)
                if bool(layer_diagnostics.get("is_visible", true)):
                    failures.append("set_layer_visible(%d, false) did not report invisible diagnostics" % layer_id)
                if not bool(viewer.clear_layer_visibility_override(layer_id)):
                    failures.append("clear_layer_visibility_override(%d) failed" % layer_id)
                layer_diagnostics = viewer.get_layer_diagnostics(layer_id)
                if bool(layer_diagnostics.get("visibility_override_enabled", false)):
                    failures.append("clear_layer_visibility_override(%d) left visibility override enabled" % layer_id)
                if not bool(viewer.set_layer_opacity(layer_id, 0.42)):
                    failures.append("set_layer_opacity(%d, 0.42) failed" % layer_id)
                layer_diagnostics = viewer.get_layer_diagnostics(layer_id)
                if not bool(layer_diagnostics.get("opacity_override_enabled", false)):
                    failures.append("set_layer_opacity(%d, 0.42) did not enable opacity override" % layer_id)
                if not bool(viewer.set_layer_transform(layer_id, Transform3D(Basis.IDENTITY, Vector3(0.25, 0.0, 0.0)))):
                    failures.append("set_layer_transform(%d, translated identity) failed" % layer_id)
                layer_diagnostics = viewer.get_layer_diagnostics(layer_id)
                if not bool(layer_diagnostics.get("transform_override_enabled", false)):
                    failures.append("set_layer_transform(%d) did not enable transform override" % layer_id)
                if not bool(viewer.clear_layer_transform_override(layer_id)):
                    failures.append("clear_layer_transform_override(%d) failed" % layer_id)
                layer_diagnostics = viewer.get_layer_diagnostics(layer_id)
                if bool(layer_diagnostics.get("transform_override_enabled", false)):
                    failures.append("clear_layer_transform_override(%d) left transform override enabled" % layer_id)

    var spawn_area_ids: PackedInt32Array = PackedInt32Array(viewer.get_spawn_area_ids())
    var active_spawn_area_index: int = int(viewer.get_active_spawn_area_index())
    if active_spawn_area_index < -1:
        failures.append("get_active_spawn_area_index returned %d" % active_spawn_area_index)
    if not spawn_area_ids.is_empty():
        var active_spawn_area: Dictionary = viewer.get_active_spawn_area_info()
        if active_spawn_area.is_empty():
            failures.append("get_active_spawn_area_info returned an empty Dictionary")

    if viewer.is_loaded():
        var original_volume: float = float(viewer.get_volume())
        viewer.set_volume(0.42)
        await process_frame
        if abs(float(viewer.get_volume()) - 0.42) > 0.002:
            failures.append("set_volume(0.42) did not update get_volume(), got %.3f" % float(viewer.get_volume()))
        viewer.set_volume(1.5)
        await process_frame
        if abs(float(viewer.get_volume()) - 1.0) > 0.002:
            failures.append("set_volume should clamp high values to 1.0, got %.3f" % float(viewer.get_volume()))
        viewer.set_volume(original_volume)
        await process_frame

        viewer.pause()
        await process_frame
        if viewer.is_playing():
            failures.append("pause() did not stop playback")
        viewer.play()
        await process_frame
        if not viewer.is_playing():
            failures.append("play() did not start playback")
        viewer.toggle_pause()
        await process_frame
        if viewer.is_playing():
            failures.append("toggle_pause() did not pause playback")
        viewer.toggle_pause()
        await process_frame
        if not viewer.is_playing():
            failures.append("toggle_pause() did not resume playback")
        viewer.restart()
        await process_frame
        if not viewer.is_playing():
            failures.append("restart() did not leave playback running")
        viewer.skip_forward()
        await process_frame
        viewer.skip_back()
        await process_frame

    var viewport_size: Vector2 = root.get_visible_rect().size
    var width: int = max(int(viewport_size.x), 1)
    var height: int = max(int(viewport_size.y), 1)
    var queue_result: int = int(viewer.queue_render_camera_transform(camera.global_transform, width, height, camera.fov, CAMERA_ID))
    if queue_result < 0:
        failures.append("queue_render_camera_transform returned %d" % queue_result)
    if expected_native:
        var direct_render_result: int = int(viewer.smoke_render_camera(CAMERA_ID, width, height))
        if direct_render_result < 0:
            failures.append("smoke_render_camera returned %d" % direct_render_result)
    var render_diagnostics: Dictionary = viewer.get_render_diagnostics()
    if render_diagnostics.is_empty():
        failures.append("get_render_diagnostics returned an empty Dictionary")
    if int(render_diagnostics.get("last_camera_id", -1)) != CAMERA_ID:
        failures.append("get_render_diagnostics reported camera %d" % int(render_diagnostics.get("last_camera_id", -1)))
    if int(render_diagnostics.get("last_viewport_width", -1)) != width or int(render_diagnostics.get("last_viewport_height", -1)) != height:
        failures.append("get_render_diagnostics reported viewport %sx%s instead of %dx%d" % [
            str(render_diagnostics.get("last_viewport_width", "")),
            str(render_diagnostics.get("last_viewport_height", "")),
            width,
            height,
        ])
    if int(render_diagnostics.get("last_projection_size", 0)) != 16:
        failures.append("get_render_diagnostics did not report a 4x4 projection matrix")

    await process_frame
    render_diagnostics = viewer.get_render_diagnostics()
    var adapter_before_count: int = int(render_diagnostics.get("adapter_before_render_count", 0))
    var adapter_after_count: int = int(render_diagnostics.get("adapter_after_render_count", 0))
    if int(render_diagnostics.get("adapter_graphics_initialized_count", 0)) <= 0:
        failures.append("render adapter graphics initialization callback did not run")
    if adapter_before_count <= 0:
        failures.append("render adapter before-render callback did not run")
    if adapter_after_count <= 0:
        failures.append("render adapter after-render callback did not run")
    if adapter_after_count > adapter_before_count:
        failures.append("render adapter after-render count exceeded before-render count")
    if bool(render_diagnostics.get("render_callback_queued", false)):
        failures.append("render callback was still queued after a process frame")
    if int(render_diagnostics.get("adapter_last_camera_id", -1)) != CAMERA_ID:
        failures.append("render adapter reported camera %d" % int(render_diagnostics.get("adapter_last_camera_id", -1)))
    if int(render_diagnostics.get("adapter_last_viewport_width", -1)) != width or int(render_diagnostics.get("adapter_last_viewport_height", -1)) != height:
        failures.append("render adapter reported viewport %sx%s instead of %dx%d" % [
            str(render_diagnostics.get("adapter_last_viewport_width", "")),
            str(render_diagnostics.get("adapter_last_viewport_height", "")),
            width,
            height,
        ])

    var load_unload_cycles := _get_env_int("IMM_GODOT_LOAD_UNLOAD_CYCLES", 0)
    if load_unload_cycles > 0:
        await _exercise_load_unload_cycles(viewer, camera, load_unload_cycles, expected_native, failures)

    scene.queue_free()
    await process_frame
    _finish(failures)

func _exercise_load_unload_cycles(viewer: Node, camera: Camera3D, cycle_count: int, expected_native: bool, failures: Array[String]) -> void:
    var viewport_size: Vector2 = root.get_visible_rect().size
    var width: int = max(int(viewport_size.x), 1)
    var height: int = max(int(viewport_size.y), 1)

    for cycle_index in range(cycle_count):
        if viewer.is_loaded():
            viewer.unload_document()
            await process_frame
        if viewer.is_loaded():
            failures.append("load/unload cycle %d did not unload the document" % [cycle_index + 1])

        var unloaded_queue_result: int = int(viewer.queue_render_camera_transform(camera.global_transform, width, height, camera.fov, CAMERA_ID))
        if unloaded_queue_result < 0:
            failures.append("load/unload cycle %d queue while unloaded returned %d" % [cycle_index + 1, unloaded_queue_result])

        var load_result: int = int(viewer.load_document())
        await process_frame
        await process_frame
        if load_result < 0:
            failures.append("load/unload cycle %d load_document returned %d" % [cycle_index + 1, load_result])
            continue
        if not await _wait_for_document_loaded(viewer):
            failures.append("load/unload cycle %d did not reload the document" % [cycle_index + 1])
            continue

        RenderingServer.set_default_clear_color(viewer.get_background_color())
        var queue_result: int = int(viewer.queue_render_camera_transform(camera.global_transform, width, height, camera.fov, CAMERA_ID))
        if queue_result < 0:
            failures.append("load/unload cycle %d queue after reload returned %d" % [cycle_index + 1, queue_result])
        if expected_native:
            var direct_render_result: int = int(viewer.smoke_render_camera(CAMERA_ID, width, height))
            if direct_render_result < 0:
                failures.append("load/unload cycle %d smoke_render_camera returned %d" % [cycle_index + 1, direct_render_result])
        await process_frame

    var final_diagnostics: Dictionary = viewer.get_render_diagnostics()
    if int(final_diagnostics.get("last_camera_id", -1)) != CAMERA_ID:
        failures.append("load/unload smoke ended with camera %d instead of %d" % [
            int(final_diagnostics.get("last_camera_id", -1)),
            CAMERA_ID,
        ])
    if bool(final_diagnostics.get("render_callback_queued", false)):
        failures.append("load/unload smoke ended with a queued render callback")

func _wait_for_document_loaded(viewer: Node) -> bool:
    var ready_deadline_msec: int = Time.get_ticks_msec() + int(MAX_READY_SECONDS * 1000.0)
    while Time.get_ticks_msec() < ready_deadline_msec:
        if viewer.is_loaded():
            return true
        await create_timer(0.05).timeout
    return false

func _finish(failures: Array[String]) -> void:
    if failures.is_empty():
        print("IMM Godot smoke test passed")
        quit(0)
        return

    for failure in failures:
        push_error(failure)
    quit(1)

func _colors_close(left: Color, right: Color, epsilon := 0.002) -> bool:
    return abs(left.r - right.r) <= epsilon \
        and abs(left.g - right.g) <= epsilon \
        and abs(left.b - right.b) <= epsilon \
        and abs(left.a - right.a) <= epsilon

func _vectors_close(left: Vector3, right: Vector3, epsilon := 0.002) -> bool:
    return abs(left.x - right.x) <= epsilon \
        and abs(left.y - right.y) <= epsilon \
        and abs(left.z - right.z) <= epsilon

func _get_env_int(name: String, default_value: int) -> int:
    var value := OS.get_environment(name)
    if value.is_empty():
        return default_value
    if not value.is_valid_int():
        return default_value
    return max(int(value), 0)
