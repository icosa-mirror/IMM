# IMM Viewer Godot Addon

This addon provides the native `ImmViewerNode` and `ImmViewerCompositorEffect` used to play `.imm` files in Godot.

## Install in a new Godot project

1. Unzip `ImmPlayerPlugin-Godot.zip` at the root of your Godot project.
   - The project should then contain `addons/imm_viewer/imm_viewer.gdextension`.
2. Restart the Godot editor so the GDExtension is discovered.
3. Copy `sample1.imm` into the project root beside `project.godot`.
   - The sample path is then `res://sample1.imm`.
4. In **Project > Project Settings > Rendering > Renderer**, use Forward+.

## Create a new playback scene

1. Create a new 3D scene.
2. Add a `Camera3D` and make it current.
3. In the camera Inspector, add a compositor:
   - `Camera3D > Compositor > New Compositor`
   - Open the compositor resource.
   - Add one effect: `ImmViewerCompositorEffect`.
4. Add an `ImmViewerNode` to the scene.
5. Set the `ImmViewerNode` properties:
   - `document_path`: `res://sample1.imm`
   - `auto_queue_render`: enabled
   - `render_camera_path`: path to the `Camera3D`
   - `load_on_ready`: enabled
6. Save the scene and press Run.

The node loads `sample1.imm`, queues the camera each frame, and the compositor effect renders the IMM output into the active Godot camera.
