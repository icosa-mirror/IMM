extends Node3D

const MOVE_SPEED := 4.0
const BOOST_MULTIPLIER := 3.0

@onready var viewer = $ImmViewer
@onready var camera_rig: Node3D = $CameraRig
@onready var camera: Camera3D = $CameraRig/Camera3D
@onready var status_label: Label3D = $StatusLabel

var _last_render_result := 0

func _ready() -> void:
	viewer.document_loaded.connect(_update_status)
	viewer.document_unloaded.connect(_update_status)
	viewer.playback_changed.connect(_on_playback_changed)
	viewer.spawn_area_changed.connect(_on_spawn_area_changed)
	_update_status()

func _process(delta: float) -> void:
	_handle_movement(delta)
	viewer.set_camera_transform(camera.global_transform)
	if viewer.is_loaded():
		var viewport_size := get_viewport().get_visible_rect().size
		var render_result: int = viewer.render_camera(0, Vector2i(int(viewport_size.x), int(viewport_size.y)), 0)
		if render_result != _last_render_result:
			_last_render_result = render_result
			_update_status()

func _handle_movement(delta: float) -> void:
	var move_input := Vector3.ZERO
	if Input.is_key_pressed(KEY_W):
		move_input.z -= 1.0
	if Input.is_key_pressed(KEY_S):
		move_input.z += 1.0
	if Input.is_key_pressed(KEY_A):
		move_input.x -= 1.0
	if Input.is_key_pressed(KEY_D):
		move_input.x += 1.0
	if Input.is_key_pressed(KEY_E):
		move_input.y += 1.0
	if Input.is_key_pressed(KEY_Q):
		move_input.y -= 1.0

	if move_input == Vector3.ZERO:
		return

	var speed := MOVE_SPEED
	if Input.is_key_pressed(KEY_SHIFT):
		speed *= BOOST_MULTIPLIER

	camera_rig.translate_object_local(move_input.normalized() * speed * delta)

func _unhandled_key_input(event: InputEvent) -> void:
	if event is not InputEventKey:
		return
	if not event.pressed or event.echo:
		return

	match event.keycode:
		KEY_L:
			viewer.load_document()
		KEY_U:
			viewer.unload_document()
		KEY_SPACE:
			viewer.toggle_pause()
		KEY_R:
			viewer.restart()
		KEY_BRACKETRIGHT:
			viewer.next_chapter()
		KEY_BRACKETLEFT:
			viewer.previous_chapter()
		KEY_APOSTROPHE:
			viewer.next_spawn_area()
		KEY_SEMICOLON:
			viewer.previous_spawn_area()

func _on_playback_changed(_is_playing: bool) -> void:
	_update_status()

func _on_spawn_area_changed(_active_index: int) -> void:
	_update_status()

func _update_status(_unused: Variant = null) -> void:
	var lines := PackedStringArray()
	lines.append("IMM Godot Sample")
	lines.append("L load | U unload | Space play/pause | R restart")
	lines.append("[ / ] chapter | ; / ' spawn area | WASDQE move")
	lines.append("")
	lines.append("Document: %s" % ("loaded" if viewer.is_loaded() else "not loaded"))
	lines.append("Playback: %s" % ("playing" if viewer.is_playing() else "paused"))
	lines.append("Last Render Result: %d" % _last_render_result)
	lines.append("Path: %s" % viewer.document_path)
	lines.append("Spawn Area Index: %d" % viewer.get_active_spawn_area_index())
	status_label.text = "\n".join(lines)
