extends SceneTree

const EXPECTED_RENDERER := "gl_compatibility"
const SAMPLE_SCENE := "res://scenes/SampleScene.tscn"
const NATIVE_SCENE := "res://scenes/NativeSmokeScene.tscn"
const CAMERA_ID := 0

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var failures: Array[String] = []

    var renderer := str(ProjectSettings.get_setting("rendering/renderer/rendering_method", ""))
    if renderer != EXPECTED_RENDERER:
        failures.append("Expected renderer %s, got %s" % [EXPECTED_RENDERER, renderer])

    var expected_native := OS.get_environment("IMM_GODOT_EXPECT_NATIVE") == "1"
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
        "queue_render_camera_transform",
        "queue_render_last_camera",
        "load_document",
        "is_loaded",
        "is_playing",
        "pause",
        "play",
        "toggle_pause",
        "restart",
        "get_background_color",
        "get_document_state",
        "is_sequence_ready",
        "get_chapter_count",
        "get_current_chapter",
        "get_bounding_box",
        "get_layer_count",
        "get_layer_info",
        "get_layer_diagnostics",
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

    var background_color: Color = viewer.get_background_color()
    if background_color.a <= 0.0:
        failures.append("get_background_color returned a fully transparent color")

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
                viewer.get_layer_diagnostics(layer_id)

    var spawn_area_ids: PackedInt32Array = PackedInt32Array(viewer.get_spawn_area_ids())
    var active_spawn_area_index: int = int(viewer.get_active_spawn_area_index())
    if active_spawn_area_index < -1:
        failures.append("get_active_spawn_area_index returned %d" % active_spawn_area_index)
    if not spawn_area_ids.is_empty():
        var active_spawn_area: Dictionary = viewer.get_active_spawn_area_info()
        if active_spawn_area.is_empty():
            failures.append("get_active_spawn_area_info returned an empty Dictionary")

    if viewer.is_loaded():
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

    var viewport_size: Vector2 = root.get_visible_rect().size
    var width: int = max(int(viewport_size.x), 1)
    var height: int = max(int(viewport_size.y), 1)
    var queue_result: int = int(viewer.queue_render_camera_transform(camera.global_transform, width, height, camera.fov, CAMERA_ID))
    if queue_result < 0:
        failures.append("queue_render_camera_transform returned %d" % queue_result)
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

    scene.queue_free()
    await process_frame
    _finish(failures)

func _finish(failures: Array[String]) -> void:
    if failures.is_empty():
        print("IMM Godot smoke test passed")
        quit(0)
        return

    for failure in failures:
        push_error(failure)
    quit(1)
