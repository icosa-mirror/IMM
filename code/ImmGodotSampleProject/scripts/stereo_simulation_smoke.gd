extends Node3D

const CAMERA_ID := 0
const MAX_READY_SECONDS := 20.0
const SETTLE_FRAMES := 12
const IMAGE_SAMPLE_STEP := 4
const MAX_HORIZONTAL_ALIGNMENT := 32
const HORIZONTAL_ALIGNMENT_STEP := 4
const MIN_CONTENT_SAMPLES := 128
const MIN_STEREO_MEAN_DIFFERENCE := 0.00005
const ORIENTATION_MARGIN := 0.92
const RESULT_MARKER := "[IMM_GODOT_STEREO_SIM_20260825]"

@onready var viewer: Node = $ImmViewer
@onready var camera: Camera3D = $Camera3D

var compositor_effect: Object
var output_directory := ""
var result_path := ""
var replay_path := ""
var replay_enabled := false

func _ready() -> void:
	call_deferred("_run")

func _run() -> void:
	output_directory = _resolve_output_directory()
	result_path = output_directory.path_join("result.json")
	if DirAccess.make_dir_recursive_absolute(output_directory) != OK:
		_finish_failure("could not create output directory: %s" % output_directory)
		return

	compositor_effect = camera.compositor.compositor_effects[0]
	if compositor_effect == null:
		_finish_failure("camera compositor effect is unavailable")
		return
	if not _configure_replay_if_requested():
		return

	viewer.log_file_path = output_directory.path_join("native.log")
	if not viewer.register_render_camera(CAMERA_ID):
		_finish_failure("could not register render camera %d" % CAMERA_ID)
		return
	for _frame in range(3):
		_queue_camera()
		await get_tree().process_frame
	var sample_path := ProjectSettings.globalize_path("res://../../exampleImmFiles/sample1.imm").simplify_path()
	var load_result := int(viewer.load_document(sample_path))
	if load_result < 0:
		_finish_failure("load_document returned %d for %s (exists=%s backend=%s)" % [
			load_result,
			sample_path,
			str(FileAccess.file_exists(sample_path)),
			str(viewer.get_render_backend_diagnostics()),
		])
		return

	if not await _wait_until_ready():
		_finish_failure("sample document did not become ready within %.1f seconds" % MAX_READY_SECONDS)
		return

	RenderingServer.set_default_clear_color(_opaque_background_color())
	if not _frame_camera_from_spawn_area():
		_finish_failure("sample document did not provide an active spawn area")
		return
	viewer.set_time(0, 0)

	var mono: Image = await _capture_mode(-1, "mono.png")
	if mono == null:
		return
	var left: Image = await _capture_mode(0, "left.png")
	if left == null:
		return
	var right: Image = await _capture_mode(1, "right.png")
	if right == null:
		return

	var background := mono.get_pixel(0, 0)
	var left_orientation := _orientation_errors(mono, left, background)
	var right_orientation := _orientation_errors(mono, right, background)
	var stereo_difference := _mean_image_difference(left, right)
	var failures: Array[String] = []
	if int(left_orientation.samples) < MIN_CONTENT_SAMPLES:
		failures.append("mono reference has too few content samples (%d)" % int(left_orientation.samples))
	if float(left_orientation.normal_error) >= float(left_orientation.flipped_error) * ORIENTATION_MARGIN:
		failures.append("left eye is not measurably closer to upright reference than vertical flip")
	if float(right_orientation.normal_error) >= float(right_orientation.flipped_error) * ORIENTATION_MARGIN:
		failures.append("right eye is not measurably closer to upright reference than vertical flip")
	if stereo_difference < MIN_STEREO_MEAN_DIFFERENCE:
		failures.append("left and right native eye renders are effectively identical")

	var result := {
		"status": "passed" if failures.is_empty() else "failed",
		"mono_path": output_directory.path_join("mono.png"),
		"left_path": output_directory.path_join("left.png"),
		"right_path": output_directory.path_join("right.png"),
		"replay_path": replay_path,
		"replay_enabled": replay_enabled,
		"left_orientation": left_orientation,
		"right_orientation": right_orientation,
		"stereo_mean_difference": stereo_difference,
		"failures": failures,
	}
	_write_result(result)
	get_tree().quit(0 if failures.is_empty() else 1)

func _wait_until_ready() -> bool:
	var deadline := Time.get_ticks_msec() + int(MAX_READY_SECONDS * 1000.0)
	while Time.get_ticks_msec() < deadline:
		if viewer.is_loaded() and viewer.is_sequence_ready():
			for _frame in range(3):
				_queue_camera()
				await get_tree().process_frame
			return true
		_queue_camera()
		await get_tree().process_frame
	return false

func _capture_mode(eye_index: int, file_name: String) -> Image:
	compositor_effect.call("set_stereo_simulation_eye", eye_index)
	var expected_mask := 0 if eye_index < 0 else (1 << eye_index)
	var mode_observed := false
	for _frame in range(SETTLE_FRAMES):
		viewer.set_time(0, 0)
		_queue_camera()
		await RenderingServer.frame_post_draw
		var diagnostics: Dictionary = compositor_effect.call("get_diagnostics")
		if eye_index < 0:
			mode_observed = not bool(diagnostics.get("last_used_stereo_simulation", true)) \
				and int(diagnostics.get("last_render_result", -1)) == 0 \
				and bool(diagnostics.get("last_composite_result", false))
		else:
			mode_observed = bool(diagnostics.get("last_used_stereo_simulation", false)) \
				and bool(diagnostics.get("last_used_stereo_replay", false)) == replay_enabled \
				and int(diagnostics.get("last_simulated_eye_index", -1)) == eye_index \
				and int(diagnostics.get("last_rendered_eye_mask", 0)) == expected_mask \
				and int(diagnostics.get("last_render_result", -1)) == 0 \
				and bool(diagnostics.get("last_composite_result", false))
	if not mode_observed:
		_finish_failure("render diagnostics did not confirm eye mode %d" % eye_index)
		return null

	_queue_camera()
	await RenderingServer.frame_post_draw
	var image := get_viewport().get_texture().get_image()
	if image == null or image.is_empty():
		_finish_failure("viewport capture for eye mode %d is empty" % eye_index)
		return null
	var save_path := output_directory.path_join(file_name)
	var save_result := image.save_png(save_path)
	if save_result != OK:
		_finish_failure("save_png returned %d for %s" % [save_result, save_path])
		return null
	return image

func _queue_camera() -> void:
	var viewport_size := get_viewport().get_visible_rect().size
	viewer.queue_render_camera_transform(
		camera.global_transform,
		maxi(int(viewport_size.x), 1),
		maxi(int(viewport_size.y), 1),
		camera.fov,
		CAMERA_ID
	)

func _configure_replay_if_requested() -> bool:
	replay_path = OS.get_environment("IMM_GODOT_STEREO_REPLAY_PATH").replace("\\", "/")
	if replay_path.is_empty():
		return true
	if not FileAccess.file_exists(replay_path):
		_finish_failure("XR replay capture does not exist: %s" % replay_path)
		return false
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(replay_path))
	if not parsed is Dictionary:
		_finish_failure("XR replay capture is not a JSON object: %s" % replay_path)
		return false
	var capture: Dictionary = parsed
	var matrix_keys := [
		"world_to_head",
		"head_projection",
		"world_to_left_eye",
		"left_eye_projection",
		"world_to_right_eye",
		"right_eye_projection",
	]
	for key in matrix_keys:
		var values: Array = capture.get(key, [])
		if values.size() != 16:
			_finish_failure("XR replay matrix %s has %d values instead of 16" % [key, values.size()])
			return false
	replay_enabled = bool(compositor_effect.call(
		"set_stereo_replay_matrices",
		PackedFloat32Array(capture.world_to_head),
		PackedFloat32Array(capture.head_projection),
		PackedFloat32Array(capture.world_to_left_eye),
		PackedFloat32Array(capture.left_eye_projection),
		PackedFloat32Array(capture.world_to_right_eye),
		PackedFloat32Array(capture.right_eye_projection)
	))
	if not replay_enabled:
		_finish_failure("native compositor rejected XR replay matrices")
		return false
	return true

func _frame_camera_from_spawn_area() -> bool:
	var spawn_info: Dictionary = viewer.get_active_spawn_area_info()
	if spawn_info.is_empty():
		return false
	var transform_info: Dictionary = spawn_info.get("transform", {})
	var basis := Basis(
		transform_info.get("basis_x", Vector3.RIGHT),
		transform_info.get("basis_y", Vector3.UP),
		transform_info.get("basis_z", Vector3.BACK)
	).orthonormalized()
	camera.global_transform = Transform3D(basis, transform_info.get("position", Vector3.ZERO))
	camera.force_update_transform()
	return true

func _orientation_errors(reference: Image, eye: Image, background: Color) -> Dictionary:
	var best_normal := INF
	var best_flipped := INF
	var best_normal_shift := 0
	var best_flipped_shift := 0
	var content_samples := 0
	for shift in range(-MAX_HORIZONTAL_ALIGNMENT, MAX_HORIZONTAL_ALIGNMENT + 1, HORIZONTAL_ALIGNMENT_STEP):
		var normal_error := 0.0
		var flipped_error := 0.0
		var samples := 0
		for y in range(0, reference.get_height(), IMAGE_SAMPLE_STEP):
			for x in range(MAX_HORIZONTAL_ALIGNMENT, reference.get_width() - MAX_HORIZONTAL_ALIGNMENT, IMAGE_SAMPLE_STEP):
				var reference_color := reference.get_pixel(x, y)
				if _color_difference(reference_color, background) < 0.03:
					continue
				normal_error += _color_difference(reference_color, eye.get_pixel(x + shift, y))
				flipped_error += _color_difference(reference_color, eye.get_pixel(x + shift, eye.get_height() - 1 - y))
				samples += 1
		if samples > 0:
			normal_error /= float(samples)
			flipped_error /= float(samples)
			content_samples = samples
			if normal_error < best_normal:
				best_normal = normal_error
				best_normal_shift = shift
			if flipped_error < best_flipped:
				best_flipped = flipped_error
				best_flipped_shift = shift
	return {
		"normal_error": best_normal,
		"flipped_error": best_flipped,
		"normal_to_flipped_ratio": best_normal / maxf(best_flipped, 0.000001),
		"normal_alignment_x": best_normal_shift,
		"flipped_alignment_x": best_flipped_shift,
		"samples": content_samples,
	}

func _mean_image_difference(first: Image, second: Image) -> float:
	var difference := 0.0
	var samples := 0
	for y in range(0, first.get_height(), IMAGE_SAMPLE_STEP):
		for x in range(0, first.get_width(), IMAGE_SAMPLE_STEP):
			difference += _color_difference(first.get_pixel(x, y), second.get_pixel(x, y))
			samples += 1
	return difference / float(maxi(samples, 1))

func _color_difference(first: Color, second: Color) -> float:
	return (absf(first.r - second.r) + absf(first.g - second.g) + absf(first.b - second.b)) / 3.0

func _opaque_background_color() -> Color:
	var color: Color = viewer.get_background_color()
	color.a = 1.0
	return color

func _resolve_output_directory() -> String:
	var configured := OS.get_environment("IMM_GODOT_STEREO_SIM_OUTPUT_DIR")
	if configured.is_empty():
		return ProjectSettings.globalize_path("user://stereo-simulation")
	return configured.replace("\\", "/")

func _finish_failure(message: String) -> void:
	_write_result({"status": "failed", "failures": [message]})
	get_tree().quit(1)

func _write_result(result: Dictionary) -> void:
	var result_file := FileAccess.open(result_path, FileAccess.WRITE)
	if result_file != null:
		result_file.store_string(JSON.stringify(result, "\t"))
	var summary_file := FileAccess.open(output_directory.path_join("summary.log"), FileAccess.WRITE)
	if summary_file != null:
		summary_file.store_line("%s %s" % [RESULT_MARKER, JSON.stringify(result)])
