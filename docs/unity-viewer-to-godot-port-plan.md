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
  - Added a native Godot render-adapter C ABI with graphics init/shutdown and before/after camera render callbacks.
  - Wired the GDExtension `ImmViewerNode` to initialize/shutdown the native Godot backend and expose render diagnostics.
  - Added a script-mode Godot smoke runner that verifies the sample scene API, playback controls, and render diagnostics.
  - Added a native smoke scene and `-RequireExtension` smoke path that must load the GDExtension `ImmViewerNode` class instead of the script stub.
  - Extended the Godot smoke runner with repeated load/render/unload lifecycle cycles after parity diagnostics so native smoke covers the v1 stability acceptance criterion.
  - Hardened the Windows smoke wrapper to require the lifecycle coverage marker and record it in smoke summaries, so CI fails if the repeated lifecycle path is skipped.
  - Resolved native Godot document loads relative to the Godot project root so CI smoke runs are independent of process working directory.
  - Corrected the Godot sample document path to resolve to the repository `exampleImmFiles/sample1.imm` from the Godot project root.
  - Added a Godot smoke preflight that globalizes the configured document path and fails before native load if the file is missing.
  - Routed native Godot backend logs to `user://imm_godot_log.txt` and had the Windows smoke wrapper collect that log in CI artifacts when present.
  - Hardened native Godot document loading to free temporary UTF-8/UTF-16 path conversion buffers after `Player.Load`.
  - Added Godot project, GDExtension manifest, and native smoke scene copies to Windows smoke artifacts so loader failures can be diagnosed from CI output alone.
  - Extended the Windows Godot smoke wrapper so missing Godot executable and missing document preflight failures write the same smoke summary artifacts as runtime-DLL failures.
  - Added initial Windows SCons/PowerShell build helpers for bootstrapping `godot-cpp` and producing the GDExtension DLL.
  - Hardened the Godot Windows build helper to resolve a Visual Studio C++ MSBuild toolchain via `MSBUILD_EXE_PATH`, preferred VS paths, `vswhere`, or PATH before building `appImmGodot`.
  - Hardened the GDExtension `SConstruct` with explicit configuration, `ImmGodotPlugin`, `godot-cpp`, and output-directory preflight checks.
  - Hardened the GDExtension `SConstruct` path resolution so it derives repository paths from the SConstruct location instead of the process working directory.
  - Added runtime dependency staging for the Godot output folder so `ImmGodotPlugin.dll` loads with its media DLLs.
  - Added SHA-256 hashes to the Windows build/smoke DLL manifests for loader and artifact triage.
  - Added Windows CI wiring to cache/build `godot-cpp`, build the Godot GDExtension, download Godot, run native smoke, and upload the resulting DLLs plus smoke diagnostics.
  - Hardened Windows CI Godot artifact uploads so staged DLLs, build logs, and smoke logs are still uploaded after native smoke failures when files exist.
  - Added Windows CI Godot build-log capture/upload (`ImmGodotBuild-Windows`) so compiler/linker failures before smoke still produce artifacts.
  - Added a C-safe `ImmGodot_GetDocumentState` shape and exposed `get_document_state()` on `ImmViewerNode` for loading/playback state diagnostics.
  - Added a C-safe `ImmGodot_GetBoundingBox` shape and exposed `get_bounding_box()` on `ImmViewerNode` for document bounds parity diagnostics.
  - Added a C-safe `ImmGodot_GetPlayerInfo` shape and exposed `get_background_color()` on `ImmViewerNode` for background-color parity diagnostics.
  - Converted exported Godot diagnostics APIs from C++ reference parameters to pointer parameters with null guards so the native bridge surface remains C ABI compatible.
  - Split the native Godot ABI import/export macro so `appImmGodot` exports symbols while the GDExtension imports them, and made the header safe for C/C++ linkage with fixed integer spawn-area ABI constants.
  - Tightened native Godot read-only ABI inputs to `const` pointers and removed the GDExtension document-path `const_cast`.
  - Added backend-initialization guards and default-zero outputs around native Godot render, matrix, document, and query C ABI calls so failed initialization produces stable smoke diagnostics instead of undefined player calls.
  - Added Godot spawn-area query/jump APIs (`get_spawn_area_info`, `get_active_spawn_area_id`, `set_active_spawn_area_index`) on top of the native spawn-area C ABI.
  - Hardened the native Godot spawn-area C ABI against null/undersized output buffers and invalid spawn-area ids so diagnostics calls fail cleanly instead of crashing native smoke.
  - Changed the native Godot spawn-area ABI to serialize names into a fixed buffer so Godot queries do not leak native string allocations.
  - Aligned the script-stub and native Godot spawn-area id API around `PackedInt32Array` so native smoke does not rely on implicit Array conversion.
  - Added matrix debug logging toggles and render diagnostics snapshots for the last submitted Godot camera matrices.
  - Added native `set_document_transform(document_transform)` support on `ImmViewerNode`, forwarding to `ImmGodot_SetDocumentToWorld` and exposing document-to-world diagnostics in smoke output.
  - Added an explicit `set_camera_matrices(camera_id, world_to_head, projection)` Godot API so parity tests can submit and verify deterministic matrix feeds.
  - Hardened shared bridge initialization to free temporary UTF-8/UTF-16 conversion buffers for log and renderer API strings.
  - Added machine-readable Godot smoke matrix diagnostics (`IMM_GODOT_MATRIX_DIAGNOSTICS_JSON` / `godot-matrix-diagnostics.json`) for Unity-vs-Godot parity comparisons.
  - Hardened the Windows smoke wrapper to parse Godot matrix diagnostics and require the expected schema, camera id, 16-float matrix arrays, and deterministic document/camera/projection values.
  - Added a machine-readable Godot smoke summary JSON artifact for CI triage alongside the raw Godot output and matrix diagnostics, including missing-runtime-DLL preflight failures and post-run diagnostics errors before failing the wrapper.
  - Added matching Unity sample matrix diagnostics (`IMM_UNITY_MATRIX_DIAGNOSTICS_JSON`) from the existing Unity camera feed.
  - Added a deterministic Unity matrix diagnostics path that emits the same parity matrices submitted by the Godot smoke runner.
  - Extended the deterministic Unity diagnostics path to submit/log the same document transform as the Godot smoke runner and include document state, background color, bounding box, and spawn-area state when a sample document is loaded.
  - Added document identity diagnostics (`document_name` and `document_size_bytes`) to Unity and Godot parity payloads so comparator artifacts prove both captures used matching IMM content.
  - Added a Windows Unity batchmode parity-capture helper that runs the deterministic Unity diagnostics path and saves `unity-matrix-diagnostics.log` for comparison.
  - Added Unity parity summary artifacts (`unity-parity-summary.txt/json`) so Unity executable, plugin-sync, batchmode, and missing-diagnostics failures are captured before the wrapper exits nonzero.
  - Added Unity editor/runtime version diagnostics to Unity parity payloads and summaries so parity artifacts identify both engine runtimes involved.
  - Extended Unity/Godot comparison summaries to preserve Unity and Godot runtime versions and require them during strict extended parity checks.
  - Added an explicit Unity parity plugin sync path so Windows parity capture can copy the freshly built `ImmUnityPlugin.dll` and runtime dependencies into the Unity sample package before launching the editor.
  - Hardened the Unity parity capture wrapper to always clear the `IMM_UNITY_PARITY_DOCUMENT` environment override after the Unity batch run.
  - Added a matrix diagnostics comparator for Unity/Godot JSON payloads with tolerance-based mismatch reporting, machine-readable summary output, and optional or strict document/background/bounds/spawn parity checks.
  - Added an end-to-end Windows parity helper that runs Godot smoke, Unity batch capture, and strict Unity-vs-Godot comparison into one artifact folder, with an optional document override applied to both engines.
  - Added top-level end-to-end parity summary artifacts (`unity-godot-parity-summary.txt/json`) so Godot smoke, Unity capture, comparator, and success phases are recorded consistently.
  - Added optional manual Windows workflow wiring to run Unity parity capture and strict Unity-vs-Godot diagnostics comparison when a Unity editor path is supplied, with an optional shared IMM document override for the Godot smoke and Unity capture steps.
  - Hardened Windows parity fixture selection so manual CI parity and `run-unity-godot-parity.ps1` resolve one shared IMM document path and pass it to both Godot and Unity instead of relying on separate project-local defaults.
  - Switched manual Windows CI parity to use `run-unity-godot-parity.ps1` directly so CI and local parity runs produce the same top-level phase summary and artifact layout.
  - Added a local verifier for Godot bridge contracts, helper script wiring, matrix comparator coverage, and optional script-stub Godot smoke.
  - Extended the verifier to check native C ABI declarations against implementations and the smoke-tested Godot API against both script and native `ImmViewerNode` surfaces.
  - Extended the verifier to require explicit ownership cleanup for native UTF-8/UTF-16 string conversion allocations in the Godot/shared bridge path.
  - Extended comparator verification to require machine-readable failure summaries for both matrix and camera-id mismatches.
  - Hardened comparator summary output so malformed payloads, schema mismatches, and invalid matrix shapes still produce CI-readable failure JSON.
  - Added the Godot bridge verifier to Windows CI before the native GDExtension build.
  - Added optional PowerShell AST syntax validation for the Windows Godot helper scripts when PowerShell is available.
  - Fixed Windows batch wrappers for Godot/Unity parity helpers to propagate PowerShell exit codes and added verifier coverage for that contract.
  - Added optional Godot native build summary artifacts (`godot-build-summary.txt/json`) so Windows CI can identify toolchain, MSBuild, SCons, dependency staging, DLL verification, and success phases without scraping the full build log.
  - Hardened the Godot Windows build helper to preflight SCons/Git tool resolution and include resolved tool paths in build summaries.
  - Hardened the GDExtension SConstruct to preflight `godot-cpp` include directories, key Godot headers, and the native source list before invoking MSVC.
  - Added Godot smoke diagnostics and verifier coverage for the `gl_compatibility` renderer method so Phase 1 artifacts prove the planned Compatibility/OpenGL path was used.
  - Added Godot engine version diagnostics to smoke payloads and summaries so CI artifacts identify the runtime that produced native smoke/parity evidence.
  - Added a local macOS `godot-cpp`/SCons path for building the Godot GDExtension against the Apple-Clang-built `ImmGodotPlugin.dylib`.
  - Imported and wired the Metal `piRenderer` backend on macOS so the native Godot bridge no longer depends on unsupported Apple OpenGL features for local validation.
  - Fixed native Godot autoplay sequencing so `Load` is not overwritten by `Resume/Show` before `GlobalWork` processes it.
  - Fixed macOS native loader stability by moving the `piLog::Printf` formatting buffer off the async loader thread stack.
  - Fixed Metal command-buffer ownership for internally-created command buffers so offscreen render/readback can survive repeated native render calls.
  - Added an env-gated native offscreen capture path (`IMM_GODOT_NATIVE_CAPTURE_PATH`) that renders the IMM frame into a Metal render target, reads it back, and writes a PNG for validation.
  - Tightened the local native smoke runner so it waits for the document to reach the real native `Loaded` state by pumping both `global_work()` and `render_camera()`, then requires valid bounds and spawn-area diagnostics.
  - Verified macOS native smoke with `sample1.imm`: backend initialized, document reached loaded state, bounds were valid, four spawn areas were reported, lifecycle reload/render/unload cycles passed, and `build/validation/imm-native-offscreen.png` contained non-background IMM pixels.
- **Remaining for Phase 1 completion:**
  - Run the Windows GDExtension CI job and fix any compiler/linker/native-smoke issues it exposes.
  - Compare `IMM_GODOT_MATRIX_DIAGNOSTICS_JSON` and `IMM_UNITY_MATRIX_DIAGNOSTICS_JSON` from equivalent sample camera poses and fix coordinate/clip-space mismatches.
  - Promote the macOS offscreen capture check into a stable local/CI validation gate when the macOS toolchain is available.

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
- **Status:** In progress
- **Current macOS evidence:**
  - Native IMM content renders successfully into an offscreen Metal render target and can be captured through `IMM_GODOT_NATIVE_CAPTURE_PATH`.
  - The Godot viewport capture still shows only the Godot background color, so native IMM pixels are not yet presented inside the Godot scene.
- **Next required integration step:** bridge the native render target into Godot presentation, either by sharing/wrapping a native texture with Godot's rendering backend or by uploading offscreen readback pixels into a Godot texture for the first visible mono path.

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
