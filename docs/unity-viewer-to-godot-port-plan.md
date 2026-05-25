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
- **Remaining for Phase 1 completion:**
  - Add adapter interfaces for engine-specific device/context callbacks beyond the current direct entrypoints.
  - Vendor/configure `godot-cpp` and build the GDExtension binary.
  - Add matrix/debug logging toggles and a minimal Godot-side smoke harness.

1. Add new project `appImmGodot` (parallel to `appImmUnity`).
2. Wrap native player lifecycle behind engine-agnostic functions:
   - `init(config)` / `shutdown()`
   - `global_work(enabled)`
   - `set_camera_matrices(camera_id, stereo_mode, ... )`
   - `render_camera(camera_id, viewport_info)`
3. Move Unity-specific glue (device callbacks, Unity event entry points) behind compile-time adapter interfaces.

**Deliverable:** IMM runtime can initialize independently from Unity headers.

## Phase 2 — Godot rendering integration
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
