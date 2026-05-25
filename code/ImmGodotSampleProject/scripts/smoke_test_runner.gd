extends SceneTree

const SCRIPT_SCENE := "res://scenes/SampleScene.tscn"
const NATIVE_SCENE := "res://scenes/NativeSmokeScene.tscn"
const LOADING_STATE_LOADED := 2
const MAX_LOAD_FRAMES := 600

func _init() -> void:
    call_deferred("_run")

func _run() -> void:
    var require_extension := OS.get_environment("IMM_GODOT_REQUIRE_EXTENSION") == "1"
    var scene_path := NATIVE_SCENE if require_extension else SCRIPT_SCENE
    var scene := load(scene_path)
    if scene == null:
        _fail("Failed to load %s" % scene_path)
        return

    var root_node: Node = scene.instantiate()
    root.add_child(root_node)
    await process_frame

    var viewer: Node = root_node.get_node_or_null("ImmViewer")
    if viewer == null:
        _fail("%s is missing ImmViewer" % scene_path)
        return
    if require_extension and viewer.get_script() != null:
        _fail("Native smoke requires the GDExtension ImmViewerNode, but ImmViewer has a script attached")
        return

    var required_methods := [
        "load_document",
        "unload_document",
        "play",
        "pause",
        "restart",
        "next_chapter",
        "previous_chapter",
        "next_spawn_area",
        "previous_spawn_area",
        "global_work",
        "get_spawn_area_ids",
        "get_active_spawn_area_index",
        "get_active_spawn_area_id",
        "set_active_spawn_area_index",
        "get_spawn_area_info",
        "set_document_transform",
        "set_camera_transform",
        "set_camera_matrices",
        "render_camera",
        "get_render_diagnostics",
        "get_document_state",
        "get_bounding_box",
        "get_background_color",
        "set_matrix_debug_logging",
        "get_matrix_debug_logging",
        "set_document_path",
        "get_document_path",
    ]
    for method_name in required_methods:
        if not _require_method(viewer, method_name):
            return

    var document_path_override := OS.get_environment("IMM_GODOT_SMOKE_DOCUMENT")
    if not document_path_override.is_empty():
        viewer.set_document_path(document_path_override)
    var document_path := str(viewer.get_document_path())
    var resolved_document_path := _resolve_project_path(document_path)
    if not FileAccess.file_exists(resolved_document_path):
        _fail("ImmViewer document path does not resolve to a file: %s -> %s" % [document_path, resolved_document_path])
        return

    var document_transform := Transform3D.IDENTITY
    document_transform.origin = Vector3(0.5, 0.0, -0.25)
    viewer.set_document_transform(document_transform)

    var camera := root_node.get_node_or_null("CameraRig/Camera3D")
    if camera != null:
        viewer.set_camera_transform(camera.global_transform)
    var explicit_world_to_head := PackedFloat32Array([
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.25, 1.6, 6.0, 1.0,
    ])
    var explicit_projection := PackedFloat32Array([
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, -1.0, -1.0,
        0.0, 0.0, -0.1, 0.0,
    ])
    if not viewer.set_camera_matrices(1, explicit_world_to_head, explicit_projection):
        _fail("ImmViewer rejected explicit camera matrices")
        return
    viewer.set_matrix_debug_logging(true)
    if not viewer.get_matrix_debug_logging():
        _fail("ImmViewer did not enable matrix debug logging")
        return

    viewer.load_document()
    if not viewer.is_loaded():
        _fail("ImmViewer did not report a loaded document")
        return
    var loaded_state := await _wait_for_loaded_document(viewer)
    if loaded_state.is_empty():
        var timed_out_state: Dictionary = viewer.get_document_state()
        _fail("ImmViewer document did not reach loaded state after %d frames; current state=%s" % [MAX_LOAD_FRAMES, str(timed_out_state)])
        return

    viewer.pause()
    if viewer.is_playing():
        _fail("ImmViewer did not pause")
        return
    var paused_state: Dictionary = viewer.get_document_state()
    if int(paused_state.get("playback_state", -1)) < 0:
        _fail("ImmViewer document state did not report a valid paused playback state")
        return

    viewer.play()
    if not viewer.is_playing():
        _fail("ImmViewer did not play")
        return
    var playing_state: Dictionary = viewer.get_document_state()
    if int(playing_state.get("playback_state", -1)) < 0:
        _fail("ImmViewer document state did not report a valid playing playback state")
        return

    var spawn_area_ids = viewer.get_spawn_area_ids()
    if viewer.get_active_spawn_area_index() < -1:
        _fail("ImmViewer reported an invalid active spawn area index")
        return
    if viewer.get_active_spawn_area_id() != -1 and not spawn_area_ids.has(viewer.get_active_spawn_area_id()):
        _fail("ImmViewer active spawn area id is not present in the spawn area id list")
        return
    if spawn_area_ids.is_empty():
        if viewer.set_active_spawn_area_index(0):
            _fail("ImmViewer accepted a spawn area index when no spawn areas were available")
            return
    else:
        var first_spawn_info: Dictionary = viewer.get_spawn_area_info(spawn_area_ids[0])
        if first_spawn_info.get("valid", false) != true:
            _fail("ImmViewer could not query the first spawn area")
            return
        if first_spawn_info.get("id", -1) != spawn_area_ids[0]:
            _fail("ImmViewer spawn area info did not preserve the requested id")
            return
        if not viewer.set_active_spawn_area_index(0):
            _fail("ImmViewer rejected a valid spawn area index")
            return
        if viewer.get_active_spawn_area_index() != 0:
            _fail("ImmViewer did not update the active spawn area index")
            return

    var background_color: Color = viewer.get_background_color()
    if background_color.a <= 0.0:
        _fail("ImmViewer background color did not have a valid alpha")
        return
    var bounding_box: Dictionary = viewer.get_bounding_box()
    if bounding_box.get("valid", false):
        var bounds_size: Vector3 = bounding_box.get("size", Vector3.ZERO)
        if bounds_size.x < 0.0 or bounds_size.y < 0.0 or bounds_size.z < 0.0:
            _fail("ImmViewer bounding box reported a negative size")
            return
    else:
        _fail("ImmViewer bounding box was not valid after the document reached loaded state")
        return

    var render_result: int = viewer.render_camera(1, Vector2i(1600, 900), 0)
    if render_result != 0:
        _fail("ImmViewer render_camera returned %d" % render_result)
        return
    await process_frame
    var capture_path := OS.get_environment("IMM_GODOT_CAPTURE_PATH")
    if not capture_path.is_empty():
        var image := root.get_viewport().get_texture().get_image()
        var save_result := image.save_png(capture_path)
        if save_result != OK:
            _fail("ImmViewer capture failed with error %d at %s" % [save_result, capture_path])
            return

    var diagnostics: Dictionary = viewer.get_render_diagnostics()
    diagnostics["document_path"] = resolved_document_path
    diagnostics["document_name"] = resolved_document_path.get_file()
    diagnostics["document_size_bytes"] = _get_file_size(resolved_document_path)
    diagnostics["rendering_method"] = ProjectSettings.get_setting("rendering/renderer/rendering_method", "")
    diagnostics["godot_version"] = str(Engine.get_version_info().get("string", ""))
    if diagnostics.get("backend_initialized", false) != true:
        _fail("Render diagnostics did not report backend initialization")
        return
    if diagnostics.get("godot_version", "").is_empty():
        _fail("Godot version diagnostics were not available")
        return
    if diagnostics.get("rendering_method", "") != "gl_compatibility":
        _fail("Godot project is not using the Compatibility renderer path")
        return
    if diagnostics.get("before_render_count", 0) < 1 or diagnostics.get("after_render_count", 0) < 1:
        _fail("Render diagnostics did not count before/after callbacks")
        return
    if diagnostics.get("last_render_camera_id", -1) != 1:
        _fail("Render diagnostics did not preserve camera id")
        return
    if diagnostics.get("last_viewport_width", 0.0) != 1600.0 or diagnostics.get("last_viewport_height", 0.0) != 900.0:
        _fail("Render diagnostics did not preserve viewport size")
        return
    if diagnostics.get("matrix_debug_logging", false) != true:
        _fail("Render diagnostics did not preserve matrix debug logging state")
        return
    if diagnostics.get("has_document_transform", false) != true:
        _fail("Render diagnostics did not report submitted document transform")
        return
    if diagnostics.get("last_document_to_world", PackedFloat32Array()).size() != 16:
        _fail("Render diagnostics did not expose document-to-world matrix")
        return
    if not is_equal_approx(diagnostics.get("last_document_to_world", PackedFloat32Array())[12], 0.5):
        _fail("Render diagnostics did not preserve explicit document transform values")
        return
    if diagnostics.get("has_last_matrices", false) != true:
        _fail("Render diagnostics did not report submitted camera matrices")
        return
    if diagnostics.get("last_matrix_camera_id", -1) != 1:
        _fail("Render diagnostics did not preserve matrix camera id")
        return
    if diagnostics.get("last_world_to_head", PackedFloat32Array()).size() != 16:
        _fail("Render diagnostics did not expose world-to-head matrix")
        return
    if diagnostics.get("last_projection", PackedFloat32Array()).size() != 16:
        _fail("Render diagnostics did not expose projection matrix")
        return
    if not is_equal_approx(diagnostics.get("last_world_to_head", PackedFloat32Array())[12], 0.25):
        _fail("Render diagnostics did not preserve explicit world-to-head matrix values")
        return

    _print_matrix_diagnostics(diagnostics)

    viewer.unload_document()
    if viewer.is_loaded():
        _fail("ImmViewer did not unload")
        return

    var lifecycle_cycles := 2
    for cycle_index in range(lifecycle_cycles):
        viewer.load_document()
        if not viewer.is_loaded():
            _fail("ImmViewer did not reload document during lifecycle cycle %d" % cycle_index)
            return
        if (await _wait_for_loaded_document(viewer)).is_empty():
            _fail("ImmViewer did not reach loaded state during lifecycle cycle %d" % cycle_index)
            return
        var cycle_render_result: int = viewer.render_camera(1, Vector2i(1600, 900), 0)
        if cycle_render_result != 0:
            _fail("ImmViewer render_camera returned %d during lifecycle cycle %d" % [cycle_render_result, cycle_index])
            return
        viewer.unload_document()
        if viewer.is_loaded():
            _fail("ImmViewer did not unload during lifecycle cycle %d" % cycle_index)
            return

    print("IMM Godot smoke lifecycle cycles: %d" % lifecycle_cycles)
    print("IMM Godot smoke test passed")
    quit(0)

func _require_method(object: Object, method_name: StringName) -> bool:
    if not object.has_method(method_name):
        _fail("ImmViewer is missing method %s" % method_name)
        return false
    return true

func _fail(message: String) -> void:
    push_error(message)
    quit(1)

func _wait_for_loaded_document(viewer: Node) -> Dictionary:
    for _frame_index in range(MAX_LOAD_FRAMES):
        viewer.global_work(true)
        viewer.render_camera(1, Vector2i(1600, 900), 0)
        var state: Dictionary = viewer.get_document_state()
        if int(state.get("loading_state", -1)) == LOADING_STATE_LOADED:
            return state
        await process_frame
    return {}

func _resolve_project_path(path: String) -> String:
    if path.begins_with("res://") or path.begins_with("user://"):
        return ProjectSettings.globalize_path(path)
    if path.is_absolute_path():
        return path
    return ProjectSettings.globalize_path("res://" + path)

func _print_matrix_diagnostics(diagnostics: Dictionary) -> void:
    var payload := {
        "schema": "imm_godot_matrix_diagnostics_v1",
        "backend_initialized": diagnostics.get("backend_initialized", false),
        "matrix_debug_logging": diagnostics.get("matrix_debug_logging", false),
        "camera_id": diagnostics.get("last_matrix_camera_id", -1),
        "last_matrix_camera_id": diagnostics.get("last_matrix_camera_id", -1),
        "last_render_camera_id": diagnostics.get("last_render_camera_id", -1),
        "last_render_eye_id": diagnostics.get("last_render_eye_id", -1),
        "last_viewport_width": diagnostics.get("last_viewport_width", 0.0),
        "last_viewport_height": diagnostics.get("last_viewport_height", 0.0),
        "document_path": diagnostics.get("document_path", ""),
        "document_name": diagnostics.get("document_name", ""),
        "document_size_bytes": diagnostics.get("document_size_bytes", -1),
        "godot_version": diagnostics.get("godot_version", ""),
        "rendering_method": diagnostics.get("rendering_method", ""),
        "document_loading_state": diagnostics.get("document_loading_state", -1),
        "document_playback_state": diagnostics.get("document_playback_state", -1),
        "bounding_box_valid": diagnostics.get("bounding_box_valid", false),
        "bounding_box_min": _vector3_to_array(diagnostics.get("bounding_box_min", Vector3.ZERO)),
        "bounding_box_max": _vector3_to_array(diagnostics.get("bounding_box_max", Vector3.ZERO)),
        "background_color": _color_to_array(diagnostics.get("background_color", Color.BLACK)),
        "spawn_area_count": diagnostics.get("spawn_area_count", 0),
        "active_spawn_area_index": diagnostics.get("active_spawn_area_index", -1),
        "active_spawn_area_id": diagnostics.get("active_spawn_area_id", -1),
        "document_to_world": _packed_float32_to_array(diagnostics.get("last_document_to_world", PackedFloat32Array())),
        "world_to_head": _packed_float32_to_array(diagnostics.get("last_world_to_head", PackedFloat32Array())),
        "projection": _packed_float32_to_array(diagnostics.get("last_projection", PackedFloat32Array())),
    }
    print("IMM_GODOT_MATRIX_DIAGNOSTICS_JSON " + JSON.stringify(payload))

func _packed_float32_to_array(values: PackedFloat32Array) -> Array:
    var result: Array = []
    for value in values:
        result.append(value)
    return result

func _color_to_array(value: Color) -> Array:
    return [value.r, value.g, value.b, value.a]

func _vector3_to_array(value: Vector3) -> Array:
    return [value.x, value.y, value.z]

func _get_file_size(path: String) -> int:
    var file := FileAccess.open(path, FileAccess.READ)
    if file == null:
        return -1
    return file.get_length()
