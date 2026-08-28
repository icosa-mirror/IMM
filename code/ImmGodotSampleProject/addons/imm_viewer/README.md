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
   - Set `Effect Callback Type` to `Pre Transparent`.
   - Enable `Access Resolved Color`, `Access Resolved Depth`, and `Render Graph Depth Composition Enabled` so Godot geometry and IMM content share depth correctly.
4. Add an `ImmViewerNode` to the scene.
5. Set the `ImmViewerNode` properties:
   - `document_path`: `res://sample1.imm`
   - `auto_queue_render`: enabled
   - `render_camera_path`: path to the `Camera3D`
   - `load_on_ready`: enabled
6. Save the scene and press Run.

The node loads `sample1.imm`, queues the camera each frame, and the compositor effect renders the IMM output into the active Godot camera.

## Foreground Regression Check

If Godot shows the blurred 360 background but not authored foreground paint, first check the shared bridge depth convention in `code/appImmShared/src/imm_engine_bridge.cpp`. Hosted Metal/Vulkan still use zero-to-one clip/projection matrices, but the player depth buffer convention must remain `DepthBuffer::Linear01`, matching the standalone viewer.

Run the authored-spawn visual smoke capture and inspect the PNG for foreground content:

CI leaves `IMM_GODOT_VISUAL_SMOKE_PLAYER_FRAME` unset and therefore captures frame 60, matching the committed Windows reference. Set it explicitly, as below, only when inspecting another authored frame.

```sh
IMM_GODOT_VISUAL_SMOKE=1 \
IMM_GODOT_VISUAL_SMOKE_RELOAD_CYCLES=0 \
IMM_GODOT_VISUAL_SMOKE_USE_SPAWN_AREA=1 \
IMM_GODOT_VISUAL_SMOKE_PLAYER_FRAME=240 \
IMM_GODOT_VISUAL_SMOKE_PNG="$PWD/artifacts/godot-foreground-check/spawn.png" \
/Applications/Godot.app/Contents/MacOS/Godot \
  --path code/ImmGodotSampleProject \
  --rendering-driver metal \
  --rendering-method forward_plus \
  --scene res://scenes/VisualSmokeScene.tscn \
  --fixed-fps 30
```
