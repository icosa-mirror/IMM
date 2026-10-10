extends SceneTree
## Run the delivered main scene without fixed-fps or visual-smoke timing.
## Usage: IMM_PERFORMANCE_OUTPUT=... godot --path <sample> --script <this file>

var _scene: Node
var _viewer: Node
var _output := OS.get_environment("IMM_PERFORMANCE_OUTPUT")
var _started := Time.get_ticks_usec()
var _ready_at := -1
var _sample_at := -1
var _previous_render := -1
var _last_render_count := 0
var _last_engine_frame := -1
var _finished := false
var _sample := {
	"schema": "imm-frame-sample-v1", "product": "godot",
	"platform": OS.get_name(), "graphics_api": "unknown",
	"engine_version": Engine.get_version_info().get("string", "unknown"),
	"width": 0, "height": 0, "warmup_seconds": 2.0, "requested_seconds": 5.0,
	"observed_seconds": 0.0, "engine_frame_count": 0, "imm_rendered_frame_count": 0,
	"intervals_ms": [], "errors": []
}

func _initialize() -> void:
	call_deferred("_start")

func _duration(name: String, fallback: float) -> float:
	var text := OS.get_environment(name)
	if text.is_empty():
		return fallback
	return text.to_float() if text.is_valid_float() else -1.0

func _start() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--imm-performance-output="):
			_output = argument.trim_prefix("--imm-performance-output=")
	if _output.is_empty():
		push_error("[IMM-FPS-GODOT] IMM_PERFORMANCE_OUTPUT is required")
		quit(2)
		return
	_sample.warmup_seconds = _duration("IMM_PERFORMANCE_WARMUP_SECONDS", 2.0)
	_sample.requested_seconds = _duration("IMM_PERFORMANCE_SAMPLE_SECONDS", 5.0)
	for duration in [_sample.warmup_seconds, _sample.requested_seconds]:
		if not is_finite(duration) or duration <= 0 or duration > 30:
			_finish("Invalid sampling duration; expected a number in (0, 30].")
			return
	for argument in OS.get_cmdline_args():
		if argument == "--fixed-fps" or argument.begins_with("--fixed-fps="):
			_finish("Synthetic fixed-fps timing is unsuitable for performance evidence.")
			return
	var packed := load(ProjectSettings.get_setting("application/run/main_scene")) as PackedScene
	if packed == null:
		_finish("Cannot load the normal sample main scene.")
		return
	_scene = packed.instantiate()
	root.add_child(_scene)
	current_scene = _scene
	RenderingServer.frame_post_draw.connect(_observe_render)
	_sample.graphics_api = RenderingServer.get_current_rendering_driver_name()
	_sample.gpu = RenderingServer.get_video_adapter_name()
	_sample.vsync_mode = DisplayServer.window_get_vsync_mode()
	_sample.target_frame_rate = Engine.max_fps

func _process(_delta: float) -> bool:
	if _finished or _scene == null:
		return false
	var now := Time.get_ticks_usec()
	if _viewer == null:
		_viewer = _scene.get("viewer") as Node
	if _sample_at < 0:
		var ready: bool = _viewer != null and _viewer.is_loaded() and _viewer.is_sequence_ready() \
			and bool(_scene.get("_initial_camera_framed"))
		if ready:
			if _ready_at < 0:
				_ready_at = now
			if (now - _ready_at) / 1000000.0 >= _sample.warmup_seconds:
				var diagnostics: Dictionary = _viewer.get_render_diagnostics()
				_sample.width = int(diagnostics.get("last_viewport_width", 0))
				_sample.height = int(diagnostics.get("last_viewport_height", 0))
				_last_render_count = int(diagnostics.get("adapter_after_render_count", 0))
				_sample_at = now
				_previous_render = now
		else:
			_ready_at = -1
		if _sample_at < 0 and (now - _started) / 1000000.0 >= 90:
			_finish("Normal sample did not become ready within 90 seconds.")
		return false
	_sample.engine_frame_count += 1
	_sample.observed_seconds = (now - _sample_at) / 1000000.0
	if _sample.observed_seconds >= _sample.requested_seconds:
		_finish("")
	return false

func _observe_render() -> void:
	if _finished or _sample_at < 0 or _viewer == null:
		return
	var frame := Engine.get_frames_drawn()
	if frame == _last_engine_frame:
		return
	_last_engine_frame = frame
	var diagnostics: Dictionary = _viewer.get_render_diagnostics()
	var count := int(diagnostics.get("adapter_after_render_count", 0))
	if count <= _last_render_count:
		return
	_last_render_count = count
	if int(diagnostics.get("adapter_last_render_result", -1)) != 0 \
		or int(diagnostics.get("performance_info_result", -1)) != 0:
		_finish("IMM render or performance diagnostics failed.")
		return
	if int(diagnostics.get("num_draw_calls", 0)) <= 0:
		return
	if _sample.width != int(diagnostics.get("last_viewport_width", 0)) \
		or _sample.height != int(diagnostics.get("last_viewport_height", 0)):
		_finish("Rendering resolution changed during sampling.")
		return
	var now := Time.get_ticks_usec()
	_sample.intervals_ms.append((now - _previous_render) / 1000.0)
	_previous_render = now
	_sample.imm_rendered_frame_count += 1

func _finish(error: String) -> void:
	if _finished:
		return
	_finished = true
	if not error.is_empty():
		_sample.errors.append(error)
	var directory := _output.get_base_dir()
	if not directory.is_empty():
		DirAccess.make_dir_recursive_absolute(directory)
	var file := FileAccess.open(_output, FileAccess.WRITE)
	if file == null:
		push_error("[IMM-FPS-GODOT] Cannot write %s: %s" % [_output, FileAccess.get_open_error()])
		quit(2)
		return
	file.store_string(JSON.stringify(_sample, "\t"))
	file.close()
	print("[IMM-FPS-GODOT] Wrote %s; IMM frames=%d, seconds=%.3f" % [
		_output, _sample.imm_rendered_frame_count, _sample.observed_seconds])
	quit(0 if _sample.errors.is_empty() else 1)

func _finalize() -> void:
	if not _finished and not _output.is_empty():
		_finish("Player stopped before sampling completed.")
