# Unity Viewer Rendering Architecture and Godot Port Plan

## 1) How the current Unity Viewer plugin renders into a Unity scene

### Runtime split
- **Managed Unity side (`ImmViewer` + sample `UseRenderingPlugin`)** drives camera data, lifecycle, and user interactions.
- **Native plugin side (`appImmUnity/src/main.cpp`)** owns renderer/player initialization and actual draw calls.

### Unity-side frame flow
1. `ImmViewer.InitializeSingleton()` creates a persistent singleton and calls native `Init(...)` once.
2. On each frame, `LateUpdate()` calls native `GlobalWork(1)` to advance async/global player work.
3. For each camera (`Camera.onPreCull`):
   - Detect stereo mode (`mono`, `multipass`, `single-pass/instanced`).
   - Ensure a per-camera `CommandBuffer` exists and issues `IssuePluginEvent(GetRenderEventFunc(), cameraId<<8)` at `AfterImageEffectsOpaque`.
   - Capture head/left/right view and projection matrices and pass them to native `SetMatrices(...)`.
   - Pull `GetPlayerInfo(...)` and set camera clear color from document background.

### Native plugin frame flow
1. Unity loads the plugin and registers `iOnGraphicsDeviceEvent`.
2. On graphics init, plugin captures Unity graphics device/backend (`D3D11`, `D3D12`, or GL/GLES).
3. Native `Init(...)` creates:
   - logger + timer
   - sound backend
   - `piRenderer` (DX or GL/GLES)
   - `ImmPlayer::Player` with backend-dependent depth/clip/front-face configuration
4. When Unity executes `IssuePluginEvent`, `iOnRenderEvent(event_id)`:
   - decodes `cameraID`
   - reads camera matrices previously stored by `SetMatrices(...)`
   - calls `Player::GlobalRender(...)`
   - dispatches one of:
     - `RenderMono(...)`
     - `RenderStereoMultiPass(...)`
     - `RenderStereoSinglePass(...)`
   - includes a single-pass Unity viewport workaround (expand viewport to 2x width then restore)
   - ticks sound backend

### Data / transform behaviors to preserve
- Document world transform applies `flipZ()` when crossing Unity ↔ IMM (`SetDocumentToWorld`).
- Sample script negates/reshuffles axes for spawn-area position/quaternion when applying viewpoint transforms.
- Playback/document/spawn-area APIs are exposed through a broad C ABI (load/unload, pause/resume, timeline, volume, metadata, spawn areas, etc.).

---

## 2) Godot port strategy (high-level)

### Guiding principle
Keep **existing native IMM core/player logic** mostly intact and replace the **engine integration layer**:
- Unity C# + Unity plugin event wiring ➜ Godot GDExtension module + Godot render callbacks.
- Reuse as much of the existing C API (or equivalent wrapper) as possible.

### Recommended target
- **Godot 4.x + GDExtension (C++)**.
- Use **OpenGL Compatibility only as a bootstrap/smoke path** for early Godot lifecycle, document-load, matrix, and render-thread invocation validation.
- Use the existing IMM **Metal** renderer as the preferred first production visible-rendering target on macOS via Godot Forward+/Mobile renderer integration. Do not continue treating OpenGL Compatibility as the production presentation path unless a clean Godot-owned OpenGL render-target hook is proven.
- Treat Metal as the first production path, not the final portability answer. Windows/Linux/Android production parity will eventually require an IMM **Vulkan and/or Direct3D 11/12** backend/integration path compatible with Godot's non-OpenGL renderers.

---

## 3) Port plan by phases

## Phase 0 — Discovery and compatibility matrix

### Phase 0 status (completed)
- **Status:** Completed
- **Owner:** Engine integration
- **Outcome:** first implementation target and constraints are now fixed, with explicit technical contracts for Phase 1.

### Discovery findings (from current codebase)

| Area | Finding | Evidence/impact |
|---|---|---|
| Renderer APIs in IMM | `piRenderer::API` supports **GL, DX, GLES, and Metal**. | Godot 4 Forward+/Mobile renderer integration on macOS can target an existing IMM Metal backend instead of requiring a brand-new Vulkan backend for the first production path. |
| Unity integration backend choices | Unity plugin chooses **DX** when a device is supplied, **Metal** when Unity exposes Metal interfaces on Apple platforms, otherwise **GL** (and GLES on Android). | Existing IMM integration patterns already include host-supplied native graphics context/resource handoff for Metal; Godot should reuse that shape. |
| Player render-path assumptions | `libImmPlayer` has API-conditional behavior for DX/Metal vs GL/GLES (clip-space/depth/front-face/stereo handling). | Godot bridge must preserve these assumptions and avoid introducing new math conventions during bring-up. |
| Build/tooling footprint | Repository currently contains a Windows-first solution under `code/projects/windows/imm.sln`. | Windows remains the lowest-risk build/staging target for CI, but visible production rendering is first validated on macOS Metal until Windows has a Godot-compatible Vulkan/D3D path. |
| Standalone Metal evidence | The repo contains `appImmViewerMetal`, `piMetal_Renderer.mm`, and macOS Metal validation targets. | Metal is not speculative; it is the preferred route to a Godot-owned production render target on macOS. |
| Cross-platform production gap | Godot Forward+/Mobile use RenderingDevice-backed drivers on non-OpenGL renderers. | After macOS Metal parity, Windows/Linux/Android parity will need Vulkan and/or Direct3D 11/12 work rather than depending on the OpenGL bootstrap path. |

### Final Phase 0 decisions
1. **Bootstrap platform:** Windows desktop remains useful for CI, GDExtension build/staging validation, script-stub smoke coverage, and native API/preflight checks, but it is not a production render-smoke gate until a Godot-compatible Windows renderer backend exists.
2. **Bootstrap renderer path:** OpenGL Compatibility remains a smoke path only.
3. **Production visible-rendering path:** macOS Metal through Godot Forward+/Mobile is now the preferred first target, using the existing IMM Metal backend and Godot-owned render resources.
4. **Cross-platform renderer roadmap:** after Metal proves the Godot-owned render-target integration model, add Vulkan and/or Direct3D 11/12 support for Windows/Linux/Android production parity.
5. **Initial feature scope:** Mono rendering parity first; XR/stereo deferred.
6. **Parity baseline for milestone 1:**
   - Load/render one IMM document
   - Background color sync
   - Document transform parity (including handedness)
   - Spawn-area query/jump
   - Basic playback controls (pause/resume/restart/skip)

### Phase 0 deliverables

#### A) Render hook and threading design note (agreed)
- `ImmViewerNode` (GDExtension) owns scene-facing API and camera registration.
- `ImmRenderBridge` owns native IMM state and is the only layer allowed to call `piRenderer`/`Player::Render*`.
- `global_work()` is called from the frame/update side; visible `render_camera()` calls are consumed by the compositor render callback only.
- No mutable renderer state is written from non-render thread; camera matrices are double-buffered and swapped at frame boundaries.

#### B) Backend smoke-path definition (validated at planning level)
- The first smoke path uses **OpenGL compatibility renderer** in Godot 4 (Compatibility mode).
- Smoke success criterion for Phase 1 implementation:
  1. initialize IMM backend,
  2. execute render callback with valid viewport,
  3. call `GlobalRender` + `RenderMono` with test camera matrices,
  4. teardown cleanly.
- Any Vulkan integration work is explicitly out of scope until parity milestone is reached.

#### C) Matrix conversion spec (Godot ➜ IMM)
- Source camera: Godot `Transform3D` / `Projection`.
- Conversion policy:
  - preserve column/row ordering contract through explicit conversion helpers,
  - normalize handedness so document placement matches Unity parity behavior,
  - normalize depth-range conventions before passing projection matrices,
  - apply document transform parity rule equivalent to Unity-side `flipZ` path.
- Validation signals:
  - spawn-area jump lands camera rig at expected position/orientation,
  - background and bounding placement visually match Unity sample scenes.

#### D) First-frame checklist (agreed)
1. `init(config)` succeeds (log/timer/sound/renderer/player).
2. Camera matrices are captured and submitted for camera 0.
3. Render callback executes once and submits mono draw.
4. No thread/context assertions fail.
5. `shutdown()` deinitializes without leaks/crash.

### Phase 0 exit criteria
- [x] A short design note exists for the Godot rendering hook API and thread ownership model.
- [x] Selected backend path is validated with a minimal smoke-path definition.
- [x] Matrix conversion spec (Godot ➜ IMM) is documented with depth-range/handedness rules.
- [x] A tiny “first frame” checklist is agreed (init, set matrices, render, teardown).

### Phase 1 handoff (next actions)
1. Create `appImmGodot` skeleton with Windows build target.
2. Add `ImmEngineBridge` (shared lifecycle/player wrapper) and isolate Unity-specific code.
3. Implement Compatibility renderer callback path and smoke run.
4. Add matrix debug logging toggles to compare Unity and Godot camera feeds.

## Phase 1 — Create a Godot-native bridge layer
### Phase 1 status (in progress)
- **Status:** In progress
- **Completed so far:**
  - Added shared native `ImmEngineBridge` ownership layer for renderer/log/timer/sound/player lifecycle and camera render dispatch.
  - Switched Unity plugin lifecycle/render entrypoints to forward through the shared bridge instead of owning IMM runtime state directly.
  - Added Windows `appImmGodot` native plugin skeleton that initializes IMM without Unity headers.
  - Added a Godot sample project scaffold mirroring the Unity sample scene structure and control surface.
  - Added a GDExtension source scaffold and `.gdextension` manifest so the sample project has a defined native integration target.
  - Added native Godot backend lifecycle wiring in `ImmViewerNode` (`ImmGodot_Init` on ready, `ImmGodot_Shutdown` on exit) with configurable color space, antialiasing, log path, temp path, and debug logging.
  - Added Godot-native debug hooks (`IMM_GODOT_DEBUG` / `debug_logging`) for matrix submission and render-camera smoke tracing.
  - Added explicit GDExtension smoke-harness methods: `submit_mono_camera_matrices(camera_id, world_to_camera, projection)` and `smoke_render_camera(camera_id, width, height)`.
  - Added a versioned `ImmGodotRenderAdapter` callback table around graphics init/shutdown and render-camera begin/end so Godot render-thread/context hooks can be attached without changing the engine-agnostic bridge.
  - Added `ImmGodot_InitEx(..., rendererApi)` plus a Godot-facing `renderer_api` property so the native bridge can select Auto/OpenGL/Direct3D/GLES/Metal instead of hardcoding the OpenGL bootstrap path.
  - Added a C ABI Metal frame seam (`ImmGodotMetalFrame`, `ImmGodot_BeginMetalFrame`, `ImmGodot_EndMetalFrame`) that can wrap Godot-owned Metal command buffers/encoders/render-pass descriptors around existing `ImmGodot_RenderCamera` calls without CPU readback/upload.
  - Added a Windows SCons build scaffold for `appImmGodotGDExtension` that links against a prebuilt `godot-cpp` library and `ImmGodotPlugin.lib`, outputting the DLL path expected by the sample `.gdextension` manifest.
  - Extended the GDExtension SCons build to stage `ImmGodotPlugin.dll` and the IMM runtime dependency DLLs beside `imm_godot_extension.dll`, matching Godot's extension-load directory expectations.
  - Added Windows build helpers (`build-godot-extension.ps1` / `.bat`) that rebuild `appImmGodot`, optionally build `godot-cpp`, run the GDExtension SCons build, and copy runtime binaries into the sample project.
  - Added a sample-scene smoke driver: `queue_render_camera_transform(camera_transform, width, height, fov_degrees, camera_id)` now submits mono camera matrices from the active camera and queues render using the active viewport dimensions.
  - Added a render-camera ID lifecycle (`register_render_camera`, `unregister_render_camera`, `is_render_camera_registered`, `get_registered_render_camera_ids`) so `ImmViewerNode` can own camera 0 registration before queuing per-frame render work.
  - Moved per-frame camera scheduling into `ImmViewerNode` behind `auto_queue_render` and `render_camera_path`, so the node owns camera registration, viewport-size capture, matrix submission, and compositor render request publishing instead of the sample controller.
  - Kept `\` as a manual fixed-viewport diagnostic render request through `queue_render_last_camera()`.
  - Replaced the early `RenderingServer.call_on_render_thread` smoke render queue with the `ImmViewerCompositorEffect` production path. `ImmViewerNode` now submits camera matrices and publishes compositor render requests; visible rendering is performed by the compositor into Godot-owned render resources. `smoke_render_camera` / `smoke_render_last_camera` remain explicit diagnostics only.
  - Added a mutex-protected render request snapshot for native queued render data so scene-thread camera/viewport submissions are not read directly by compositor/diagnostic render paths.
  - Added a Godot-facing `get_render_diagnostics()` snapshot so script/native smoke tests can assert the queued camera id, viewport dimensions, registered camera set, projection submission, and adapter callback state without reaching into private render state.
  - Replaced the initial no-op Godot render adapter callbacks with counted adapter callbacks, and extended smoke diagnostics to verify graphics initialization plus before/after render callback bracketing around queued camera work.
  - Added `ImmViewerNode.get_render_backend_diagnostics()` in both the native node and script stub so smoke tests can report the active Godot rendering method, rendering-device driver setting, RenderingDevice availability, selected IMM renderer API, and whether the current project state is a candidate for the macOS Metal adapter. The Compatibility smoke path must report that it is not a Metal adapter candidate.
  - Retargeted the Godot GDExtension build/smoke baseline from Godot 4.2 to **Godot 4.5** because `godot-4.2-stable` exposes only Vulkan-specific `RenderingDevice.DriverResource` handles, while Godot 4.5 exposes the generic driver resources and Metal rendering-driver enum needed for the macOS Metal adapter path.
  - Added `ImmViewerCompositorEffect`, a registered native `CompositorEffect` render-pipeline entry point. Its `_render_callback` extracts `RenderSceneBuffersRD`, the active color texture RID, target/internal sizes, view count, and native driver handles for the command queue and color texture via `RenderingDevice.get_driver_resource`.
  - Added the first macOS compositor render attempt path: queued camera work is published from `ImmViewerNode` to `ImmViewerCompositorEffect`; when the compositor receives Godot-owned command queue and color texture handles, it builds an `MTLRenderPassDescriptor`, calls `ImmGodot_BeginMetalFrame`, invokes `ImmGodot_RenderCamera`, and closes with `ImmGodot_EndMetalFrame`.
  - Added `MetalVisualSmokeScene.tscn` plus `metal_visual_smoke_controller.gd`, a Forward+/Metal visual harness that explicitly loads the GDExtension, creates the native `ImmViewerNode`, attaches `ImmViewerCompositorEffect` to the active camera through a Godot `Compositor`, saves a PNG, and fails unless the compositor sees Godot-owned Metal command queue/color texture handles and `ImmGodot_RenderCamera` succeeds.
  - Built the macOS debug GDExtension locally against `godot-4.5-stable` `godot-cpp` and staged it into the sample project. Fixed SCons macOS debug/release flags, macOS `godot-cpp` hot-reload defines, and full-path library ordering so the extension links with `ImmGodotPlugin`.
  - Added the sample project's `[native_extensions]` registration and explicit smoke-runner `GDExtensionManager.load_extension(...)` call so headless `--script` native smoke tests load `ImmViewerNode` deterministically before instantiating `NativeSmokeScene.tscn`.
  - Hardened Godot-facing playback/time/chapter/bounds/spawn-area queries so early smoke callbacks return stable defaults until the native sequence is ready instead of crashing or returning invalid sequence data.
  - Added `build-godot-extension.ps1 -BootstrapGodotCpp` so Windows local/CI builds can clone the default Godot 4.5-compatible `godot-cpp` bindings into `thirdparty/godot-cpp` instead of requiring a manual checkout first.
  - Added Windows preflight modes for the Godot extension build and smoke wrapper so tool/dependency/Godot executable resolution can be checked before starting the full native build or launching the project.
  - Added `build-godot-extension.ps1 -RunSmoke` to run the native Godot smoke scene immediately after a successful Windows GDExtension build.
  - Extended the Windows GitHub Actions workflow to install SCons, cache the Godot 4.5 `godot-cpp` checkout/build tree, bootstrap/build `godot-cpp` on cache misses, run the Godot GDExtension build helper, and upload the staged GDExtension DLL set.
  - Updated `build-godot-extension.ps1 -BuildGodotCpp` to reuse a cached `godot-cpp` library plus generated bindings when present, while still rebuilding the bindings on cache misses or incomplete cache restores.
  - Hardened `run-godot-smoke.ps1 -RequireExtension` so native smoke preflight requires the GDExtension DLL, `ImmGodotPlugin.dll`, and all staged IMM runtime dependency DLLs before launching Godot.
  - Hardened the Windows Godot smoke wrapper to require the `IMM Godot smoke test passed` marker in Godot output in addition to a zero process exit code, and to record the marker result in smoke summaries.
  - Added `godot-extension-dlls.txt` smoke-log output for native smoke runs, recording expected staged DLLs with found/missing status, byte size, and UTC timestamp before Godot launches.
  - Hardened `build-godot-extension.ps1` with a post-SCons staged-output check that requires the GDExtension DLL, `ImmGodotPlugin.dll`, and all runtime dependency DLLs before reporting updated sample binaries or running optional smoke.
  - Added a staged-output `godot-extension-dlls.txt` manifest beside the GDExtension DLLs and included it in the Windows `ImmGodotGDExtension-Windows` artifact for CI diagnostics.
  - Added local verification that the source IMM runtime dependency DLLs referenced by the Godot SCons staging step exist in the repository's third-party layout before the Windows build attempts to copy them.
  - Added a headless Godot smoke runner (`smoke_test_runner.gd`) and Windows wrapper (`run-godot-smoke.ps1`) that instantiate the sample scene, verify the Compatibility renderer setting, verify `ImmViewerNode` auto-registers camera 0 through `auto_queue_render`, load the sample IMM document, require `is_loaded()`, check document state/background color, apply that color to Godot's default clear color, exercise chapter/bounds/layer/spawn-area query APIs, exercise volume and playback controls, exercise the camera/viewport render queue, and validate the render diagnostics snapshot. The macOS native debug smoke now passes locally with the staged GDExtension and installed Godot 4.6.1.
  - Added opt-in repeated load/unload smoke coverage through `IMM_GODOT_LOAD_UNLOAD_CYCLES` / `run-godot-smoke.ps1 -LoadUnloadCycles`, which unloads and reloads the sample document while the camera/render queue remains active, then verifies final render diagnostics.
  - Added Godot smoke log capture through `run-godot-smoke.ps1 -LogDir`, and wired the Windows workflow to upload `ImmGodotSmokeLogs-Windows` so first real project-load/render failures preserve output and run metadata.
  - Split the Windows workflow gates into script-stub smoke before the native build and native-extension staging preflight after the GDExtension build, separating Godot project/GDScript regressions from native packaging failures while deferring Windows native render smoke until a valid Godot-compatible backend exists.
  - Split smoke validation into a script-stub scene (`SampleScene.tscn`) and native-only scene (`NativeSmokeScene.tscn`), and removed the script stub's `class_name ImmViewerNode` so it does not shadow the GDExtension class during native validation.
  - Added Debug/Release selection to the Godot smoke wrapper so local runs validate the same `bin/windows/{debug,release}` output path used by the `.gdextension` manifest.
  - Wired the Windows GitHub Actions job to download Godot 4.5, run the script-stub smoke before building the GDExtension, and run `-RequireExtension -PreflightOnly` after building the GDExtension so CI validates native DLL staging and Godot editor lookup paths without launching unsupported Windows native rendering.
  - Updated GitHub Actions so build jobs can be opted in from any branch by including `[CI BUILD]` in the pushed commit message, while generated-binary sync jobs remain restricted to direct `main`/`develop` pushes.
  - Added a C-safe `ImmGodotPlayerInfo` API and `ImmViewerNode.get_background_color()` so the Godot sample can observe IMM document background color without exposing C++ player structs across the ABI. The sample and smoke scenes now apply that value through `RenderingServer.set_default_clear_color(...)` before camera rendering.
  - Added Godot-facing `set_document_transform()` / `get_document_transform()` hooks that store a `Transform3D` and apply it through `ImmGodot_SetDocumentToWorld`, preserving the shared native `flipZ` parity path. The smoke runner now sets and resets a non-identity document transform to keep the script/native surface covered.
  - Added Godot-facing spawn-area info queries (`get_spawn_area_info`, `get_active_spawn_area_info`) with converted pose basis vectors, plus sample camera-rig jumps that mirror Unity's active-spawn view-target compensation.
  - Hardened the Godot spawn-area ABI to use caller-owned `ImmGodotSpawnArea` storage and fixed-size name buffers instead of returning allocated C strings across the plugin boundary.
  - Extended the native smoke harness to validate authored spawn-area transform dictionaries beyond non-empty results: it now checks every authored spawn-area ID, active-index bounds, active info/id consistency, converted position/basis vectors, raw pose fields, locomotion metadata, volume constraints, and next/previous spawn-area index cycling before sample jump logic depends on them.
  - Tightened Phase 3 spawn-area matrix parity coverage: the smoke runner now verifies converted basis vectors are finite, non-degenerate, approximately orthogonal, and preserve right-handed orientation, and also checks raw rotation/scale fields for finite positive values.
  - Added Godot playback time controls (`set_time`, `get_time`, `get_play_time`, `get_play_time_seconds`, `seek_relative_seconds`) backed by native `piTick` conversion helpers, plus sample seek/status controls.
  - Hardened native playback time controls after smoke exposed a crash in `set_time()` before the IMM timeline is fully entered. `ImmViewerNode` now keeps timeline setters/relative seeks as safe no-ops until the stronger native timeline-ready state is reached, while script-stub smoke still validates seek math and both smoke modes validate non-negative/default time snapshots.
  - Extended smoke coverage for Godot signals: `document_loaded`, `document_unloaded`, `playback_changed`, and `spawn_area_changed` are now connected and asserted during load, unload, playback, and spawn-area navigation. The script stub now also mirrors native backend lifecycle signals (`native_backend_initialized`, `native_backend_failed`), and `verify_sample_api.py` fails if native/script signal sets drift.
  - Added sample and smoke coverage for document volume parity through `set_volume()` / `get_volume()`, including clamp validation in the smoke runner.
  - Added C-safe Godot document state/info queries (`get_document_state`, `get_document_info_flags`, `is_sequence_ready`) so the sample can report loading/playback state and IMM info flags like the Unity wrapper.
  - Added C-safe document bounds and layer summary queries (`get_bounding_box`, `get_layer_count`, `get_layer_info`) with sample status output for bounds and layer count.
  - Added C-safe layer visibility/opacity override and diagnostics hooks (`set_layer_visible`, `clear_layer_visibility_override`, `set_layer_opacity`, `get_layer_diagnostics`) with a sample first-layer visibility toggle and smoke assertions for visibility/opacity override diagnostics.
  - Added layer transform override parity (`set_layer_transform`, `clear_layer_transform_override`) through the same host-matrix plus `flipZ` native conversion used by document transforms, with smoke assertions for transform override diagnostics when authored layers exist.
  - Added direct chapter query/selection methods (`get_chapter_count`, `get_current_chapter`, `set_chapter`) and sample chapter status, matching Unity's chapter API beyond next/previous shortcuts.
  - Aligned the native `ImmViewerNode` surface with the sample controller by adding `toggle_pause()` to the GDExtension class, so the script-backed and native-backed control paths expose the same playback command.
  - Added `verify_sample_api.py` to catch sample-controller calls that are missing from the native `ImmViewerNode` bindings or the script stub while Godot CLI validation is unavailable.
  - Added `verify_local.py` and wired `build-godot-extension.ps1 -VerifyOnly` through it so local Godot extension API, `.gdextension` manifest, sample Compatibility renderer setting, native `ImmViewerNode` registration, `ImmViewerNode` method binding coverage, sample/native scene structure, `ImmViewerNode` camera registration plus camera/viewport render queue ownership, `ImmGodot` C ABI export alignment, Python, and native syntax checks can run before MSBuild/SCons/`godot-cpp` are installed.
  - Added optional PowerShell AST syntax validation for the Windows Godot build/smoke helper scripts when `pwsh` or Windows PowerShell is available, so CI can catch script parse errors before invoking the Windows build path.
  - Added optional local Godot script-smoke execution through `IMM_GODOT_RUN_LOCAL_SMOKE=1 python code/appImmGodotGDExtension/verify_local.py`, which runs the script-stub sample scene headlessly and validates project loading, GDScript parsing, scene wiring, `auto_queue_render`, sample document load state, document state/background color, Godot clear-color application, chapter/bounds/layer/spawn-area query APIs, playback controls, and the camera/viewport queue before native extension binaries exist.
  - Added an optional `godot-cpp` syntax-only verification path: when `GODOT_CPP_PATH` or `thirdparty/godot-cpp` points at a checkout with generated bindings, `verify_local.py` compiles `imm_viewer_compositor_effect.cpp`, `imm_viewer_node.cpp`, and `register_types.cpp` against the actual Godot 4.5 C++ headers. This caught and fixed binding/API drift before the Windows build runs.
  - Updated the Windows build helper to rerun local verification with `GODOT_CPP_PATH` set after `godot-cpp` is built, so generated binding/API drift is checked before the extension SCons build.
  - Fixed the Windows CI native-smoke extension-load path by mirroring the staged Release GDExtension DLL set into `bin/windows/debug` before launching the Godot editor/headless smoke. Godot's editor feature lookup selects the debug manifest entry even when CI built the Release DLLs.
  - Fixed shared IMM backend initialization for headless/no-audio environments by falling back to the null sound backend when the platform sound backend is unavailable or fails to initialize. This matches the standalone viewer's validation behavior, but the Windows Godot CI gate still stops at GDExtension staging preflight until a valid Windows renderer backend exists.
  - Extended local verification so `verify_local.py` checks the shared bridge audio fallback tokens and syntax-compiles `appImmShared/src/imm_engine_bridge.cpp` alongside the Godot wrapper sources.
- **Remaining for Phase 1 completion:**
  - Commit and push the CI gate correction after review, then rerun Windows CI so the branch proves GDExtension build/staging and script-stub smoke without pretending native Windows rendering is production-ready. Native Windows document/render smoke stays deferred until the Windows backend is either Vulkan/D3D-backed or otherwise wired to a valid Godot-owned render target.

1. Add new project `appImmGodot` (parallel to `appImmUnity`).
2. Wrap native player lifecycle behind engine-agnostic functions:
   - `init(config)` / `shutdown()`
   - `global_work(enabled)`
   - `set_camera_matrices(camera_id, stereo_mode, ... )`
   - `render_camera(camera_id, viewport_info)`
3. Move Unity-specific glue (device callbacks, Unity event entry points) behind compile-time adapter interfaces.

**Deliverable:** IMM runtime can initialize independently from Unity headers.

## Phase 2 — Godot rendering integration
### Phase 2 status (in progress)
- **Current state:** the macOS debug GDExtension builds locally, loads in the sample project, instantiates native `ImmViewerNode`, loads the sample IMM document, and passes the native headless smoke on installed Godot 4.6.1. The GDExtension can queue camera matrices and render from Godot's render-thread handoff. It can also report backend readiness diagnostics (`get_render_backend_diagnostics`) including both project-configured and actual runtime renderer/method names. A native `ImmViewerCompositorEffect` now provides the production render-pipeline callback, records Godot-owned RD/driver resources from `RenderSceneBuffersRD`, renders IMM Metal content into a Godot-created intermediate Metal/RD texture, and composites that texture back into Godot's scene color framebuffer through a Godot `RenderingDevice` draw pass.
- **Visual Metal smoke status:** `MetalVisualSmokeScene.tscn` now runs under Godot Forward+/Metal on macOS, attaches `ImmViewerCompositorEffect` to the active camera, loads `sample1.imm`, performs one reload cycle by default, saves a 1600x900 PNG, and passes locally on Godot 4.6.1. The latest local run after the compositor orientation and timeline-safety smoke updates reported 1308 content pixels, a 52x43 content bound, luma range 1.0, orientation luma delta 0.1804, `sequence_ready=true`, `last_metal_frame_started=true`, `last_had_intermediate_texture=true`, and `last_composite_result=true`. The visual smoke now also checks orientation by comparing upper/lower content luma in the saved PNG. The compositor samples the Godot-owned IMM Metal intermediate texture with RD's native texture orientation, without adding an extra vertical flip during the final scene-color composite.
- **Production renderer decision:** OpenGL Compatibility was selected only to simplify bootstrap. Because Godot's clean render-pipeline extension points are Forward+/Mobile and IMM already has a Metal renderer, the preferred first production target is **macOS Metal**, not OpenGL Compatibility.
- **Current Metal seam:** `ImmGodot_BeginMetalFrame` can attach the existing IMM Metal renderer to externally owned Metal command buffers/encoders/render-pass descriptors, and `ImmGodot_EndMetalFrame` closes that frame after `ImmGodot_RenderCamera`. Instrumentation shows the IMM Metal renderer issues indexed draw calls and command buffers complete successfully.
- **Godot presentation finding:** direct IMM rendering or red-clear diagnostics submitted via an independent Metal command buffer against Godot's reported scene color texture did not appear in the presented viewport. A Godot `RenderingDevice` framebuffer clear did present. The current working path therefore keeps IMM rendering GPU-direct into a Godot-owned intermediate texture, then uses Godot RD to perform the final scene-color composite so presentation remains inside Godot's render graph. CPU readback followed by upload to a Godot texture remains allowed only as a diagnostic capture path, not as the renderer.
- **Portability note:** Metal is the first production integration target because the backend already exists. It does not remove the need for Vulkan and/or Direct3D 11/12 backends later; those are required for production Godot rendering on non-Apple platforms.

1. Implement a GDExtension class (e.g., `ImmViewerNode`) and register it.
2. Hook render lifecycle using Godot rendering callbacks (render-thread safe):
   - per-frame global work trigger
   - per-camera data capture (view/projection for mono first)
   - draw callback that invokes IMM Metal render for that camera into a Godot-owned render target
3. Map Godot viewport/render-target dimensions to IMM `res` argument.
4. Recreate single-pass/multipass stereo logic only after mono path is stable.

**Key risk:** Godot render threading/resource ownership differs from Unity command buffers; ensure all Metal calls occur on the render thread and that IMM renders only into Godot-owned render targets or explicitly shared Metal textures.

## Phase 3 — Coordinate system and matrix parity
1. Build explicit conversion utilities:
   - Godot `Transform3D`/`Projection` ➜ IMM matrix format
   - handedness/depth-range normalization (`[-1,1]` vs `[0,1]`)
2. Port Unity’s `flipZ`-equivalent behavior and validate against known sample files.
3. Port spawn-area transform adaptation and locomotion metadata.

**Validation:** A known spawn area should place camera/rig identically (within tolerance) versus Unity sample.

## Phase 4 — Public API + scripting surface in Godot
1. Expose a Godot script API mirroring `ImmViewer` methods:
   - load/unload, play controls, chapter/time, volume
   - player/document/spawn-area queries
2. Provide a sample Godot scene equivalent to `UseRenderingPlugin` behavior:
   - keybindings for load/unload/playback
   - jump/follow spawn areas
   - optional camera-rig synchronization

## Phase 5 — XR/stereo enablement
1. Start with multipass stereo (simpler parity) before single-pass optimizations.
2. Integrate with Godot XR interfaces for tracking origin modes and eye transforms.
3. Add explicit handling for per-eye projection and render-target layout.

## Phase 6 — Hardening and release prep
1. Performance profiling (CPU update, GPU frame time, memory).
2. Error/logging bridge from native logs into Godot editor console.
3. Packaging docs for editor/runtime deployment on target OS.
4. Regression checklist against Unity plugin feature parity.

---

## 4) Concrete mapping: Unity concepts ➜ Godot equivalents

- `Camera.onPreCull` matrix capture ➜ camera/viewport render callback data extraction in Godot.
- `CommandBuffer.IssuePluginEvent(GetRenderEventFunc, id)` ➜ `ImmViewerCompositorEffect` render callback consuming queued camera data and invoking IMM draw into Godot-owned render resources.
- Unity `ColorSpace`/AA config passed to `Init(...)` ➜ project settings read from Godot and passed at startup.
- Unity camera clear color update from `GetPlayerInfo` ➜ `ImmViewerNode.get_background_color()` feeds `RenderingServer.set_default_clear_color(...)` in the sample and smoke harness before camera render.

---

## 5) Suggested implementation order (first milestone)
1. Godot node + native init/shutdown only.
2. Mono camera matrix feed + mono rendering.
3. Load/unload + simple playback commands.
4. Background color + document transform.
5. Spawn areas read/apply.
6. XR/stereo.

This sequence minimizes risk by validating rendering correctness before XR complexity.

---

## 6) Risks and mitigations

1. **Renderer backend mismatch (Vulkan vs existing DX/GL assumptions).**
   - Mitigation: add abstraction layer now; start with backend already supported by `piRenderer`.
2. **Render-thread/context ownership issues in Godot.**
   - Mitigation: isolate render entrypoint and enforce thread assertions in debug builds.
3. **Subtle matrix/clip/handedness differences.**
   - Mitigation: golden-scene tests using known IMM files and spawn-area anchors.
4. **XR parity complexity.**
   - Mitigation: mono parity first, multipass stereo second, single-pass last.

---

## 7) Acceptance criteria for “Unity parity v1”

- A sample IMM file loads and renders correctly in a Godot scene.
- Playback controls function (pause/resume/restart/skip).
- Background color and document placement match Unity behavior.
- Spawn-area query + jump works and camera rig updates correctly.
- No render-thread violations/crashes during repeated load/unload cycles.
