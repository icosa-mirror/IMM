# IMM Godot Sample Project

This project mirrors the intent of the Unity sample project and exercises the native Godot integration.

## Current status

- The scene and controls are in place.
- The `ImmViewerNode` script mirrors the runtime API shape for API-parity checks, but the sample scene uses the native GDExtension node.
- The native `appImmGodot` plugin and GDExtension own native init/shutdown, camera registration, matrix submission, and compositor render request publishing.
- A `.gdextension` manifest is present under `addons/imm_viewer/`.
- The project Run button opens `SampleScene.tscn`, which is the user-facing native-addon setup: an `ImmViewerNode` loads `sample1.imm`, queues the authored camera, and composites through `ImmViewerCompositorEffect`.
- `VisualSmokeScene.tscn` is the deterministic visual validation harness. It defaults to Metal on macOS and Vulkan on Windows/Android when run by the smoke scripts.

## Open in Godot

1. Open `code/ImmGodotSampleProject` in Godot 4.5 or newer.
2. Use Forward+ rendering. On macOS, Godot 4.6.1 selects Metal for the Forward+ path.
3. Press Run. The project main scene is `scenes/SampleScene.tscn`, which loads `sample1.imm` through the native GDExtension and compositor. The editor Play path uses the active authored spawn area for initial camera placement when one is available, converted for Godot's `Camera3D` `-Z` forward convention, with document-bounds framing only as a fallback.

## Using the addon in a new project

The CI artifact (`ImmPlayerPlugin-Godot`) is a self-contained `addons/imm_viewer/` folder. Copy it into your Godot project's root so you have `res://addons/imm_viewer/`.

### macOS quarantine (required after downloading from CI)

macOS Gatekeeper quarantines files downloaded from the internet. The dylibs are linker-signed by Apple's toolchain; running `codesign` on them again breaks the signature. The correct fix is to remove the quarantine attribute recursively — **do not** codesign CI-built dylibs manually:

```bash
xattr -dr com.apple.quarantine addons/imm_viewer/
```

Run this once from your project root before opening the project in Godot. If you skip this step Godot will crash (SIGABRT) when it tries to load the extension.

### Scene setup

1. **Add an `ImmViewerNode`** to your scene tree. Set its properties in the Inspector:
   - `document_path`: path to your `.imm` file (e.g. `res://myfile.imm`)
   - `load_on_ready`: leave disabled when using the Vulkan compositor path; load after at least one camera/render warmup frame
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
                         load_on_ready: false
                         auto_play: true
                         auto_queue_render: true
```

For Vulkan editor playback, queue the active camera for a few frames before calling `load_document()`. The sample controller does this automatically so Godot has exposed the external Vulkan frame resources before IMM loads GPU content.

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

The default document path is set to `res://../../exampleImmFiles/sample1.imm`, which Godot globalizes to the repository sample from this project directory.

## Native build and validation

Build the GDExtension with `godot-cpp`, run the native smoke, and run the visual smoke scene when validating visible rendering. On Windows, `.\code\projects\windows\build-godot-extension.ps1 -Configuration Release -BootstrapGodotCpp -BuildGodotCpp` clones the default Godot 4.5-compatible `godot-cpp` bindings into `thirdparty\godot-cpp`, builds them, and builds the extension.

For Windows renderer-backend work, add `-RunSmoke -GodotExe C:\path\to\Godot_v4.5-stable_win64.exe` to the same command to run the native `SampleScene.tscn` smoke immediately after the build. For visible validation, run the `VisualSmokeScene.tscn` harness with `IMM_GODOT_VISUAL_SMOKE=1`; on Windows it selects Vulkan by default.

### Offline stereo simulation

Windows renderer work can exercise the native two-eye matrix/render path without OpenXR, SteamVR, or a headset:

```powershell
.\code\projects\windows\run-godot-stereo-simulation-smoke.ps1 `
    -GodotExe "C:\path\to\Godot_v4.6.1-stable_mono_win64.exe"
```

The helper explicitly launches Godot with `--xr-mode off`. `StereoSimulationSmokeScene.tscn` captures an upright mono reference, then supplies a synthetic 64 mm IPD through the same native stereo camera contract used by the OpenXR path and renders eye 0 and eye 1 separately. It writes `mono.png`, `left.png`, `right.png`, and `result.json` under `artifacts/godot-stereo-simulation/`.

The test fails when either eye is closer to a vertically flipped reference than the upright reference, when native/compositor diagnostics do not confirm the requested eye, or when the left and right captures are effectively identical. It validates matrix conversion, native per-eye selection, intermediate rendering, and Godot composition deterministically. It does not validate OpenXR swapchain acquisition, headset tracking, runtime-specific projection data, or headset presentation.

To capture one real OpenXR frame and replay its exact submitted stereo matrices without XR:

```powershell
.\code\projects\windows\capture-godot-openxr-frame.ps1 `
    -GodotExe "C:\path\to\Godot_v4.6.1-stable_mono_win64.exe"

.\code\projects\windows\run-godot-stereo-simulation-smoke.ps1 `
    -GodotExe "C:\path\to\Godot_v4.6.1-stable_mono_win64.exe" `
    -ReplayPath "artifacts\godot-xr-matrix-replay\xr-frame.json" `
    -OutputDirectory "artifacts\godot-xr-matrix-replay\replay"
```

The first command requests XR only until one valid stereo frame has been recorded, then exits automatically. The capture contains Godot's raw head/per-eye projections and eye offsets together with the six final matrices submitted to `ImmGodot_SetCameraMatrices`. The second command uses those final matrices while keeping XR disabled.

The native build scaffold writes the GDExtension DLL to `addons/imm_viewer/bin/windows/{debug,release}/`, which matches `addons/imm_viewer/imm_viewer.gdextension`, stages `ImmGodotPlugin.dll` plus the IMM runtime dependency DLLs beside it for Godot's extension loader, verifies the complete staged DLL set before reporting build success, and writes `godot-extension-dlls.txt` beside the DLLs for CI artifact diagnostics. On Windows, the GDExtension registers its own binary directory with the process DLL search path during library initialization so editor Play can resolve delayed IMM dependencies without a wrapper-modified `PATH`. The addon is self-contained, so distributing it is just a matter of copying the `addons/imm_viewer/` folder.

Android native addon packaging is built from `code/projects/android`:

```powershell
.\code\projects\android\build-godot-extension-android.ps1 -Configuration Debug -BuildGodotCpp
```

That helper builds/stages `libImmGodotPlugin.so` and `libimm_godot_extension.arm64.so` under `addons/imm_viewer/bin/android/debug/`, matching the Android entries in `imm_viewer.gdextension`. It requires the usual Android SDK/NDK `26.1.10909125` for IMM plus NDK `28.1.13356709` for Godot 4.5 `godot-cpp`.

If Godot is not installed, `python code/appImmGodotGDExtension/verify_local.py` checks the sample/native API boundary, `.gdextension` manifest paths, Forward+ project default, Run-button main scene, native sample scene structure, visual smoke scene structure, native `ImmViewerNode` registration and method bindings, `ImmViewerNode` camera registration plus camera/viewport render queue ownership, Windows `godot-cpp` bootstrap/CI/smoke wiring, source paths for the IMM runtime dependency DLLs staged by SCons, PowerShell helper syntax when PowerShell is available, `ImmGodot` C ABI export alignment, local Python files, and the `appImmGodot` syntax-only compile when `clang++` is available. If `GODOT_CPP_PATH` or `thirdparty/godot-cpp` points at a Godot 4.5 `godot-cpp` checkout with generated bindings, it also syntax-checks the GDExtension sources against the real Godot C++ headers. On Windows, `.\code\projects\windows\build-godot-extension.ps1 -VerifyOnly` runs the same local verification without requiring MSBuild, SCons, or `godot-cpp`.

With Godot installed and the extension DLLs built, `.\code\projects\windows\run-godot-smoke.ps1 -Configuration Release -RequireExtension` verifies the staged native DLL set and runs the headless smoke script against `NativeSmokeScene.tscn`. It validates the native `ImmViewerNode`, camera registration and render queue, document loading and state, playback controls, chapter/layer/spawn-area queries, signal parity, render diagnostics, and repeated load/unload behavior. Without `-RequireExtension`, the script uses `ScriptSmokeScene.tscn`. Add `-LoadUnloadCycles 2` for repeated lifecycle coverage and `-LogDir artifacts\godot-smoke` to retain logs and the staged-file inventory.

The `ImmViewer` node has `auto_queue_render = true` and `render_camera_path = ../CameraRig/Camera3D`. That makes `ImmViewerNode` register camera 0 and queue the active camera transform, field of view, and viewport dimensions each frame while a document is loaded. In the visual smoke scene, queued work is consumed by `ImmViewerCompositorEffect` and rendered into Godot-owned render resources. Press `\` to queue a fixed-viewport diagnostic render request.

The validation matrix builds and exercises the Windows Godot Vulkan path on a hosted runner using Mesa lavapipe, including full-depth diagnostics and the supported ordered-overlay composition. It also validates Android Godot Vulkan and macOS Godot Metal. The Windows Godot OpenXR lane remains gated to an explicitly enabled headset-capable runner. Local visible validation can run `VisualSmokeScene.tscn` directly or use `run-godot-vulkan-visual-baseline-smoke.ps1`.

The sample and smoke scenes apply the current IMM background color from `get_background_color()` to `RenderingServer.set_default_clear_color(...)` before camera rendering. The status label also displays that color for quick diagnostics.

The status label displays playback time from `get_play_time_seconds()`, and `,` / `.` call `seek_relative_seconds()` to exercise the native `piTick` time API.

The `-` and `=` keys call `set_volume()` and display `get_volume()`, exercising the same document-volume path used by the Unity wrapper.

The status label displays `get_current_chapter()` and `get_chapter_count()`, while `[`, `]`, and `set_chapter()` share the direct native chapter API.

The status label also reports `get_document_state()`, `is_sequence_ready()`, and document info flags so native loading/playback state can be compared against the Unity wrapper.

The status label reports `get_layer_count()` and `get_bounding_box()` when the native sequence is ready, matching the Unity wrapper's document inspection path.

The `V` key exercises the native layer override path by toggling the first layer through `set_layer_visible()` and displaying `get_layer_diagnostics()` status.

The viewer API also mirrors Unity's layer transform override calls with `set_layer_transform()` and `clear_layer_transform_override()`.

The viewer API now includes `set_document_transform()` and `get_document_transform()`. In the native GDExtension path this forwards to `ImmGodot_SetDocumentToWorld`; in the script stub it stores the value for API-parity checks.

The spawn-area controls now query `get_active_spawn_area_info()` and move the camera rig to the active authored pose, including the same head-offset compensation used by the Unity sample.
