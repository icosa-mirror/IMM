extends Node2D

const LOG_PREFIX := "[IMM_GODOT_IOS_COMPAT_PROBE_20260814]"
const PNG_PATH := "user://godot-ios-compatibility-probe.png"
const RESULT_PATH := "user://godot-ios-compatibility-probe-result.log"

func _ready() -> void:
	_write_result("ready", "running", OK)
	queue_redraw()
	call_deferred("_run_probe")

func _draw() -> void:
	var viewport_size := get_viewport_rect().size
	var short_side := minf(viewport_size.x, viewport_size.y)
	draw_rect(Rect2(Vector2.ZERO, viewport_size), Color(0.04, 0.08, 0.16, 1.0))
	draw_circle(viewport_size * 0.5, short_side * 0.24, Color(0.1, 0.75, 0.95, 1.0))
	draw_rect(Rect2(viewport_size * 0.36, viewport_size * 0.12), Color(1.0, 0.78, 0.08, 1.0))

func _run_probe() -> void:
	for _frame in range(4):
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var image := get_viewport().get_texture().get_image()
	var save_result := image.save_png(PNG_PATH)
	var status := "passed" if save_result == OK and not image.is_empty() else "failed"
	_write_result("capture", status, save_result)
	print("%s status=%s driver=%s method=%s png=%s save_result=%d size=%s" % [
		LOG_PREFIX,
		status,
		RenderingServer.get_current_rendering_driver_name(),
		RenderingServer.get_current_rendering_method(),
		PNG_PATH,
		save_result,
		str(image.get_size()),
	])
	get_tree().quit(0 if status == "passed" else 1)

func _write_result(phase: String, status: String, save_result: int) -> void:
	var result_file := FileAccess.open(RESULT_PATH, FileAccess.WRITE)
	if result_file == null:
		push_error("%s could not open %s: %s" % [LOG_PREFIX, RESULT_PATH, error_string(FileAccess.get_open_error())])
		return
	result_file.store_line("%s phase=%s status=%s driver=%s method=%s save_result=%d" % [
		LOG_PREFIX,
		phase,
		status,
		RenderingServer.get_current_rendering_driver_name(),
		RenderingServer.get_current_rendering_method(),
		save_result,
	])
