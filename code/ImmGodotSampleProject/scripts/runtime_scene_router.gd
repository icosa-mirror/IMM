extends Node

const ROUTE_PREFIX := "--imm-godot-ci-scene="
const DEFAULT_SCENE := "res://scenes/SampleScene.tscn"
const ROUTED_SCENES := {
	"compatibility": "res://scenes/IOSSimulatorCompatibilityProbe.tscn",
	"sample": DEFAULT_SCENE,
	"visual-smoke": "res://scenes/VisualSmokeScene.tscn",
}

func _ready() -> void:
	call_deferred("_route_to_requested_scene")

func _route_to_requested_scene() -> void:
	var route := "sample"
	var arguments := OS.get_cmdline_args()
	for argument in OS.get_cmdline_user_args():
		if not arguments.has(argument):
			arguments.append(argument)
	for argument in arguments:
		if argument.begins_with(ROUTE_PREFIX):
			route = argument.substr(ROUTE_PREFIX.length())
			break
	var scene_path: String = ROUTED_SCENES.get(route, "")
	if scene_path.is_empty():
		push_error("Unknown IMM Godot CI scene route: %s" % route)
		get_tree().quit(2)
		return
	var result := get_tree().change_scene_to_file(scene_path)
	if result != OK:
		push_error("Failed to route IMM Godot scene '%s' to %s: %s" % [
			route,
			scene_path,
			error_string(result),
		])
		get_tree().quit(2)
