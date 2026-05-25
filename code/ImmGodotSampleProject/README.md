# IMM Godot Sample Project

This project mirrors the intent of the Unity sample project while the native Godot integration is still being built.

## Current status

- The scene and controls are in place.
- The `ImmViewerNode` script mirrors the runtime API shape the future GDExtension will expose.
- The native `appImmGodot` DLL exists in the C++ solution and the GDExtension source now initializes it.
- On macOS, the native `ImmGodotPlugin` bridge can be compiled locally through `code/projects/macos/CMakeLists.txt`, producing `build/macos-godot-appleclang/godot/libImmGodotPlugin.dylib` when built with Apple Clang.
- A `.gdextension` manifest is present under `addons/imm_viewer/`, pointing at staged Windows and macOS GDExtension binaries under `bin/{windows,macos}/{debug,release}`.
- On macOS, the native bridge uses IMM's Metal renderer for local validation.
- `scripts/smoke_test_runner.gd` verifies the sample API, playback controls, and render diagnostics in script-stub mode.
- `scenes/NativeSmokeScene.tscn` uses the native `ImmViewerNode` class and is selected by the smoke runner when `IMM_GODOT_REQUIRE_EXTENSION=1`.
- Native smoke waits for the IMM document to reach the real loaded state, verifies valid bounds/spawn diagnostics, and repeats load/render/unload lifecycle cycles.
- Setting `IMM_GODOT_NATIVE_CAPTURE_PATH` writes an offscreen native IMM render PNG. This proves native IMM pixels are rendering, but those pixels are not yet presented into the Godot viewport.
- `get_document_state()` mirrors the Unity loading/playback state query and is included in smoke diagnostics.
- `get_bounding_box()` mirrors the Unity document bounds query and is included in smoke diagnostics for placement checks.
- `get_background_color()` mirrors the native player-info background color path and is included in smoke diagnostics.
- Spawn-area query/jump methods (`get_spawn_area_ids`, `get_spawn_area_info`, `set_active_spawn_area_index`) mirror the Unity sample surface and are included in smoke diagnostics.
- `matrix_debug_logging` records the last submitted camera matrices in render diagnostics for Unity/Godot parity checks.
- `set_document_transform(document_transform)` is smoke-tested against the native API path and exposed in render diagnostics as `document_to_world`.
- `set_camera_matrices(camera_id, world_to_head, projection)` lets smoke tests submit deterministic 16-float matrices before rendering.

## Open in Godot

1. Open `code/ImmGodotSampleProject` in Godot 4.x.
2. Use the Compatibility renderer path.
3. Run `scenes/SampleScene.tscn`.

The sample submits camera matrices every frame and calls `render_camera()` while a document is loaded. In script-stub mode this records the render attempt in diagnostics; with the native extension loaded, the status label's render result comes from the native IMM render call. The current native macOS path can render to an offscreen capture file, but the Godot viewport itself still shows the Godot scene/background until the texture presentation bridge is implemented.

## Sample controls

- `L`: load document
- `U`: unload document
- `Space`: pause/resume
- `R`: restart
- `]`: next chapter
- `[`: previous chapter
- `'`: next spawn area
- `;`: previous spawn area
- `W/A/S/D/Q/E`: move camera

## Document path

The default document path is set to `../../exampleImmFiles/sample1.imm`, which points at the repository sample from this project directory.

## Smoke test

From this project directory, run:

```powershell
godot --headless --path . --script res://scripts/smoke_test_runner.gd
```

On Windows, `code\projects\windows\run-godot-smoke.ps1` wraps the same check. Passing `-RequireExtension` also checks for the full staged DLL set and runs `NativeSmokeScene.tscn`, which fails if the GDExtension class is not loaded. Passing `-LogDir` records smoke output, DLL manifests, and `godot-matrix-diagnostics.json` for parity comparison.

## macOS native bridge compile check

From the repository root:

```bash
CC=/usr/bin/clang CXX=/usr/bin/clang++ cmake -S code/projects/macos -B build/macos-godot-appleclang -DIMM_BUILD_VIEWER=OFF
cmake --build build/macos-godot-appleclang --target ImmGodotPlugin -j 8
```

This verifies the native Godot C ABI bridge compiles and links against the IMM core/player on macOS.

To build the local macOS GDExtension after `thirdparty/godot-cpp` has been bootstrapped:

```bash
build/godot-tools/bin/scons -f code/appImmGodotGDExtension/SConstruct platform=macos configuration=release arch=arm64 -j 8
build/godot-tools/bin/scons -f code/appImmGodotGDExtension/SConstruct platform=macos configuration=debug arch=arm64 -j 8
```

Native macOS smoke with a viewport capture and offscreen IMM capture:

```bash
IMM_GODOT_REQUIRE_EXTENSION=1 \
IMM_GODOT_CAPTURE_PATH="$PWD/build/validation/godot-native-smoke.png" \
IMM_GODOT_NATIVE_CAPTURE_PATH="$PWD/build/validation/imm-native-offscreen.png" \
/Applications/Godot.app/Contents/MacOS/Godot \
  --path code/ImmGodotSampleProject \
  --script res://scripts/smoke_test_runner.gd
```

Expected current result: the smoke test passes and `imm-native-offscreen.png` contains native IMM pixels; `godot-native-smoke.png` is still the flat Godot background until Phase 2 presentation work is complete.

## Next integration step

Bridge the native offscreen render target into a Godot-visible texture, then keep the Windows GDExtension CI smoke/parity path green.
