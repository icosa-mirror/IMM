extends SceneTree

const EXPECTED_RENDERER := "forward_plus"
const EXTENSION_PATH := "res://addons/imm_viewer/imm_viewer.gdextension"
const SAMPLE_SCENE := "res://scenes/SampleScene.tscn"
const CAMERA_ID := 0
const MAX_READY_SECONDS := 12.0
const RENDERER_API_AUTO := 0

var _signal_document_loaded_count := 0
var _signal_document_unloaded_count := 0
var _signal_playback_changed_count := 0
var _signal_spawn_area_changed_count := 0
var _signal_last_document_loaded_path := ""
var _signal_last_playback_state := false
var _signal_last_spawn_area_index := -1

func _initialize() -> void:
    call_deferred("_run")

func _run() -> void:
    var failures: Array[String] = []

    var renderer := str(ProjectSettings.get_setting("rendering/renderer/rendering_method", ""))
    if renderer != EXPECTED_RENDERER:
        failures.append("Expected renderer %s, got %s" % [EXPECTED_RENDERER, renderer])

    var expected_native := true
    if not ClassDB.class_exists("ImmViewerNode"):
        var extension_status := GDExtensionManager.load_extension(EXTENSION_PATH)
        if extension_status != OK and extension_status != ERR_ALREADY_EXISTS:
            failures.append("Failed to load %s: %d" % [EXTENSION_PATH, int(extension_status)])

    var scene_path := OS.get_environment("IMM_GODOT_SMOKE_SCENE")
    if scene_path.is_empty():
        scene_path = SAMPLE_SCENE

    var packed_scene := load(scene_path)
    if packed_scene == null:
        failures.append("Failed to load %s" % scene_path)
        _finish(failures)
        return

    var scene: Node = packed_scene.instantiate()
    var requested_renderer_api := _get_env_int("IMM_GODOT_RENDERER_API", RENDERER_API_AUTO)
    var pre_ready_viewer := scene.get_node_or_null("ImmViewer")
    if pre_ready_viewer != null:
        pre_ready_viewer.set("renderer_api", requested_renderer_api)
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
        "set_time",
        "get_time",
        "get_play_time",
        "get_play_time_seconds",
        "seek_relative_seconds",
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
        "get_spawn_area_info",
        "get_active_spawn_area_info",
        "next_spawn_area",
        "previous_spawn_area",
    ]
    for method_name in required_methods:
        if not viewer.has_method(method_name):
            failures.append("ImmViewer is missing method %s" % method_name)

    if not failures.is_empty():
        _finish(failures)
        return

    _connect_viewer_signals(viewer, failures)
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
    print("IMM Godot backend diagnostics: %s" % str(backend_diagnostics))
    if backend_diagnostics.is_empty():
        failures.append("get_render_backend_diagnostics returned an empty Dictionary")
    if int(backend_diagnostics.get("renderer_api", -1)) != requested_renderer_api:
        failures.append("render backend diagnostics reported renderer_api %d instead of requested %d" % [
            int(backend_diagnostics.get("renderer_api", -1)),
            requested_renderer_api,
        ])
    if str(backend_diagnostics.get("project_rendering_method", "")) != EXPECTED_RENDERER:
        failures.append("render backend diagnostics reported renderer %s" % str(backend_diagnostics.get("project_rendering_method", "")))
    var requested_vulkan := bool(backend_diagnostics.get("wants_vulkan_renderer", false)) and bool(backend_diagnostics.get("driver_is_vulkan", false))
    if requested_vulkan and expected_native:
        if not bool(backend_diagnostics.get("wants_vulkan_renderer", false)):
            failures.append("native Vulkan smoke did not report wants_vulkan_renderer")
        if not bool(backend_diagnostics.get("driver_is_vulkan", false)):
            failures.append("native Vulkan smoke did not report a Vulkan RenderingDevice driver")
        if not bool(backend_diagnostics.get("vulkan_adapter_candidate", false)):
            failures.append("native Vulkan smoke did not report a Vulkan adapter candidate")
    if int(backend_diagnostics.get("renderer_api", -1)) < 0:
        failures.append("render backend diagnostics did not report renderer_api")
    if bool(backend_diagnostics.get("native_backend_initialized", false)) != expected_native:
        failures.append("render backend diagnostics native_backend_initialized=%s did not match expected_native=%s" % [
            str(backend_diagnostics.get("native_backend_initialized", false)),
            str(expected_native),
        ])

    if requested_vulkan and expected_native:
        var warmup_size: Vector2 = root.get_visible_rect().size
        var warmup_width: int = max(int(warmup_size.x), 1)
        var warmup_height: int = max(int(warmup_size.y), 1)
        var warmup_queue_result: int = int(viewer.queue_render_camera_transform(camera.global_transform, warmup_width, warmup_height, camera.fov, CAMERA_ID))
        if warmup_queue_result < 0:
            failures.append("native Vulkan warmup queue_render_camera_transform returned %d" % warmup_queue_result)
        var compositor_diagnostics: Dictionary = await _wait_for_compositor_render(camera, -1, -1, -1)
        if compositor_diagnostics.is_empty():
            failures.append("native Vulkan smoke scene is missing ImmViewerCompositorEffect diagnostics")
        else:
            print("IMM Godot compositor diagnostics: %s" % str(compositor_diagnostics))
            if int(compositor_diagnostics.get("callback_count", 0)) <= 0:
                failures.append("ImmViewerCompositorEffect render callback did not run")
            if int(compositor_diagnostics.get("last_vulkan_instance_handle", 0)) == 0:
                failures.append("ImmViewerCompositorEffect did not receive a Vulkan instance handle")
            if int(compositor_diagnostics.get("last_vulkan_device_handle", 0)) == 0:
                failures.append("ImmViewerCompositorEffect did not receive a Vulkan device handle")
            if int(compositor_diagnostics.get("last_vulkan_queue_handle", 0)) == 0:
                failures.append("ImmViewerCompositorEffect did not receive a Vulkan queue handle")
            if not bool(compositor_diagnostics.get("ever_vulkan_frame_started", false)):
                failures.append("ImmViewerCompositorEffect did not start a Vulkan frame")

    var initiated_load := false
    if not viewer.is_loaded():
        initiated_load = true
        var load_result: int = int(viewer.load_document())
        await process_frame
        await process_frame
        if load_result < 0:
            failures.append("load_document returned %d" % load_result)
    if not viewer.is_loaded():
        failures.append("ImmViewer did not load %s" % str(viewer.get("document_path")))
    if initiated_load and _signal_document_loaded_count <= 0:
        failures.append("document_loaded signal was not emitted by load_document")
    elif _signal_document_loaded_count > 0 and _signal_last_document_loaded_path != str(viewer.get("document_path")):
        failures.append("document_loaded path %s did not match document_path %s" % [
            _signal_last_document_loaded_path,
            str(viewer.get("document_path")),
        ])
    if initiated_load and bool(viewer.get("auto_play")) and _signal_playback_changed_count <= 0:
        failures.append("playback_changed signal was not emitted by auto-play load")

    var document_state: Dictionary = viewer.get_document_state()
    if document_state.is_empty():
        failures.append("get_document_state returned an empty Dictionary")
    if not document_state.has("loading_state") or not document_state.has("playback_state"):
        failures.append("get_document_state is missing loading/playback state keys")
    var timeline_ready := await _wait_for_timeline_ready(viewer, expected_native)

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
        var layer_id := _find_override_test_layer(viewer, layer_count)
        if layer_id < 0:
            print("IMM Godot smoke skipped layer override checks; no override-capable layer was found")
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
            if abs(float(layer_diagnostics.get("opacity", -1.0)) - 0.42) > 0.002:
                failures.append("set_layer_opacity(%d, 0.42) reported opacity %.3f" % [
                    layer_id,
                    float(layer_diagnostics.get("opacity", -1.0)),
                ])
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
        if active_spawn_area_index < 0 or active_spawn_area_index >= spawn_area_ids.size():
            failures.append("get_active_spawn_area_index %d was outside %d authored spawn areas" % [
                active_spawn_area_index,
                spawn_area_ids.size(),
            ])
        for spawn_area_id in spawn_area_ids:
            var spawn_area_info: Dictionary = viewer.get_spawn_area_info(spawn_area_id)
            if spawn_area_info.is_empty():
                failures.append("get_spawn_area_info(%d) returned an empty Dictionary" % spawn_area_id)
            else:
                _validate_spawn_area_info(spawn_area_info, "get_spawn_area_info(%d)" % spawn_area_id, failures)
        var active_spawn_area: Dictionary = viewer.get_active_spawn_area_info()
        if active_spawn_area.is_empty():
            failures.append("get_active_spawn_area_info returned an empty Dictionary")
        else:
            _validate_spawn_area_info(active_spawn_area, "get_active_spawn_area_info", failures)
            var expected_active_id: int = int(spawn_area_ids[active_spawn_area_index])
            if int(active_spawn_area.get("id", -1)) != expected_active_id:
                failures.append("get_active_spawn_area_info id %d did not match active spawn id %d" % [
                    int(active_spawn_area.get("id", -1)),
                    expected_active_id,
                ])

        var original_spawn_area_index := active_spawn_area_index
        viewer.next_spawn_area()
        await process_frame
        var next_spawn_area_index: int = int(viewer.get_active_spawn_area_index())
        var expected_next_spawn_area_index: int = posmod(original_spawn_area_index + 1, spawn_area_ids.size())
        if next_spawn_area_index != expected_next_spawn_area_index:
            failures.append("next_spawn_area moved active index to %d instead of %d" % [
                next_spawn_area_index,
                expected_next_spawn_area_index,
            ])
        viewer.previous_spawn_area()
        await process_frame
        var restored_spawn_area_index: int = int(viewer.get_active_spawn_area_index())
        if restored_spawn_area_index != original_spawn_area_index:
            failures.append("previous_spawn_area restored active index to %d instead of %d" % [
                restored_spawn_area_index,
                original_spawn_area_index,
            ])
        if _signal_spawn_area_changed_count < 2:
            failures.append("spawn_area_changed signal was not emitted by next/previous spawn-area navigation")
        if _signal_last_spawn_area_index != original_spawn_area_index:
            failures.append("spawn_area_changed final index %d did not match restored active index %d" % [
                _signal_last_spawn_area_index,
                original_spawn_area_index,
            ])

    if viewer.is_loaded():
        viewer.set_time(-10, -20)
        await process_frame
        var clamped_time: Dictionary = viewer.get_time()
        for key in ["time_since_start", "time_since_stop", "play_time", "play_time_seconds"]:
            if not clamped_time.has(key):
                failures.append("get_time result is missing %s" % key)
        if int(clamped_time.get("time_since_start", -1)) < 0 or int(clamped_time.get("time_since_stop", -1)) < 0:
            failures.append("set_time should clamp negative values, got %s" % str(clamped_time))

        if timeline_ready:
            viewer.seek_relative_seconds(0.5)
            await process_frame
            var seek_time: Dictionary = viewer.get_time()
            var seek_play_time_seconds: float = float(viewer.get_play_time_seconds())
            if int(viewer.get_play_time()) <= 0:
                failures.append("seek_relative_seconds(0.5) did not advance get_play_time()")
            if seek_play_time_seconds < 0.45 or seek_play_time_seconds > 0.75:
                failures.append("seek_relative_seconds(0.5) produced play time %.3fs" % seek_play_time_seconds)
            if abs(float(seek_time.get("play_time_seconds", -1.0)) - seek_play_time_seconds) > 0.002:
                failures.append("get_time play_time_seconds %.3f did not match get_play_time_seconds %.3f" % [
                    float(seek_time.get("play_time_seconds", -1.0)),
                    seek_play_time_seconds,
                ])

            viewer.seek_relative_seconds(-999.0)
            await process_frame
            if int(viewer.get_play_time()) < 0 or float(viewer.get_play_time_seconds()) < -0.002:
                failures.append("seek_relative_seconds should clamp below zero, got %d / %.3fs" % [
                    int(viewer.get_play_time()),
                    float(viewer.get_play_time_seconds()),
                ])
        else:
            viewer.seek_relative_seconds(0.5)
            await process_frame
            if int(viewer.get_play_time()) != 0 or abs(float(viewer.get_play_time_seconds())) > 0.002:
                failures.append("timeline APIs should remain at zero before IMM Loaded state, got %d / %.3fs" % [
                    int(viewer.get_play_time()),
                    float(viewer.get_play_time_seconds()),
                ])

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
        if _signal_last_playback_state:
            failures.append("playback_changed did not report paused state after pause()")
        viewer.play()
        await process_frame
        if not viewer.is_playing():
            failures.append("play() did not start playback")
        if not _signal_last_playback_state:
            failures.append("playback_changed did not report playing state after play()")
        viewer.toggle_pause()
        await process_frame
        if viewer.is_playing():
            failures.append("toggle_pause() did not pause playback")
        if _signal_last_playback_state:
            failures.append("playback_changed did not report paused state after toggle_pause()")
        viewer.toggle_pause()
        await process_frame
        if not viewer.is_playing():
            failures.append("toggle_pause() did not resume playback")
        if not _signal_last_playback_state:
            failures.append("playback_changed did not report playing state after second toggle_pause()")
        viewer.restart()
        await process_frame
        if not viewer.is_playing():
            failures.append("restart() did not leave playback running")
        if not _signal_last_playback_state:
            failures.append("playback_changed did not report playing state after restart()")
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
    if expected_native and not requested_vulkan:
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

    var post_load_compositor_diagnostics: Dictionary = {}
    if requested_vulkan and expected_native:
        post_load_compositor_diagnostics = await _wait_for_compositor_render(camera, CAMERA_ID, width, height)
        print("IMM Godot post-load Vulkan compositor diagnostics: %s" % str(post_load_compositor_diagnostics))
        if post_load_compositor_diagnostics.is_empty():
            failures.append("ImmViewerCompositorEffect diagnostics were unavailable after sample render")
        else:
            if not bool(post_load_compositor_diagnostics.get("last_vulkan_frame_started", false)):
                failures.append("post-load Vulkan render did not start a Vulkan frame")
            if not bool(post_load_compositor_diagnostics.get("last_had_intermediate_texture", false)):
                failures.append("post-load Vulkan render did not allocate an intermediate texture")
            if not bool(post_load_compositor_diagnostics.get("last_composite_result", false)):
                failures.append("post-load Vulkan render did not composite into the Godot color texture")
            if int(post_load_compositor_diagnostics.get("last_render_result", -1)) < 0:
                failures.append("post-load ImmGodot_RenderCamera returned %d" % int(post_load_compositor_diagnostics.get("last_render_result", -1)))
            if int(post_load_compositor_diagnostics.get("last_render_camera_id", -1)) != CAMERA_ID:
                failures.append("post-load Vulkan compositor rendered camera %d" % int(post_load_compositor_diagnostics.get("last_render_camera_id", -1)))
            if int(post_load_compositor_diagnostics.get("last_render_width", -1)) != width or int(post_load_compositor_diagnostics.get("last_render_height", -1)) != height:
                failures.append("post-load Vulkan compositor rendered viewport %sx%s instead of %dx%d" % [
                    str(post_load_compositor_diagnostics.get("last_render_width", "")),
                    str(post_load_compositor_diagnostics.get("last_render_height", "")),
                    width,
                    height,
                ])
    else:
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

    if viewer.is_loaded():
        var document_unloaded_count_before := _signal_document_unloaded_count
        viewer.unload_document()
        await process_frame
        if viewer.is_loaded():
            failures.append("unload_document() did not clear loaded state")
        if _signal_document_unloaded_count <= document_unloaded_count_before:
            failures.append("document_unloaded signal was not emitted by unload_document")
        if _signal_last_playback_state:
            failures.append("playback_changed did not report paused state after unload_document")

    scene.queue_free()
    await process_frame
    _finish(failures)

func _connect_viewer_signals(viewer: Node, failures: Array[String]) -> void:
    for signal_name in ["document_loaded", "document_unloaded", "playback_changed", "spawn_area_changed", "native_backend_initialized", "native_backend_failed"]:
        if not viewer.has_signal(signal_name):
            failures.append("ImmViewer is missing signal %s" % signal_name)

    if not viewer.has_signal("document_loaded") or not viewer.has_signal("document_unloaded") \
        or not viewer.has_signal("playback_changed") or not viewer.has_signal("spawn_area_changed"):
        return

    var connection_results := {
        "document_loaded": viewer.connect("document_loaded", Callable(self, "_on_document_loaded")),
        "document_unloaded": viewer.connect("document_unloaded", Callable(self, "_on_document_unloaded")),
        "playback_changed": viewer.connect("playback_changed", Callable(self, "_on_playback_changed")),
        "spawn_area_changed": viewer.connect("spawn_area_changed", Callable(self, "_on_spawn_area_changed")),
    }
    for signal_name in connection_results.keys():
        var result: int = int(connection_results[signal_name])
        if result != OK and result != ERR_INVALID_PARAMETER:
            failures.append("Failed to connect %s signal: %d" % [signal_name, result])

func _on_document_loaded(path: String) -> void:
    _signal_document_loaded_count += 1
    _signal_last_document_loaded_path = path

func _on_document_unloaded() -> void:
    _signal_document_unloaded_count += 1

func _on_playback_changed(is_playing: bool) -> void:
    _signal_playback_changed_count += 1
    _signal_last_playback_state = is_playing

func _on_spawn_area_changed(active_index: int) -> void:
    _signal_spawn_area_changed_count += 1
    _signal_last_spawn_area_index = active_index

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

func _wait_for_timeline_ready(viewer: Node, expected_native: bool) -> bool:
    var ready_deadline_msec: int = Time.get_ticks_msec() + int(MAX_READY_SECONDS * 1000.0)
    while Time.get_ticks_msec() < ready_deadline_msec:
        var document_state: Dictionary = viewer.get_document_state()
        var loaded_state := 3 if expected_native else 2
        if viewer.is_sequence_ready() and int(document_state.get("loading_state", -1)) == loaded_state:
            return true
        await create_timer(0.05).timeout
    return false

func _wait_for_compositor_render(camera: Camera3D, expected_camera_id: int, expected_width: int, expected_height: int) -> Dictionary:
    if camera.compositor == null or camera.compositor.compositor_effects.is_empty():
        return {}
    var compositor_effect: Object = camera.compositor.compositor_effects[0]
    if compositor_effect == null or not compositor_effect.has_method("get_diagnostics"):
        return {}

    var ready_deadline_msec: int = Time.get_ticks_msec() + int(MAX_READY_SECONDS * 1000.0)
    var latest: Dictionary = {}
    while Time.get_ticks_msec() < ready_deadline_msec:
        latest = compositor_effect.get_diagnostics()
        if expected_camera_id < 0:
            if int(latest.get("callback_count", 0)) > 0 and bool(latest.get("ever_vulkan_frame_started", false)):
                return latest
        elif int(latest.get("last_render_camera_id", -1)) == expected_camera_id \
            and int(latest.get("last_render_width", -1)) == expected_width \
            and int(latest.get("last_render_height", -1)) == expected_height \
            and bool(latest.get("last_vulkan_frame_started", false)):
            return latest
        await process_frame
    return latest

func _find_override_test_layer(viewer: Node, layer_count: int) -> int:
    for index in range(layer_count):
        var layer_info: Dictionary = viewer.get_layer_info(index)
        if layer_info.is_empty():
            continue
        var layer_id: int = int(layer_info.get("id", -1))
        if layer_id < 0:
            continue
        if bool(layer_info.get("is_timeline", false)):
            continue
        var layer_diagnostics: Dictionary = viewer.get_layer_diagnostics(layer_id)
        if layer_diagnostics.is_empty():
            continue
        return layer_id
    return -1

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

func _validate_spawn_area_info(info: Dictionary, source: String, failures: Array[String]) -> void:
    for key in ["id", "name", "type", "animated", "locomotion", "transform", "volume"]:
        if not info.has(key):
            failures.append("%s result is missing %s" % [source, key])

    var transform: Dictionary = info.get("transform", {})
    if transform.is_empty():
        failures.append("%s transform was empty" % source)
    for key in ["position", "basis_x", "basis_y", "basis_z", "raw_position", "raw_rotation", "raw_rotation_w", "scale"]:
        if not transform.has(key):
            failures.append("%s transform is missing %s" % [source, key])

    for key in ["position", "basis_x", "basis_y", "basis_z", "raw_position", "raw_rotation"]:
        var value: Variant = transform.get(key, Vector3.ZERO)
        if not value is Vector3:
            failures.append("%s transform %s was not a Vector3" % [source, key])
            continue
        if not _vector_is_finite(value):
            failures.append("%s transform %s was not finite: %s" % [source, key, str(value)])

    for key in ["basis_x", "basis_y", "basis_z"]:
        var axis: Vector3 = transform.get(key, Vector3.ZERO)
        if axis.length() < 0.01:
            failures.append("%s transform %s was too small: %s" % [source, key, str(axis)])

    var basis_x: Vector3 = transform.get("basis_x", Vector3.ZERO)
    var basis_y: Vector3 = transform.get("basis_y", Vector3.ZERO)
    var basis_z: Vector3 = transform.get("basis_z", Vector3.ZERO)
    if basis_x.length() >= 0.01 and basis_y.length() >= 0.01 and basis_z.length() >= 0.01:
        var normalized_x := basis_x.normalized()
        var normalized_y := basis_y.normalized()
        var normalized_z := basis_z.normalized()
        if abs(normalized_x.dot(normalized_y)) > 0.05:
            failures.append("%s basis_x/basis_y were not orthogonal: %.3f" % [source, normalized_x.dot(normalized_y)])
        if abs(normalized_x.dot(normalized_z)) > 0.05:
            failures.append("%s basis_x/basis_z were not orthogonal: %.3f" % [source, normalized_x.dot(normalized_z)])
        if abs(normalized_y.dot(normalized_z)) > 0.05:
            failures.append("%s basis_y/basis_z were not orthogonal: %.3f" % [source, normalized_y.dot(normalized_z)])
        if normalized_x.cross(normalized_y).dot(normalized_z) < 0.5:
            failures.append("%s converted basis did not preserve right-handed orientation" % source)

    for key in ["raw_rotation_w", "scale"]:
        var scalar: float = float(transform.get(key, 0.0))
        if scalar != scalar or abs(scalar) > 1.0e20:
            failures.append("%s transform %s was not finite: %.3f" % [source, key, scalar])
    if float(transform.get("scale", 0.0)) <= 0.0:
        failures.append("%s transform scale was not positive: %.3f" % [source, float(transform.get("scale", 0.0))])

    var volume: Dictionary = info.get("volume", {})
    if volume.is_empty():
        failures.append("%s volume was empty" % source)
    for key in ["type", "offset", "sphere_radius", "box_extent", "allow_translation"]:
        if not volume.has(key):
            failures.append("%s volume is missing %s" % [source, key])
    for key in ["offset", "box_extent"]:
        var value: Variant = volume.get(key, Vector3.ZERO)
        if not value is Vector3:
            failures.append("%s volume %s was not a Vector3" % [source, key])
        elif not _vector_is_finite(value):
            failures.append("%s volume %s was not finite: %s" % [source, key, str(value)])

func _vector_is_finite(value: Vector3) -> bool:
    return value.x == value.x \
        and value.y == value.y \
        and value.z == value.z \
        and abs(value.x) < 1.0e20 \
        and abs(value.y) < 1.0e20 \
        and abs(value.z) < 1.0e20

func _get_env_int(name: String, default_value: int) -> int:
    var value := OS.get_environment(name)
    if value.is_empty():
        return default_value
    if not value.is_valid_int():
        return default_value
    return max(int(value), 0)
