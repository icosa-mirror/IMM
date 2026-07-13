# IMM Godot Sample Project

This project mirrors the intent of the Unity sample project and exercises the native Godot integration.

## Current status

- The scene and controls are in place.
- The `ImmViewerNode` script mirrors the runtime API shape the native GDExtension exposes.
- The native `appImmGodot` plugin and GDExtension own native init/shutdown, camera registration, matrix submission, and compositor render request publishing.
- A `.gdextension` manifest is present under `addons/imm_viewer/`.
- The project Run button opens the macOS Forward+/Metal scene by default, and the script-stub smoke plus macOS Forward+/Metal visual smoke pass locally. Windows currently validates GDExtension build/staging in CI, not native rendering.

## Open in Godot

1. Open `code/ImmGodotSampleProject` in Godot 4.5 or newer.
2. Use Forward+ rendering. On macOS, Godot 4.6.1 selects Metal for the Forward+ path.
3. Press Run. The project main scene is `scenes/MetalVisualSmokeScene.tscn`, which loads `sample1.imm` through the native GDExtension and compositor.

## Using the addon in a new project

The CI artifact (`ImmGodotGDExtension-macOS`) is a self-contained `addons/imm_viewer/` folder. Copy it into your Godot project's root so you have `res://addons/imm_viewer/`.

### macOS quarantine (required after downloading from CI)

macOS Gatekeeper quarantines files downloaded from the internet. The dylibs are linker-signed by Apple's toolchain; running `codesign` on them again breaks the signature. The correct fix is to remove the quarantine attribute recursively — **do not** codesign CI-built dylibs manually:

```bash
xattr -dr com.apple.quarantine addons/imm_viewer/
```

Run this once from your project root before opening the project in Godot. If you skip this step Godot will crash (SIGABRT) when it tries to load the extension.

### Scene setup

1. **Add an `ImmViewerNode`** to your scene tree. Set its properties in the Inspector:
   - `document_path`: path to your `.imm` file (e.g. `res://myfile.imm`)
   - `load_on_ready`: enable to load automatically when the scene starts
   - `auto_play`: enable to start playback immediately after loading
   - `auto_queue_render`: enable to let the node submit camera transforms each frame
   - `render_camera_path`: set to the path of your `Camera3D` node

2. **Attach the compositor effect to your Camera3D.** This is the step that makes rendering actually appear on screen — without it the IMM backend runs but nothing composites into the viewport:
   1. Select your `Camera3D` in the Scene tree
   2. In the Inspector, find the **Compositor** property (in the Camera3D section)
   3. Click the field → **New Compositor**
   4. Click the new Compositor resource to expand it
   5. Find **Compositor Effects** → click the array → **Add Element**
   6. In the new element slot, choose **ImmViewerCompositorEffect**

The scene then needs: a `Camera3D` with `ImmViewerCompositorEffect` in its compositor, and an `ImmViewerNode` pointing at that camera via `render_camera_path`. No `WorldEnvironment` is required.

### Minimum working scene

```
Node3D (root)
├── Camera3D          ← compositor → Compositor [ImmViewerCompositorEffect]
└── ImmViewerNode     ← render_camera_path: ../Camera3D
                         document_path: res://myfile.imm
                         load_on_ready: true
                         auto_play: true
                         auto_queue_render: true
```

## Sample controls

- `L`: load document
- `U`: unload document
- `Space`: pause/resume
- `R`: restart
- `,`: seek backward one second
- `.`: seek forward one second
- `V`: toggle first layer visibility when native layer info is available
- `]`: next chapter
- `[`: previous chapter
- `'`: next spawn area
- `;`: previous spawn area
- `\`: queue a fixed-viewport diagnostic render request
- `W/A/S/D/Q/E`: move camera

## Document path

The default document path is set to `../../../exampleImmFiles/sample1.imm`, which points at the repository sample from this project directory.

## Native build and validation

Build the GDExtension with `godot-cpp`, run the script-stub smoke, and run the macOS Forward+/Metal visual smoke when validating visible rendering. On Windows, `.\code\projects\windows\build-godot-extension.ps1 -Configuration Release -BootstrapGodotCpp -BuildGodotCpp` clones the default Godot 4.5-compatible `godot-cpp` bindings into `thirdparty\godot-cpp`, builds them, and builds the extension.

For Windows renderer-backend work, add `-RunSmoke -GodotExe C:\path\to\Godot_v4.5-stable_win64.exe` to the same command to run the native smoke scene immediately after the build. This is not the current Windows CI gate because Windows does not yet have a valid Godot-compatible production renderer backend.

The native build scaffold writes the GDExtension DLL to `addons/imm_viewer/bin/windows/{debug,release}/`, which matches `addons/imm_viewer/imm_viewer.gdextension`, stages `ImmGodotPlugin.dll` plus the IMM runtime dependency DLLs beside it for Godot's extension loader, verifies the complete staged DLL set before reporting build success, and writes `godot-extension-dlls.txt` beside the DLLs for CI artifact diagnostics. The addon is self-contained, so distributing it is just a matter of copying the `addons/imm_viewer/` folder.

If Godot is not installed, `python code/appImmGodotGDExtension/verify_local.py` checks the sample/native API boundary, `.gdextension` manifest paths, Forward+ project default, Run-button main scene, script-stub/native scene structure, native `ImmViewerNode` registration and method bindings, `ImmViewerNode` camera registration plus camera/viewport render queue ownership, Windows `godot-cpp` bootstrap/CI/smoke wiring, source paths for the IMM runtime dependency DLLs staged by SCons, PowerShell helper syntax when PowerShell is available, `ImmGodot` C ABI export alignment, local Python files, and the `appImmGodot` syntax-only compile when `clang++` is available. If `GODOT_CPP_PATH` or `thirdparty/godot-cpp` points at a Godot 4.5 `godot-cpp` checkout with generated bindings, it also syntax-checks the GDExtension sources against the real Godot C++ headers. On Windows, `.\code\projects\windows\build-godot-extension.ps1 -VerifyOnly` runs the same local verification without requiring MSBuild, SCons, or `godot-cpp`.

With Godot installed, add `IMM_GODOT_RUN_LOCAL_SMOKE=1` to `verify_local.py` to run the script-stub smoke scene headlessly. This validates project loading, GDScript parsing, scene wiring, `auto_queue_render`, `load_document()`, `is_loaded()`, document state/background color, chapter/bounds/layer/spawn-area query APIs, playback controls, playback time snapshots/seek math, document/playback/spawn-area signals, native backend signal parity, the camera/viewport queue, and render diagnostics before the native Windows extension is available.

The `ImmViewer` node has `auto_queue_render = true` and `render_camera_path = ../CameraRig/Camera3D`. That makes `ImmViewerNode` register camera 0 and queue the active camera transform, field of view, and viewport dimensions each frame while a document is loaded. In the Metal visual scene, queued work is consumed by `ImmViewerCompositorEffect` and rendered into Godot-owned render resources. Press `\` to queue a fixed-viewport diagnostic render request.

With Godot installed and the extension DLLs built, `.\code\projects\windows\run-godot-smoke.ps1 -Configuration Release -RequireExtension` first verifies that the GDExtension DLL, `ImmGodotPlugin.dll`, and staged IMM runtime dependency DLLs exist, then runs the headless smoke script against `NativeSmokeScene.tscn`. It asserts `ImmViewer` is the native `ImmViewerNode`, verifies camera 0 was auto-registered by `auto_queue_render`, loads the sample document, checks document state/background color, exercises chapter/bounds/layer/spawn-area query APIs, validates every authored spawn-area dictionary, exercises playback controls and signal emissions, verifies playback time APIs remain safe before the native timeline-ready state, exercises the registered camera/viewport queue, validates render diagnostics including adapter graphics/before/after callback counts, and requires the `IMM Godot smoke test passed` output marker. Add `-LoadUnloadCycles 2` to repeatedly unload/reload the document while the render queue remains active, and add `-LogDir artifacts\godot-smoke` to save smoke output, run metadata, and the native staged-DLL inventory. Without `-RequireExtension`, the smoke script uses the script-stub `ScriptSmokeScene.tscn`.

The Windows workflow runs `ScriptSmokeScene.tscn`, which contains no native
resource types, before the native build. It runs a native-extension preflight
after the GDExtension build. The preflight validates staged DLLs and Godot editor
lookup paths without launching native IMM rendering, because Windows does not
yet have a valid Godot-compatible production renderer backend. `SampleScene.tscn`
remains the normal native sample rather than doubling as the pre-build stub test.
Use macOS Forward+/Metal for the current visible-rendering gate.

The sample and smoke scenes apply the current IMM background color from `get_background_color()` to `RenderingServer.set_default_clear_color(...)` before camera rendering. The status label also displays that color for quick diagnostics.

The status label displays playback time from `get_play_time_seconds()`, and `,` / `.` call `seek_relative_seconds()` to exercise the native `piTick` time API.

The `-` and `=` keys call `set_volume()` and display `get_volume()`, exercising the same document-volume path used by the Unity wrapper.

The status label displays `get_current_chapter()` and `get_chapter_count()`, while `[`, `]`, and `set_chapter()` share the direct native chapter API.

The status label also reports `get_document_state()`, `is_sequence_ready()`, and document info flags so native loading/playback state can be compared against the Unity wrapper.

The status label reports `get_layer_count()` and `get_bounding_box()` when the native sequence is ready, matching the Unity wrapper's document inspection path.

The `V` key exercises the native layer override path by toggling the first layer through `set_layer_visible()` and displaying `get_layer_diagnostics()` status.

The viewer API also mirrors Unity's layer transform override calls with `set_layer_transform()` and `clear_layer_transform_override()`.

The viewer API now includes `set_document_transform()` and `get_document_transform()`. In the native GDExtension path this forwards to `ImmGodot_SetDocumentToWorld`; in the script stub it stores the value so sample code can call the same API before the native DLL is present.

The spawn-area controls now query `get_active_spawn_area_info()` and move the camera rig to the active authored pose, including the same head-offset compensation used by the Unity sample. In script-stub mode no authored spawn-area data is available, so the keys only update status.
