# appImmGodotGDExtension

This folder contains the Godot 4 GDExtension-side integration for `ImmGodotPlugin`.

## Current state

- The source files define and register the native `ImmViewerNode` and `ImmViewerCompositorEffect` classes.
- The extension builds locally when pointed at a Godot 4.5-compatible `godot-cpp` checkout; the Windows helper can bootstrap that checkout into `thirdparty/godot-cpp`.
- The sample project includes a `.gdextension` manifest that points at the staged binary locations.
- `ImmViewerNode` now owns native backend init/shutdown and exposes a smoke API for mono camera matrix submission plus queued viewport render calls.
- `ImmViewerNode` registers the `ImmGodotRenderAdapter` callback table and records graphics/render callback diagnostics for the smoke harness.
- The visual smoke path renders IMM content into Godot-owned GPU resources through `ImmViewerCompositorEffect` and verifies visible pixels in a saved PNG. It defaults to Metal on macOS and Vulkan on Windows/Android.

## Expected dependency

Add or bootstrap `godot-cpp` and configure the build so these headers resolve:

- `godot_cpp/classes/*`
- `godot_cpp/core/*`
- `godot_cpp/variant/*`

## Windows build scaffold

The included `SConstruct` builds the GDExtension DLL expected by the sample manifest:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -Configuration Debug -GodotCppPath C:\path\to\godot-cpp
.\code\projects\windows\build-godot-extension.ps1 -Configuration Release -GodotCppPath C:\path\to\godot-cpp
```

If `godot-cpp` is not already checked out, the helper can clone the default Godot 4.5-compatible bindings ref and build it before the extension:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -Configuration Release -BootstrapGodotCpp -BuildGodotCpp
```

When `-BuildGodotCpp` is passed, the helper reuses an existing `godot-cpp` library and generated bindings when they are already present. It rebuilds `godot-cpp` only when the library or generated headers are missing.

Set `-GodotCppRef <tag-or-branch>` or `GODOT_CPP_REF` to override the default `godot-4.5-stable` ref.

Use `-PreflightOnly` to resolve Python, MSBuild, SCons, `godot-cpp`, and expected library paths without compiling or cloning dependencies:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -Configuration Release -BootstrapGodotCpp -BuildGodotCpp -PreflightOnly
```

Use `-RunSmoke` to run the native Godot smoke test immediately after the build:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -Configuration Release -BootstrapGodotCpp -BuildGodotCpp -RunSmoke -GodotExe C:\path\to\Godot_v4.5-stable_win64.exe
```

You can still run SCons directly from this directory if `appImmGodot` and `godot-cpp` are already built:

```powershell
cd code\appImmGodotGDExtension
scons platform=windows target=template_debug arch=x86_64 imm_config=Debug godot_cpp=C:\path\to\godot-cpp
```

Prerequisites:

- Build `godot-cpp` first so `libgodot-cpp.windows.*.x86_64.lib` exists under its `bin` folder, or pass `godot_cpp_lib=...`.
- Build `appImmGodot` first so `code/appImmGodot/exe/ImmGodotPlugin.lib` exists.
- The output DLL is written to `code/ImmGodotSampleProject/addons/imm_viewer/bin/windows/{debug,release}/imm_godot_extension.dll`, matching `addons/imm_viewer/imm_viewer.gdextension`. The addon is self-contained: every binary lives under `addons/imm_viewer/bin/`.
- `ImmGodotPlugin.dll` and the IMM runtime dependency DLLs are copied beside the GDExtension DLL. On Windows, the GDExtension registers its own binary directory with the process DLL search path at library initialization so editor Play can resolve delayed IMM dependencies without a wrapper-modified `PATH`.
- The Windows build helper verifies the complete staged DLL set after SCons before reporting success or running `-RunSmoke`, then writes `godot-extension-dlls.txt` beside the DLLs for artifact diagnostics.

## Intended runtime shape

- `ImmViewerNode` owns native IMM session lifecycle.
- It exposes lifecycle signals (`native_backend_initialized`, `native_backend_failed`) plus document/playback/spawn-area signals with matching script-stub definitions.
- It exposes load/unload/playback/volume/spawn-area APIs to Godot scripts, including `toggle_pause()` for parity with the sample script stub.
- It exposes `get_chapter_count()`, `get_current_chapter()`, and `set_chapter()` in addition to next/previous chapter shortcuts.
- It exposes `set_time()`, `get_time()`, `get_play_time()`, and `seek_relative_seconds()` for Unity-style timeline control over native `piTick` playback time.
- It exposes `get_document_state()`, `get_document_info_flags()`, and `is_sequence_ready()` for Unity-style document state/status checks.
- It exposes `get_bounding_box()`, `get_layer_count()`, and `get_layer_info()` for Unity-style document/layer inspection once the sequence is ready.
- It exposes `set_layer_visible()`, `clear_layer_visibility_override()`, `set_layer_opacity()`, and `get_layer_diagnostics()` for the same runtime layer visibility/opacity override path used by the Unity wrapper.
- It exposes `set_layer_transform()` and `clear_layer_transform_override()` using the same matrix conversion path as document transforms.
- It exposes `get_background_color()` through a C-safe player-info ABI for sample background parity checks.
- It exposes `set_document_transform()` / `get_document_transform()` and forwards loaded documents through the native `ImmGodot_SetDocumentToWorld` path, including the existing IMM handedness adjustment.
- It exposes `get_spawn_area_info()` / `get_active_spawn_area_info()` dictionaries with converted Godot pose basis vectors for camera-rig jump tests.
- Spawn-area metadata is copied through caller-owned ABI structs; names use fixed-size buffers so GDExtension callers do not own native allocations.
- It is the handoff point for render-thread camera capture and draw callbacks in Phase 2.
- The native backend now exposes `ImmGodot_InitEx(..., rendererApi)` plus `ImmGodot_BeginMetalFrame` / `ImmGodot_EndMetalFrame`, so the production macOS path can attach IMM's existing Metal renderer to Godot-owned Metal render resources instead of relying on the OpenGL smoke path.
- `ImmViewerCompositorEffect` is the production render-pipeline entry point. Its render callback records `RenderSceneBuffersRD`, color texture RID, target/internal size, view count, and native command-queue/color-texture handles through `RenderingDevice.get_driver_resource`. It renders IMM content into a Godot-created intermediate texture and composites that texture back into Godot's scene color through `RenderingDevice`, keeping final presentation inside Godot's render graph. To activate it, assign an instance to the **Camera3D's `compositor` property**: create a `Compositor` resource, add `ImmViewerCompositorEffect` to its `compositor_effects` array, and assign the `Compositor` to the camera. The reference scene also assigns the same compositor to a `WorldEnvironment` node so the path remains explicit when opened in the editor. In GDScript: `var effect = ImmViewerCompositorEffect.new(); var c = Compositor.new(); c.compositor_effects = [effect]; $Camera3D.compositor = c`. The `VisualSmokeScene` script (`visual_smoke_controller.gd`) shows the reference implementation of `_setup_compositor()`.

## Smoke API

The native class exposes these test and diagnostic hooks around the production compositor path:

- `submit_mono_camera_matrices(camera_id, world_to_camera, projection)` expects two 16-float arrays in Godot/host matrix order.
- `smoke_render_camera(camera_id, width, height)` invokes the native mono render path for viewport smoke testing.
- `set_camera_transform(camera_transform)` generates a temporary mono camera matrix/projection feed for sample-scene smoke testing.
- `queue_render_camera_transform(camera_transform, width, height, fov_degrees, camera_id)` captures camera matrices and publishes a compositor render request using the active viewport dimensions.
- `register_render_camera(camera_id)` / `unregister_render_camera(camera_id)` define which camera IDs may submit queued render work.
- `set_document_transform(document_transform)` stores a Godot document transform and applies it after load when the native backend is active. `get_document_transform()` exposes the stored transform for script and smoke parity checks.
- `smoke_render_last_camera()` invokes the smoke render hook for the last submitted camera.
- `queue_render_last_camera()` publishes a diagnostic request for the last submitted camera through the same compositor queue used by the production render path.
- The compositor callback copies camera id and viewport size from a mutex-protected request snapshot before calling into `ImmGodot_RenderCamera`. It keeps the latest render request active for continuous editor playback and reuses the Godot-created intermediate texture until its size or format changes, avoiding black-frame flicker from render callbacks that arrive between `_process` queues or after a just-recorded composite draw.
- `auto_queue_render` plus `render_camera_path` lets `ImmViewerNode` own per-frame camera lookup, viewport-size capture, render-camera registration, matrix submission, and compositor request publishing.
- `debug_logging` or `IMM_GODOT_DEBUG=1` enables native log messages for init, matrix submission, and render calls.
- `ImmGodotRenderAdapter` brackets graphics init/shutdown and render-camera calls so the real Godot render integration has a stable handoff point.
- `get_render_diagnostics()` reports the last queued camera/viewport, queued callback state, registered camera IDs, and render-adapter callback counts used by the smoke test.
- `get_render_backend_diagnostics()` reports the configured and actual Godot rendering method/driver, RenderingDevice availability, selected IMM renderer API, whether the current runtime state is a macOS Metal adapter candidate, and whether the current runtime state is a Vulkan adapter candidate for the Windows/Android path. `Auto` resolves to Metal on macOS and Vulkan on Windows/Android.
- `ImmViewerCompositorEffect.get_diagnostics()` reports whether Godot called the compositor render callback, whether RD scene buffers, color texture RID, command queue handle, and color texture handle were available, and whether the macOS Metal frame/render attempt started.

OpenGL Compatibility remains a bootstrap/smoke path. Visible production rendering is the macOS Metal compositor path: `ImmViewerNode` publishes queued camera work, `ImmViewerCompositorEffect` consumes it on the render callback, IMM renders into Godot-owned GPU resources, and Godot performs the final composite into scene color.

## Local validation

When Godot is not installed, run the local verification wrapper:

```powershell
python code\appImmGodotGDExtension\verify_local.py
```

The Windows build helper also has a verification-only mode:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -VerifyOnly
```

This verifies that methods, properties, and signals used by `sample_scene_controller.gd` are present on both the script stub and native `ImmViewerNode` bindings, checks the `.gdextension` entry point and DLL paths, checks that the sample defaults to Forward+ and launches `SampleScene.tscn` from the Run button, checks native registration of `ImmViewerNode`, checks that public script-facing `ImmViewerNode` methods are bound through `ClassDB`, checks native sample scene structure, checks visual smoke scene structure, checks `ImmViewerNode` camera registration and camera/viewport render queue ownership, checks Windows `godot-cpp` bootstrap/CI/smoke wiring, checks the source paths for the IMM runtime dependency DLLs staged by SCons, validates Windows PowerShell helper syntax when `pwsh` or Windows PowerShell is available, checks `ImmGodot` C ABI declaration/definition alignment, checks local Python files, and runs the `appImmGodot` syntax-only compile when `clang++` is available.

If `GODOT_CPP_PATH` points at a `godot-cpp` checkout with generated bindings, or `thirdparty/godot-cpp` exists, the verifier also runs a syntax-only compile of `imm_viewer_node.cpp` and `register_types.cpp` against the actual Godot C++ headers. Generate the bindings first with `scons platform=windows target=template_release arch=x86_64 generate_bindings=yes` or let `build-godot-extension.ps1 -BootstrapGodotCpp -BuildGodotCpp` do it. This does not replace a real Godot project parse or full GDExtension link.

To also run the native Godot project smoke locally, build the extension, set `IMM_GODOT_RUN_LOCAL_SMOKE=1`, and either put Godot on PATH or set `GODOT_EXE`:

```powershell
$env:IMM_GODOT_RUN_LOCAL_SMOKE = "1"
$env:GODOT_EXE = "C:\path\to\Godot_v4.5-stable_win64.exe"
python code\appImmGodotGDExtension\verify_local.py
```

On macOS, `verify_local.py` also checks `/Applications/Godot.app/Contents/MacOS/Godot` and `/Applications/Godot_mono.app/Contents/MacOS/Godot` when the smoke flag is set.

The SCons build stages `imm_godot_extension.dll`, `ImmGodotPlugin.dll`, and the IMM runtime dependency DLLs in `addons/imm_viewer/bin/windows/{debug,release}`. The Windows GDExtension registers that directory with `AddDllDirectory`/`SetDllDirectoryW` during library initialization so delayed dependencies such as `jpeg62.dll` and `libpng16.dll` resolve in editor Play and wrapper-launched runs.

## Android build scaffold

The Android helper builds the native `ImmGodotPlugin` library and the Android GDExtension library, then stages both files in the paths already declared by `addons/imm_viewer/imm_viewer.gdextension`:

```powershell
.\code\projects\android\build-godot-extension-android.ps1 -Configuration Debug -BuildGodotCpp
```

Outputs:

- `code/ImmGodotSampleProject/addons/imm_viewer/bin/android/debug/libImmGodotPlugin.so`
- `code/ImmGodotSampleProject/addons/imm_viewer/bin/android/debug/libimm_godot_extension.arm64.so`

Prerequisites:

- Android SDK with NDK `26.1.10909125` for the existing IMM Gradle/CMake modules.
- Android SDK command-line tools plus NDK `28.1.13356709` for Godot 4.5 `godot-cpp` Android builds.
- Python and SCons.
- A Godot 4.5-compatible `godot-cpp` checkout. Pass `-BuildGodotCpp` to build the Android `libgodot-cpp.android.*.arm64.a` library when it is missing.

This is Android packaging coverage for the Godot addon binaries. Runtime verification still requires exporting/running a Godot Android project with the addon enabled on a Vulkan-capable Android device or emulator.

After building the extension and installing Godot 4.5 or newer, run the sample smoke test:

```powershell
.\code\projects\windows\run-godot-smoke.ps1 -Configuration Release -GodotExe C:\path\to\Godot_v4.5-stable_win64.exe -RequireExtension
```

The smoke test runs `res://scripts/smoke_test_runner.gd` headlessly, checks the Forward+ project default, instantiates `res://scenes/SampleScene.tscn`, verifies native-class loading, asserts that `auto_queue_render` auto-registers camera 0, calls `load_document()` if the scene has not already loaded the document, requires `is_loaded()`, checks document state/background color, applies that color to Godot's default clear color, exercises chapter/bounds/layer/spawn-area query APIs, exercises layer visibility/opacity/transform overrides when authored layers exist, exercises volume and pause/play/toggle/restart controls, exercises the registered camera/viewport render queue, and validates render-adapter graphics/before/after callback diagnostics. The wrapper requires both a zero Godot exit code and the `IMM Godot smoke test passed` marker in output. With `-RequireExtension`, the wrapper first verifies that `imm_godot_extension.dll`, `ImmGodotPlugin.dll`, and the staged IMM runtime dependency DLLs exist, then asserts the `ImmViewer` node is the native `ImmViewerNode` class rather than the script stub. Passing `-LoadUnloadCycles N` additionally unloads and reloads the document `N` times while the camera/render queue remains active, then verifies the final render diagnostics.

On macOS, the local debug path is:

```bash
/tmp/imm-godot-scons-venv/bin/python -m SCons platform=macos target=template_debug arch=arm64 godot_cpp=/tmp/godot-cpp-4.5-check imm_config=Debug
IMM_GODOT_EXPECT_NATIVE=1 IMM_GODOT_SMOKE_SCENE=res://scenes/SampleScene.tscn /Applications/Godot.app/Contents/MacOS/Godot --headless --path code/ImmGodotSampleProject --script res://scripts/smoke_test_runner.gd
```

That path currently builds and loads the native extension, instantiates `ImmViewerNode`, loads the sample IMM document, and passes the native Compatibility/headless smoke on Godot 4.6.1. The smoke validates core signals and keeps native timeline setters/relative seeks safe before the IMM timeline-ready state is reached. It does not exercise visible Metal presentation; use the Forward+/Metal visual harness below for that.

For the macOS Forward+/Metal visual harness, run:

```bash
IMM_GODOT_VISUAL_SMOKE=1 IMM_GODOT_VISUAL_SMOKE_PNG=/tmp/imm-godot-metal-visual-smoke.png /Applications/Godot.app/Contents/MacOS/Godot --path code/ImmGodotSampleProject --rendering-driver metal --rendering-method forward_plus --scene res://scenes/VisualSmokeScene.tscn --fixed-fps 30
```

This path explicitly loads the GDExtension, creates a native `ImmViewerNode`, attaches `ImmViewerCompositorEffect` to the active camera, loads `sample1.imm`, queues camera renders, saves a PNG, and requires valid Godot-owned Metal command queue/color texture handles plus a successful `ImmGodot_RenderCamera` result. The current local visual smoke passes on Godot 4.6.1, verifies non-background content pixels in the saved PNG, checks the saved image orientation, and composites the Metal intermediate into Godot scene color without an extra vertical texture-coordinate flip.

Add `-LogDir artifacts\godot-smoke` to save `godot-smoke-output.log` and `godot-smoke-summary.txt`. On Windows, pass `-RendererApi 5` to `run-godot-smoke.ps1` or `-SmokeRendererApi 5` to `build-godot-extension.ps1 -RunSmoke` to request Vulkan diagnostics from the smoke scene. For native smoke and preflight runs, the wrapper also writes `godot-extension-dlls.txt` with the expected staged DLLs, found/missing status, byte size, and UTC timestamp. The Windows GitHub Actions job caches `thirdparty/godot-cpp` by `GODOT_CPP_REF`, builds the GDExtension, and runs the staging preflight. The headed Windows Godot Vulkan playback smoke and standalone Vulkan viewer smoke run in CI GPU Matrix on GitHub-hosted Windows runners. macOS CI builds the Metal standalone viewer, runs its playback smoke with resolution-independent validation, builds the macOS Godot GDExtension, and wires the Metal visual smoke scene behind the same CI profile. The combined Godot addon package now waits for Windows, Android, and macOS platform binaries before assembling `ImmPlayerPlugin-Godot`.

Use `-PreflightOnly` on `run-godot-smoke.ps1` to check Godot executable resolution, extension DLL path selection, and scene selection without launching the project.
