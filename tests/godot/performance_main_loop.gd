extends SceneTree
# Desktop CLI entry point. Exported mobile apps install the same node as an autoload.
func _initialize() -> void:
	var path: String = get_script().resource_path.get_base_dir().path_join("performance_probe.gd")
	root.add_child(load(path).new())
