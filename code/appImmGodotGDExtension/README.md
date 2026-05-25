# appImmGodotGDExtension

This folder contains the Godot 4 GDExtension-side integration scaffold for `ImmGodotPlugin`.

## Current state

- The source files define the `ImmViewerNode` native class and registration flow.
- `ImmViewerNode` initializes and shuts down the native `ImmGodotPlugin` backend.
- The native plugin exposes a render-adapter C ABI for graphics init/shutdown and before/after camera render callbacks.
- `ImmViewerNode` registers those callbacks and exposes `get_render_diagnostics()` for smoke tests.
- The sample project has a native smoke scene that instantiates the GDExtension class directly when `IMM_GODOT_REQUIRE_EXTENSION=1`.
- `SConstruct` builds the Windows and macOS GDExtension when `godot-cpp` is available.
- `SConstruct` resolves repository paths from its own location, so it can be launched from the extension folder or via `scons -f`.
- The Windows build helper stages `ImmGodotPlugin.dll` and its runtime media DLL dependencies beside the GDExtension DLL and writes `godot-extension-dlls.txt`.
- `get_document_state()` exposes native loading/playback state for smoke diagnostics and CI triage.
- `get_bounding_box()` exposes native document bounds for background/bounding placement parity checks.
- `get_background_color()` exposes the native player-info background color used by the Unity clear-color path and is covered by smoke diagnostics.
- Spawn-area query/jump methods expose native spawn IDs, active spawn state, and serialized spawn-area info to Godot scripts.
- `set_document_transform(document_transform)` forwards document-to-world placement into the native backend and is covered by smoke diagnostics.
- `matrix_debug_logging` enables native matrix logging and `get_render_diagnostics()` exposes the last submitted world-to-head and projection matrices for parity checks.
- `set_camera_matrices(camera_id, world_to_head, projection)` accepts explicit 16-float matrix arrays so smoke/parity tests can verify deterministic camera feeds.
- The smoke runner prints `IMM_GODOT_MATRIX_DIAGNOSTICS_JSON`; the Windows wrapper validates the JSON schema/camera/matrix shape, deterministic document/camera/projection values, and document/background/bounds/spawn diagnostic fields, then saves it as `godot-matrix-diagnostics.json`.
- The native node initializes the backend with `user://imm_godot_log.txt`; the Windows smoke wrapper copies that native log into the smoke artifact folder when it is present.
- Native smoke now waits for the real native loaded state by pumping `global_work()` and `render_camera()`, then requires valid bounds and spawn-area diagnostics before accepting render success.
- On macOS, setting `IMM_GODOT_NATIVE_CAPTURE_PATH` writes an offscreen Metal render-target PNG through the native bridge. This proves IMM pixels render natively, while Godot viewport presentation remains the next Phase 2 task.
- The sample project includes a `.gdextension` manifest that points at the future binary location.

## Build dependency

The Windows helper can clone `godot-cpp` into `thirdparty/godot-cpp`:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -BootstrapGodotCpp -BuildGodotCpp
```

The build expects these headers to resolve:

- `godot_cpp/classes/*`
- `godot_cpp/core/*`
- `godot_cpp/variant/*`

## Local verification

Run the scaffold verifier from the repository root:

```bash
python3 code/appImmGodotGDExtension/verify_local.py
```

To also run the script-stub Godot smoke locally:

```bash
IMM_GODOT_RUN_LOCAL_SMOKE=1 GODOT_EXE=/Applications/Godot.app/Contents/MacOS/Godot python3 code/appImmGodotGDExtension/verify_local.py
```

The Windows CI job runs the fast verifier before building the native GDExtension.
When PowerShell is available, the verifier also parses the Windows helper scripts with PowerShell's AST parser.

## macOS native smoke

After building `ImmGodotPlugin` with Apple Clang and building the macOS GDExtension with SCons, run:

```bash
IMM_GODOT_REQUIRE_EXTENSION=1 \
IMM_GODOT_CAPTURE_PATH="$PWD/build/validation/godot-native-smoke.png" \
IMM_GODOT_NATIVE_CAPTURE_PATH="$PWD/build/validation/imm-native-offscreen.png" \
/Applications/Godot.app/Contents/MacOS/Godot \
  --path code/ImmGodotSampleProject \
  --script res://scripts/smoke_test_runner.gd
```

The expected current result is a passing native smoke with valid diagnostics and a non-flat `imm-native-offscreen.png`. The Godot viewport capture is still a flat background image until native texture presentation is wired into Godot.

## Intended runtime shape

- `ImmViewerNode` owns native IMM session lifecycle.
- It exposes load/unload/playback/spawn-area APIs to Godot scripts.
- It is the handoff point for render-thread camera capture and draw callbacks in Phase 2.
- Render diagnostics and offscreen capture currently prove native rendering; presenting the native render target in the Godot viewport remains a Phase 2 hardening item.
