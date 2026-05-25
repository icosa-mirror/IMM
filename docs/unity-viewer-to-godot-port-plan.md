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
- Use a **compatibility renderer path (OpenGL)** for v1 bring-up, then evaluate Vulkan after parity milestones.

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
| Renderer APIs in IMM | `piRenderer::API` currently supports **GL, DX, GLES** only (no Vulkan enum path today). | Godot 4 Forward+ (Vulkan) is not a day-1 backend for IMM without renderer expansion. |
| Unity integration backend choices | Unity plugin chooses **DX** when a device is supplied, otherwise **GL** (and GLES on Android). | Existing IMM integration patterns are DX/GL-centric and should be reused in Phase 1. |
| Player render-path assumptions | `libImmPlayer` has API-conditional behavior for DX vs GL/GLES (clip-space/depth/front-face/stereo handling). | Godot bridge must preserve these assumptions and avoid introducing new math conventions during bring-up. |
| Build/tooling footprint | Repository currently contains a Windows-first solution under `code/projects/windows/imm.sln`. | Lowest-risk first target remains Windows desktop. |

### Final Phase 0 decisions
1. **Initial platform:** Windows desktop.
2. **Initial renderer path:** OpenGL compatibility path for Godot integration in v1; Vulkan deferred.
3. **Initial feature scope:** Mono rendering parity first; XR/stereo deferred.
4. **Parity baseline for milestone 1:**
   - Load/render one IMM document
   - Background color sync
   - Document transform parity (including handedness)
   - Spawn-area query/jump
   - Basic playback controls (pause/resume/restart/skip)

### Phase 0 deliverables

#### A) Render hook and threading design note (agreed)
- `ImmViewerNode` (GDExtension) owns scene-facing API and camera registration.
- `ImmRenderBridge` owns native IMM state and is the only layer allowed to call `piRenderer`/`Player::Render*`.
- `global_work()` is called from the frame/update side; `render_camera()` is called from render-thread callback only.
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
  - Added a Windows SCons build scaffold for `appImmGodotGDExtension` that links against a prebuilt `godot-cpp` library and `ImmGodotPlugin.lib`, outputting the DLL path expected by the sample `.gdextension` manifest.
  - Extended the GDExtension SCons build to stage `ImmGodotPlugin.dll` and the IMM runtime dependency DLLs beside `imm_godot_extension.dll`, matching Godot's extension-load directory expectations.
  - Added Windows build helpers (`build-godot-extension.ps1` / `.bat`) that rebuild `appImmGodot`, optionally build `godot-cpp`, run the GDExtension SCons build, and copy runtime binaries into the sample project.
  - Added a sample-scene smoke driver: `queue_render_camera_transform(camera_transform, width, height, fov_degrees, camera_id)` now submits mono camera matrices from the active camera and queues render using the active viewport dimensions.
  - Added a render-camera ID lifecycle (`register_render_camera`, `unregister_render_camera`, `is_render_camera_registered`, `get_registered_render_camera_ids`) so `ImmViewerNode` can own camera 0 registration before queuing per-frame render work.
  - Moved per-frame camera scheduling into `ImmViewerNode` behind `auto_queue_render` and `render_camera_path`, so the node owns camera registration, viewport-size capture, matrix submission, and render-thread queueing instead of the sample controller.
  - Kept `\` as a manual fixed-viewport render-thread smoke hook through `queue_render_last_camera()`.
  - Added a first Godot render-thread scheduling diagnostic through `RenderingServer.call_on_render_thread`, with an internal `render_last_camera_on_render_thread` callback invoking the existing mono smoke render hook. This proves render-thread handoff and native render invocation only; it is not the final viewport presentation path.
  - Added a mutex-protected render request snapshot for the native queued render path so scene-thread camera/viewport submissions are not read directly by the render-thread callback, and kept the queued flag set until `ImmGodot_RenderCamera` returns.
  - Added a Godot-facing `get_render_diagnostics()` snapshot so script/native smoke tests can assert the queued camera id, viewport dimensions, registered camera set, projection submission, and render callback queued state without reaching into private render-thread state.
  - Replaced the initial no-op Godot render adapter callbacks with counted adapter callbacks, and extended smoke diagnostics to verify graphics initialization plus before/after render callback bracketing around queued camera work.
  - Added `build-godot-extension.ps1 -BootstrapGodotCpp` so Windows local/CI builds can clone the default Godot 4.2-compatible `godot-cpp` bindings into `thirdparty/godot-cpp` instead of requiring a manual checkout first.
  - Added Windows preflight modes for the Godot extension build and smoke wrapper so tool/dependency/Godot executable resolution can be checked before starting the full native build or launching the project.
  - Added `build-godot-extension.ps1 -RunSmoke` to run the native Godot smoke scene immediately after a successful Windows GDExtension build.
  - Extended the Windows GitHub Actions workflow to install SCons, cache the Godot 4.2 `godot-cpp` checkout/build tree, bootstrap/build `godot-cpp` on cache misses, run the Godot GDExtension build helper, and upload the staged GDExtension DLL set.
  - Updated `build-godot-extension.ps1 -BuildGodotCpp` to reuse a cached `godot-cpp` library plus generated bindings when present, while still rebuilding the bindings on cache misses or incomplete cache restores.
  - Hardened `run-godot-smoke.ps1 -RequireExtension` so native smoke preflight requires the GDExtension DLL, `ImmGodotPlugin.dll`, and all staged IMM runtime dependency DLLs before launching Godot.
  - Hardened the Windows Godot smoke wrapper to require the `IMM Godot smoke test passed` marker in Godot output in addition to a zero process exit code, and to record the marker result in smoke summaries.
  - Added `godot-extension-dlls.txt` smoke-log output for native smoke runs, recording expected staged DLLs with found/missing status, byte size, and UTC timestamp before Godot launches.
  - Hardened `build-godot-extension.ps1` with a post-SCons staged-output check that requires the GDExtension DLL, `ImmGodotPlugin.dll`, and all runtime dependency DLLs before reporting updated sample binaries or running optional smoke.
  - Added a staged-output `godot-extension-dlls.txt` manifest beside the GDExtension DLLs and included it in the Windows `ImmGodotGDExtension-Windows` artifact for CI diagnostics.
  - Added local verification that the source IMM runtime dependency DLLs referenced by the Godot SCons staging step exist in the repository's third-party layout before the Windows build attempts to copy them.
  - Added a headless Godot smoke runner (`smoke_test_runner.gd`) and Windows wrapper (`run-godot-smoke.ps1`) that instantiate the sample scene, verify the Compatibility renderer setting, verify `ImmViewerNode` auto-registers camera 0 through `auto_queue_render`, load the sample IMM document, require `is_loaded()`, check document state/background color, exercise chapter/bounds/layer/spawn-area query APIs, exercise playback controls, exercise the camera/viewport render queue, and validate the render diagnostics snapshot.
  - Added Godot smoke log capture through `run-godot-smoke.ps1 -LogDir`, and wired the Windows workflow to upload `ImmGodotSmokeLogs-Windows` so first real project-load/render failures preserve output and run metadata.
  - Split the Windows workflow smoke gates into script-stub smoke before the native build and native-extension smoke after the GDExtension build, separating Godot project/GDScript regressions from native load/render failures.
  - Split smoke validation into a script-stub scene (`SampleScene.tscn`) and native-only scene (`NativeSmokeScene.tscn`), and removed the script stub's `class_name ImmViewerNode` so it does not shadow the GDExtension class during native validation.
  - Added Debug/Release selection to the Godot smoke wrapper so local runs validate the same `bin/windows/{debug,release}` output path used by the `.gdextension` manifest.
  - Wired the Windows GitHub Actions job to download Godot 4.2.2, run the script-stub smoke before building the GDExtension, and run the native smoke after building the GDExtension.
  - Added a C-safe `ImmGodotPlayerInfo` API and `ImmViewerNode.get_background_color()` so the Godot sample can observe IMM document background color without exposing C++ player structs across the ABI.
  - Added a Godot-facing `set_document_transform()` hook that stores a `Transform3D` and applies it through `ImmGodot_SetDocumentToWorld`, preserving the shared native `flipZ` parity path.
  - Added Godot-facing spawn-area info queries (`get_spawn_area_info`, `get_active_spawn_area_info`) with converted pose basis vectors, plus sample camera-rig jumps that mirror Unity's active-spawn view-target compensation.
  - Hardened the Godot spawn-area ABI to use caller-owned `ImmGodotSpawnArea` storage and fixed-size name buffers instead of returning allocated C strings across the plugin boundary.
  - Added Godot playback time controls (`set_time`, `get_time`, `get_play_time`, `get_play_time_seconds`, `seek_relative_seconds`) backed by native `piTick` conversion helpers, plus sample seek/status controls.
  - Added C-safe Godot document state/info queries (`get_document_state`, `get_document_info_flags`, `is_sequence_ready`) so the sample can report loading/playback state and IMM info flags like the Unity wrapper.
  - Added C-safe document bounds and layer summary queries (`get_bounding_box`, `get_layer_count`, `get_layer_info`) with sample status output for bounds and layer count.
  - Added C-safe layer visibility/opacity override and diagnostics hooks (`set_layer_visible`, `clear_layer_visibility_override`, `set_layer_opacity`, `get_layer_diagnostics`) with a sample first-layer visibility toggle.
  - Added layer transform override parity (`set_layer_transform`, `clear_layer_transform_override`) through the same host-matrix plus `flipZ` native conversion used by document transforms.
  - Added direct chapter query/selection methods (`get_chapter_count`, `get_current_chapter`, `set_chapter`) and sample chapter status, matching Unity's chapter API beyond next/previous shortcuts.
  - Aligned the native `ImmViewerNode` surface with the sample controller by adding `toggle_pause()` to the GDExtension class, so the script-backed and native-backed control paths expose the same playback command.
  - Added `verify_sample_api.py` to catch sample-controller calls that are missing from the native `ImmViewerNode` bindings or the script stub while Godot CLI validation is unavailable.
  - Added `verify_local.py` and wired `build-godot-extension.ps1 -VerifyOnly` through it so local Godot extension API, `.gdextension` manifest, sample Compatibility renderer setting, native `ImmViewerNode` registration, `ImmViewerNode` method binding coverage, sample/native scene structure, `ImmViewerNode` camera registration plus camera/viewport render queue ownership, `ImmGodot` C ABI export alignment, Python, and native syntax checks can run before MSBuild/SCons/`godot-cpp` are installed.
  - Added optional PowerShell AST syntax validation for the Windows Godot build/smoke helper scripts when `pwsh` or Windows PowerShell is available, so CI can catch script parse errors before invoking the Windows build path.
  - Added optional local Godot script-smoke execution through `IMM_GODOT_RUN_LOCAL_SMOKE=1 python code/appImmGodotGDExtension/verify_local.py`, which runs the script-stub sample scene headlessly and validates project loading, GDScript parsing, scene wiring, `auto_queue_render`, sample document load state, document state/background color, chapter/bounds/layer/spawn-area query APIs, playback controls, and the camera/viewport queue before native extension binaries exist.
  - Added an optional `godot-cpp` syntax-only verification path: when `GODOT_CPP_PATH` or `thirdparty/godot-cpp` points at a checkout with generated bindings, `verify_local.py` compiles `imm_viewer_node.cpp` and `register_types.cpp` against the actual Godot 4.2 C++ headers. This caught and fixed the Godot 4.2 `class_db.hpp` include path and GDExtension entry-point signature before the Windows build runs.
  - Updated the Windows build helper to rerun local verification with `GODOT_CPP_PATH` set after `godot-cpp` is built, so generated binding/API drift is checked before the extension SCons build.
- **Remaining for Phase 1 completion:**
  - Run the updated Windows CI/local GDExtension build and Godot smoke test with `godot-cpp` bootstrap enabled, then fix any compiler/linker/project-load issues it exposes.
  - Replace the `ImmViewerNode` `RenderingServer.call_on_render_thread` smoke queue with production render integration that targets Godot-owned render resources on the render thread. Visible rendering must not be implemented by CPU readback/upload of native offscreen pixels.

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
- **Current state:** the GDExtension can queue camera matrices and invoke the native mono render smoke path from Godot's render-thread handoff.
- **Important limitation:** this smoke queue does not yet attach IMM drawing to a Godot viewport render target or Godot-owned texture, so it does not prove visible scene rendering.
- **Required next step:** use Godot's rendering backend ownership model for presentation: render into a Godot-owned render target, wrap/share a native backend texture through `RenderingDevice`/`RenderingServer`, or add the missing IMM backend support needed for that path. CPU readback followed by upload to a Godot texture is allowed only as a diagnostic capture path, not as the renderer.

1. Implement a GDExtension class (e.g., `ImmViewerNode`) and register it.
2. Hook render lifecycle using Godot rendering callbacks (render-thread safe):
   - per-frame global work trigger
   - per-camera data capture (view/projection for mono first)
   - draw callback that invokes IMM render for that camera
3. Map Godot viewport/render-target dimensions to IMM `res` argument.
4. Recreate single-pass/multipass stereo logic only after mono path is stable.

**Key risk:** Godot render threading model differs from Unity command buffers; ensure all graphics calls occur on correct thread/context.

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
- `CommandBuffer.IssuePluginEvent(GetRenderEventFunc, id)` ➜ Godot render-thread callback invoking IMM draw entrypoint.
- Unity `ColorSpace`/AA config passed to `Init(...)` ➜ project settings read from Godot and passed at startup.
- Unity camera clear color update from `GetPlayerInfo` ➜ set Godot environment/background before camera render.

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
