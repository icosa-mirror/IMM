# IMM Godot Sample Project

This project mirrors the intent of the Unity sample project while the native Godot integration is still being built.

## Current status

- The scene and controls are in place.
- The `ImmViewerNode` script mirrors the runtime API shape the future GDExtension will expose.
- The native `appImmGodot` DLL exists in the C++ solution, and the GDExtension source now owns native init/shutdown, camera registration, matrix submission, and render-thread queueing.
- A `.gdextension` manifest is present under `addons/imm_viewer/`, ready for the future extension binary.
- The final `godot-cpp` build and real Godot Compatibility renderer validation are still pending.

## Open in Godot

1. Open `code/ImmGodotSampleProject` in Godot 4.x.
2. Use the Compatibility renderer path.
3. Run `scenes/SampleScene.tscn`.

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
- `\`: queue a fixed-viewport render-thread smoke hook
- `W/A/S/D/Q/E`: move camera

## Document path

The default document path is set to `../../../exampleImmFiles/sample1.imm`, which points at the repository sample from this project directory.

## Next integration step

Build the GDExtension with `godot-cpp`, open `NativeSmokeScene.tscn`, then validate the `ImmViewerNode`-owned per-frame camera/viewport queue in a real Godot Compatibility renderer context. On Windows, `.\code\projects\windows\build-godot-extension.ps1 -Configuration Release -BootstrapGodotCpp -BuildGodotCpp` clones the default Godot 4.2-compatible `godot-cpp` bindings into `thirdparty\godot-cpp`, builds them, and builds the extension.

Add `-RunSmoke -GodotExe C:\path\to\Godot_v4.2.2-stable_win64.exe` to the same command to run the native smoke scene immediately after the build.

The native build scaffold writes the GDExtension DLL to `bin/windows/{debug,release}/`, which matches `addons/imm_viewer/imm_viewer.gdextension`, stages `ImmGodotPlugin.dll` plus the IMM runtime dependency DLLs beside it for Godot's extension loader, verifies the complete staged DLL set before reporting build success, and writes `godot-extension-dlls.txt` beside the DLLs for CI artifact diagnostics.

If Godot is not installed, `python code/appImmGodotGDExtension/verify_local.py` checks the sample/native API boundary, `.gdextension` manifest paths, Compatibility renderer setting, script-stub/native scene structure, native `ImmViewerNode` registration and method bindings, `ImmViewerNode` camera registration plus camera/viewport render queue ownership, Windows `godot-cpp` bootstrap/CI/smoke wiring, source paths for the IMM runtime dependency DLLs staged by SCons, PowerShell helper syntax when PowerShell is available, `ImmGodot` C ABI export alignment, local Python files, and the `appImmGodot` syntax-only compile when `clang++` is available. If `GODOT_CPP_PATH` or `thirdparty/godot-cpp` points at a checkout with generated bindings, it also syntax-checks the GDExtension sources against the real Godot C++ headers. On Windows, `.\code\projects\windows\build-godot-extension.ps1 -VerifyOnly` runs the same local verification without requiring MSBuild, SCons, or `godot-cpp`.

With Godot installed, add `IMM_GODOT_RUN_LOCAL_SMOKE=1` to `verify_local.py` to run the script-stub smoke scene headlessly. This validates project loading, GDScript parsing, scene wiring, `auto_queue_render`, `load_document()`, `is_loaded()`, document state/background color, chapter/bounds/layer/spawn-area query APIs, playback controls, the camera/viewport queue, and render diagnostics before the native Windows extension is available.

The `ImmViewer` node has `auto_queue_render = true` and `render_camera_path = ../CameraRig/Camera3D`. That makes `ImmViewerNode` register camera 0 and queue the active camera transform, field of view, and viewport dimensions each frame while a document is loaded. Press `\` to schedule the older fixed-viewport smoke render call through `RenderingServer.call_on_render_thread`; this remains a Phase 1 fallback hook.

With Godot installed and the extension DLLs built, `.\code\projects\windows\run-godot-smoke.ps1 -Configuration Release -RequireExtension` first verifies that the GDExtension DLL, `ImmGodotPlugin.dll`, and staged IMM runtime dependency DLLs exist, then runs the headless smoke script against `NativeSmokeScene.tscn`. It asserts `ImmViewer` is the native `ImmViewerNode`, verifies camera 0 was auto-registered by `auto_queue_render`, loads the sample document, checks document state/background color, exercises chapter/bounds/layer/spawn-area query APIs, exercises playback controls, exercises the registered camera/viewport queue, validates render diagnostics including adapter graphics/before/after callback counts, and requires the `IMM Godot smoke test passed` output marker. Add `-LogDir artifacts\godot-smoke` to save smoke output, run metadata, and the native staged-DLL inventory. Without `-RequireExtension`, the smoke script uses the script-stub `SampleScene.tscn`.

The Windows workflow runs both forms: script-stub smoke before the native build and native-extension smoke after the GDExtension build.

The status label also displays the current IMM background color reported by `get_background_color()`, which is the first parity hook for matching Unity camera clear-color behavior.

The status label displays playback time from `get_play_time_seconds()`, and `,` / `.` call `seek_relative_seconds()` to exercise the native `piTick` time API.

The status label displays `get_current_chapter()` and `get_chapter_count()`, while `[`, `]`, and `set_chapter()` share the direct native chapter API.

The status label also reports `get_document_state()`, `is_sequence_ready()`, and document info flags so native loading/playback state can be compared against the Unity wrapper.

The status label reports `get_layer_count()` and `get_bounding_box()` when the native sequence is ready, matching the Unity wrapper's document inspection path.

The `V` key exercises the native layer override path by toggling the first layer through `set_layer_visible()` and displaying `get_layer_diagnostics()` status.

The viewer API also mirrors Unity's layer transform override calls with `set_layer_transform()` and `clear_layer_transform_override()`.

The viewer API now includes `set_document_transform()`. In the native GDExtension path this forwards to `ImmGodot_SetDocumentToWorld`; in the script stub it stores the value so sample code can call the same API before the native DLL is present.

The spawn-area controls now query `get_active_spawn_area_info()` and move the camera rig to the active authored pose, including the same head-offset compensation used by the Unity sample. In script-stub mode no authored spawn-area data is available, so the keys only update status.
