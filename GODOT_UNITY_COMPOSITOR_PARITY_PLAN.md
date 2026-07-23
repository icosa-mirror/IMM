# IMM Compositor Parity And Platform Regression Plan

## Goal Contract

This document can be used as a goal only if the active milestone is named explicitly.

There are two milestones:

- **Immediate milestone: ordered overlay/background composition.**
  IMM renders as the background or lower ordered layer, and normal Unity/Godot content renders visibly above it by host render order. Host depth is not used and is not claimed.
- **Deferred milestone: full depth compositing.**
  IMM and host geometry interleave by depth, with host geometry correctly occluding IMM where it is closer and IMM remaining visible where it is closer.

Unless a task says otherwise, the active goal for this document is the immediate ordered-overlay milestone for both Unity and Godot. Full depth compositing remains deferred work and cannot block completion of the ordered-overlay milestone.

## Ordered Overlay Completion Criteria

The ordered-overlay milestone is complete only when all of the following are true:

- Godot Vulkan ordered overlay has an automated CI lane or CI report section.
- Unity Vulkan ordered overlay has an automated CI lane or CI report section.
- Each ordered-overlay lane produces a capture image embedded in the final testing report.
- Each embedded image has been opened and visually inspected against the expected layout.
- Each image shows IMM picture/background content.
- Each image shows IMM paint content where expected by the fixture.
- Each image shows host-engine content visibly above IMM by render order.
- Neither image is blank, single-color, host-only, IMM-only, flipped, or visibly mis-composited.
- Each status JSON reports `composition_mode=ordered_overlay`.
- Each status JSON reports `composition_contract=ordered_overlay`.
- Each status JSON reports `ordered_overlay=success`.
- Each status JSON reports `depth_composition=not_claimed`.
- Each status JSON reports `depth_interleaving=not_claimed`.
- Unity DirectX composition CI passes as a regression guard.
- Existing Godot Vulkan alpha/depth CI lanes keep their current expected behavior.
- Any shared renderer, player, or shader change has been validated on affected hosts or explicitly gated at the host boundary.
- The accepted implementation does not depend on CPU readback, screenshot compositing, per-frame CPU texture upload, forced MSAA disablement, or an unprofiled full-frame copy path.
- Mobile-class performance risk has been assessed for the accepted approach, either with measurement or with a concrete bandwidth/synchronization analysis tied to the actual implementation.

If any item above is missing, failed, or only indirectly inferred from logs or pixel counts, the ordered-overlay milestone is not complete.

## Full Depth Completion Criteria

The full depth milestone is complete only when all of the following are true:

- Godot and Unity both have automated CI or runtime validation that explicitly tests depth interleaving.
- Host geometry correctly occludes IMM where the host geometry is closer.
- IMM correctly remains visible where IMM content is closer than host geometry.
- Captures for both engines are embedded in the final testing report and visually inspected.
- Each status JSON reports `composition_mode=full_depth`.
- Each status JSON reports `depth_composition=success`.
- Each status JSON reports `depth_interleaving=success`.
- Ordered-overlay evidence is not reused as proof of full depth compositing.
- XR/multiview, Metal, Android Vulkan/OpenXR, and Android GLES impacts are either validated or explicitly documented as unsupported/deferred for the full-depth milestone.

Full depth compositing is intentionally deferred while the ordered-overlay milestone is active.

## Evidence Rules

Evidence must be visual and contract-specific.

- A log saying draw calls ran is not enough.
- Nonblank pixel counts are not enough.
- Color bucket counts are not enough.
- A host-depth image is not ordered-overlay evidence.
- A no-MSAA diagnostic image is not normal Unity Vulkan ordered-overlay evidence.
- A local screenshot is useful during development, but CI evidence is required for completion.
- Failed captures must remain visible in the final testing report when a lane fails, because they are the fastest way to diagnose whether the failure is rendering, composition, orientation, or fixture setup.

## Goal

Bring the Godot IMM compositor behavior to parity with the Unity plugin scene integration while keeping shared renderer behavior correct across all hosts and platforms.

Unity currently renders IMM from the active Unity camera during the camera render path, so IMM content is composed into the same scene frame as Unity geometry. The Godot path uses a `CompositorEffect` that renders IMM into an intermediate texture and composites that texture back over Godot's color buffer. The Vulkan non-XR path now alpha-blends that intermediate and can use Godot scene depth for host-depth composition; Metal, XR/multiview, and Unity/Android runtime validation remain separate work items.

The desired Godot behavior is:

- Godot scene color remains visible behind transparent IMM pixels.
- IMM opacity and coverage blend correctly over the Godot scene.
- IMM content can be depth-tested against Godot scene depth where the renderer and Godot APIs expose the needed depth resource.
- A useful intermediate mode can compose host Unity/Godot elements over IMM by render order only, with no host-depth testing. This is a milestone, not final depth parity.
- The compositor works for Vulkan first, then Metal if the same resource access is available.
- Existing standalone, Unity, and Godot smoke paths keep their current behavior unless explicitly covered by the new compositor path.

## Milestone Agreement

We agreed to split compositor parity into two separate goals:

- Immediate goal: ordered overlay/background composition for both Unity and Godot. IMM renders as the background or lower ordered layer, and normal Unity/Godot content renders visibly above it by host render order. Host scene depth is not used and is not claimed.
- Deferred goal: full depth compositing for Unity and Godot. IMM and host geometry interleave by depth, with host geometry correctly occluding IMM where appropriate and IMM remaining visible where it is in front.

Evidence and CI reports must state which goal is being tested. A passing ordered-overlay result must use `composition_mode=ordered_overlay`, must not imply depth interleaving, and must not be used as full-depth parity evidence. A full-depth result must use `composition_mode=full_depth`; known full-depth gaps can remain `expected_failed` while the ordered-overlay milestone is being implemented.

The scope is not Godot-only once a fix touches shared IMM renderer, player, or shader code. Any change under `libImmCore`, `libImmPlayer`, or shared generated shaders must be treated as affecting all hosts that use that backend, including the Unity plugin on Vulkan. Godot-specific behavior must be gated at the Godot host boundary or by an explicit renderer capability/state that is only true for the relevant host integration path.

The platform scope for shared-code changes is:

- Godot desktop Vulkan, including non-XR compositor alpha and depth composition.
- Godot desktop Metal where equivalent Godot resource access exists.
- Godot XR / multiview when compositor view-layer support is enabled.
- Unity desktop Vulkan and Unity native plugin builds.
- Unity Android Vulkan/OpenXR and GLES paths when shared player or shader code is touched.
- Standalone IMM viewer Vulkan smoke paths.
- Existing non-Vulkan backends, which must remain unchanged unless intentionally modified.

The working assumption must be that a shared renderer or generated-shader change can affect more than Godot until a build, smoke, or explicit state gate proves otherwise.

## Full Depth Platform Disposition

For `FULL_DEPTH_COMPOSITOR_GOAL.md`, the current full-depth completion claim is intentionally limited to desktop non-XR Windows Vulkan unless a later validation row replaces this table. Other platform impacts are explicitly scoped as follows:

| Platform / Host | Full-depth status for this milestone | Required evidence before promotion |
| --- | --- | --- |
| Unity Windows Vulkan non-XR, persistent camera target | In scope. Local smoke evidence exists and CI lane code exists. | CI run artifact with embedded PNG, `composition_mode=full_depth`, `depth_composition=success`, `depth_interleaving=success`, and managed/native host-depth log contracts. |
| Unity Windows Vulkan display backbuffer | Partially validated locally. A rebuilt visible-window display capture now passes the scene-probe contract when the smoke harness freezes IMM playback and the native path is allowed to assume Unity's display depth attachment. Unity still reports null display render-buffer pointers, so this is not yet the final production handoff. | CI artifact or repeated local evidence with embedded PNG/status, `composition_mode=full_depth`, `depth_composition=success`, `depth_interleaving=success`, host-depth native markers, and a documented production answer for Unity's null display render-buffer pointers rather than relying on an unguarded assumption. |
| Unity Android Vulkan/OpenXR | Deferred for this full-depth milestone. Unity owns OpenXR swapchains and the current branch has no device runtime proof for Vulkan OpenXR eye targets. | Device run or hardware-gated CI artifact proving OpenXR starts, Vulkan is active, IMM renders into eye targets, and full-depth probes pass or a separate supported/unsupported product decision. |
| Android GLES | Deferred as a runtime validation target. GLES does not use the Vulkan host-depth shader changes directly, but shared player/native changes still require build/runtime guard evidence before broad parity claims. | Android GLES runtime smoke or explicit product decision that GLES is outside full-depth parity for this milestone. Build-only evidence remains insufficient. |
| Godot Windows Vulkan non-XR | In scope. Local scene-probe evidence exists and the GPU CI lane runs the full-depth smoke as the default composition mode. | CI artifact with embedded PNG and status fields reporting full-depth success. |
| Godot Metal | Deferred. The current Metal path has external render-pass entry points, but no local or CI runtime proof that Godot exposes a compatible color/depth resource contract for this compositor path. | macOS Metal runtime smoke with equivalent full-depth scene probes, or a documented platform limitation after inspecting Godot Metal resource access. |
| Godot XR / multiview | Deferred. The current compositor records view count but the validated path is mono/non-XR and feeds `eye=0`. | XR/multiview runtime capture or hardware-gated validation proving each eye has correct color, depth, orientation, and layer/view selection. |

Deferred here means "not claimed as implemented or validated by this milestone." It does not mean the code is known safe on that platform. Any shared renderer, player, or shader change still needs either a platform guard, a build/runtime check, or a documented product decision before release.

## Full Depth Performance Assessment

The accepted Windows desktop Vulkan full-depth paths must keep GPU work on the GPU and must not depend on CPU readback, screenshot compositing, per-frame CPU texture upload, forced MSAA disablement, or an unprofiled full-frame copy path.

Current assessment:

- Unity Windows Vulkan full-depth smoke uses Unity's active host render pass and normal 8x MSAA sample count. It does not use CPU readback to compose pixels, does not upload a screenshot back to the GPU, and does not force MSAA off. A visible-window display-backbuffer diagnostic passes locally after freezing IMM playback. Unity's display `RenderBuffer` native pointers are still null, so the current display path uses the explicit `IMM_UNITY_VK_HOST_RENDER_PASS_HAS_DEPTH=1` contract: IMM records into Unity's current Vulkan render pass and treats the active render pass as depth-capable without separately owning or sampling a depth image. This is narrower than the old `IMM_UNITY_VK_ASSUME_HOST_DEPTH=1` diagnostic alias, but it still needs CI artifact proof and performance evidence before broad release.
- Godot Windows Vulkan full-depth smoke renders IMM into a Godot-owned intermediate texture and composites it through the compositor path while using Godot scene depth for host-depth composition. This is GPU-local; the CI/local PPM/PNG capture happens after rendering for validation only and is not part of the compositor implementation. The path adds an intermediate color target and a fullscreen composite pass, so mobile/tile-GPU performance cannot be inferred from desktop correctness.
- Shared Vulkan picture-backdrop and depth-test changes are gated by explicit host-depth/external-frame state rather than by `API == Vulkan`, limiting the performance and behavior change to frames that opt into host-depth composition.

Implementation-specific synchronization and bandwidth analysis for the current Windows desktop evidence:

- Unity host-render-pass path: `BeginHostRenderPassFrame(...)` replaces the IMM renderer command buffer with Unity's active Vulkan command buffer and marks `hostRenderPassFrameActive=true`. The paint and picture draw helpers check that flag and, in host-render-pass mode, emit `vkCmd*` draw commands and return before the standalone-path `vkQueueSubmit(...)` and `vkWaitForFences(...)` blocks. Therefore the accepted Unity Windows Vulkan evidence is not using plugin-side per-draw queue submits or per-draw CPU/GPU waits for composition. It also does not allocate an IMM color intermediate, perform a fullscreen Unity blit, or force MSAA off. The remaining Unity risk is validating this same command-buffer contract in CI and on any later mobile/XR promotion target.
- Godot Vulkan render-graph path: the current full-depth path uses a Godot-owned intermediate color texture and an intermediate IMM depth texture, then runs one `RenderingDevice` fullscreen draw list that samples IMM color, IMM depth, and Godot scene depth before writing surviving IMM pixels to Godot scene color. There is no compositor CPU readback unless `IMM_GODOT_TRACE_INTERMEDIATE_TEXTURE` is explicitly enabled for diagnostics. At 1280x720 with 32-bit color/depth formats, the composite pass touches roughly 3.5 MiB IMM color + 3.5 MiB IMM depth + 3.5 MiB scene depth plus up to 3.5 MiB color output, about 14 MiB/frame worst-case for the fullscreen composite, excluding normal IMM rendering and driver/tile overhead. That is acceptable as a desktop validation path, but not sufficient proof for mobile/tile GPUs.
- Godot direct Vulkan color-target mode remains a separate possible optimization path, but the current accepted Godot full-depth evidence uses the render-graph intermediate path because it has the validated depth-aware final composite.
- No accepted full-depth evidence depends on CPU screenshot compositing, per-frame CPU texture upload, forced no-MSAA, or an unprofiled Unity full-frame copy/blit path. Local/CI captures and PNG/PPM conversions are validation outputs after rendering, not part of runtime composition.

Concrete follow-up performance evidence required before broad release:

- Unity: CI artifact proof of the named host-render-pass depth contract, plus GPU timing around the IMM host render pass before promoting beyond Windows desktop non-XR.
- Godot: GPU timing for the intermediate render plus fullscreen composite at 1280x720 and target mobile resolution, or RenderDoc/AGI evidence showing expected render-target load/store behavior and no CPU/GPU synchronization point.
- Android/mobile: either device timing for the selected Android graphics path or a documented product decision that Android full-depth parity is deferred.

## Current State

Relevant files:

- `code/appImmGodotGDExtension/src/imm_viewer_compositor_effect.cpp`
- `code/appImmGodotGDExtension/src/imm_viewer_compositor_effect.h`
- `code/appImmGodotGDExtension/src/imm_viewer_vulkan_frame.cpp`
- `code/appImmGodotGDExtension/src/imm_viewer_metal_frame.cpp`
- `code/appImmGodot/src/imm_godot_plugin.*`
- `code/libImmPlayer/src/player.cpp`
- `code/ImmGodotSampleProject/scenes/SampleScene.tscn`
- `code/ImmGodotSampleProject/scripts/smoke_test_runner.gd`
- `code/ImmGodotSampleProject/scripts/visual_smoke_controller.gd`

The Godot compositor currently:

- Registers `ImmViewerCompositorEffect`.
- Runs at `CompositorEffect::EFFECT_CALLBACK_TYPE_POST_TRANSPARENT`.
- Gets the current Godot color texture from `RenderSceneBuffersRD`.
- Creates an intermediate texture matching the color format.
- Starts a native IMM Vulkan or Metal texture frame against the intermediate texture.
- Calls `ImmGodot_RenderCamera(...)`.
- Draws the intermediate texture back to the Godot color texture.

The final draw currently:

- Uses a fullscreen shader that outputs only `texture(source_color, uv)`.
- Does not sample Godot scene color.
- Uses alpha blending for the default parity path.
- Has depth test/write disabled.
- Relies on native host-depth testing for the Vulkan depth-composite path.

This preserves a compositor hook while allowing Unity-style scene composition for the locally validated Godot Vulkan non-XR path.

Current validation status:

- Earlier global pixel-count checks were insufficient. They could pass while the rendered image was visibly wrong.
- A valid depth-composite test must compare the rendered image against expected spatial composition, not only count that some IMM and some occluder-colored pixels exist.
- The non-XR Godot depth output is accepted only when a capture shows paint, 360 backdrop, and opaque Godot geometry occupying the expected regions with correct depth ordering.
- The current checked depth smoke uses spatial validation of the occluder region, not just global color counts, and has been visually inspected against the expected composition.
- Unity desktop Vulkan ordered overlay/background now has local evidence for the explicit overlay-camera route. The earlier `transientubo-*` normal SampleScene MSAA captures show IMM picture and paint are no longer missing, but those two 8x Vulkan overlay images have visible depth/order problems and are failed evidence, not acceptance evidence. The later host-depth MSAA candidate captures are visually better and no longer blank, but they use host depth and do not by themselves prove the no-host-depth ordered-overlay milestone.
- The Unity editor smoke now rejects blank/single-color captures even when the native runtime initializes, so the previous false-positive "passed" state should not be used as evidence.
- Unity Vulkan diagnostics from `2026-06-10` through `2026-06-12` show the normal Game camera custom-blit destination as an 8x MSAA color render buffer in the command-buffer path. Earlier query-only tests around `CommandRecordingState` were not sufficient to prove the final target, because the editor smoke harness could previously exit on native load/audio completion before proving pixels.
- The editor smoke harness now requires a real capture whenever `IMM_UNITY_EDITOR_SMOKE_CAPTURE_PATH` is set. With that fixed harness, `IMM_UNITY_VK_SKIP_HOST_RENDER=1` captures the expected Unity sky/ground baseline.
- With the fixed harness, a Vulkan host-render-pass magenta-clear diagnostic proves that Unity's Vulkan plugin API can write visibly when issued late: `WaitForEndOfFrame` event 1 and command-buffer `CameraEvent.AfterEverything` both produce fully magenta captures.
- The same magenta-clear diagnostic fails for earlier camera-scoped events needed for ordered background composition: `AfterSkybox`, `AfterForwardOpaque`, `BeforeImageEffectsOpaque`, and the existing `BeforeForwardOpaque` path all collapse to a flat dark/single-color capture. Disabling `ModifiesCommandBuffersState` did not change the `AfterSkybox` failure.
- Filtering the Unity diagnostic to Game cameras only, excluding the editor `SceneCamera`, does not change the `AfterSkybox` failure. The scene camera was not the cause of the dark capture.
- `AfterSkybox` also fails through Unity's `IssuePluginCustomBlit` path with a non-null destination render buffer and a valid recording state. The failure is not specific to plain `IssuePluginEvent`.
- Skipping the separate Vulkan prepare render event changes the `AfterSkybox` magenta-clear diagnostic from dark/single-color to visibly magenta. That means the old render-thread prepare event was corrupting or invalidating the early camera path before the host render event ran.
- Moving prepare into managed `OnCameraPreCull` through `PrepareCamera(cameraID)` restores IMM draw calls, but the `AfterSkybox` capture becomes dark/single-color again. The remaining failure is therefore inside `PrepareCamera` / `Player::GlobalRender`, not in the minimal clear or command-buffer event itself.
- `Player::GlobalRender` is not CPU-only. It updates shader buffers, attaches shader constants, runs per-layer display preparation, and calls `Document::UpdateStateGPU(...)` and display pre-render hooks. Running that path before Unity continues the camera can still mutate Vulkan renderer/GPU state outside the host render event.
- Follow-up diagnostics corrected part of that interpretation: when the sample scene reaches `Loaded`, its serialized example component applies the IMM document background color to the camera and switches it to `SolidColor`. Several black captures were caused by that sample-scene camera behavior, not by Vulkan draw failure. The smoke now has an env-only bypass, `IMM_UNITY_SKIP_DOCUMENT_BACKGROUND_COLOR=1`, to isolate renderer behavior without editing `SampleScene.unity`.
- With the background-color bypass, `AfterSkybox`/`AfterEverything` host render produces visible IMM pixels. `BeforeForwardOpaque` records IMM draw calls but the final frame is still the Unity-only baseline, so that event is overwritten later by the camera pipeline and does not currently prove ordered background composition.
- A same-command-buffer diagnostic now proves one useful ordered-overlay primitive: after the Vulkan plugin event renders IMM, Unity can draw a red command-buffer quad over it in the same camera command buffer without a CPU readback/copy path. Evidence: `artifacts/unity-vulkan-overlay/smoke-commandbuffer-overlay-fixture-aftereverything-clipquad-newinstance.png` shows IMM background plus the red Unity overlay quad. This is only a diagnostic primitive, not yet a general scene overlay implementation.
- Two-camera overlay diagnostics now pass when the IMM manager is explicitly restricted to the base camera and a later Unity overlay camera renders host elements. This proves the ordered-overlay milestone route for Unity Windows Vulkan MSAA without host-depth interleaving. It does not prove same-camera arbitrary Unity geometry interleaving.
- Paint absence in Unity Vulkan mono was isolated on `2026-06-12`: with both picture and paint enabled the capture showed only the blurred 360 picture; with `IMM_RENDER_SKIP_PICTURE=1`, paint became visible; with `IMM_RENDER_SKIP_PAINT=1`, the blurred 360 picture remained. Evidence captures:
  - `artifacts/unity-vulkan-overlay/paint-isolation-normal-no-overlay.png`
  - `artifacts/unity-vulkan-overlay/paint-isolation-skip-picture-no-overlay.png`
  - `artifacts/unity-vulkan-overlay/paint-isolation-skip-paint-no-overlay.png`
- Source inspection explains that first paint failure: `Player::RenderMono` still used paint-before-picture order, while Vulkan stereo paths already use picture-before-paint. Reordering Vulkan mono to picture-before-paint exposes paint, but it is not a complete fix.
- After the Vulkan mono order change, paint is visible but the blurred 360 picture is no longer preserved when paint runs. Picture-only still works, and paint-only still works. Evidence captures:
  - `artifacts/unity-vulkan-overlay/background-depth-fix-after-everything-skip-paint-picture-only.png` shows the blurred 360 picture.
  - `artifacts/unity-vulkan-overlay/background-depth-fix-after-everything-normal-no-overlay.png` shows paint over Unity sky/ground instead of the 360 picture.
  - `artifacts/unity-vulkan-overlay/chunk-filter-contact-sheet.png` shows that even paint chunk filters with little or no visible paint still lose the 360 picture once a static paint chunk path runs.
  - `artifacts/unity-vulkan-overlay/paint-layer-filter-none-aftereverything.png` preserves the 360 picture when the paint renderer runs but all paint layers are filtered out before upload/draw.
- The Unity Vulkan paint/picture boundary failure was caused by mutating shared Vulkan uniform buffers after draw commands had been recorded into Unity's command buffer but before Unity executed that command buffer. Picture draws could therefore read later paint constants. The current fix allocates host-frame transient uniform slices for constant-buffer updates while recording into Unity's Vulkan render pass.
- Current inspected evidence:
  - `artifacts/unity-vulkan-overlay/transientubo-afterskybox-msaa8-clear-then-normal.png` is a failed 8x Vulkan overlay capture. Picture and paint are present, but depth/order is visibly wrong.
  - `artifacts/unity-vulkan-overlay/transientubo-afterskybox-msaa8-normal.png` is also a failed 8x Vulkan overlay capture with visible depth/order problems.
  - `artifacts/unity-vulkan-overlay/transientubo-no-msaa-aftereverything-normal.png` is useful 1x Vulkan host-depth diagnostic evidence only. It does not prove the normal MSAA overlay milestone.
  - `artifacts/unity-vulkan-overlay/regression-d3d11-normal.png` shows the D3D11 Unity sample still renders picture, paint, and Unity geometry, but the image is Y-flipped and must be tracked as a separate regression or capture-orientation problem.
- 2026-06-13 follow-up:
  - D3D11 Game-camera projection was changed back to the backbuffer projection path for non-XR desktop Game cameras. `artifacts/unity-vulkan-overlay/candidate-d3d11-default-projection.png` is upright.
  - Unity Vulkan now uses `AfterSkybox`, binds the camera target, and uses a plain plugin event by default. The previous default `BeforeForwardOpaque` path could record native draw calls while producing a blank capture.
  - Unity Vulkan host-depth use was changed to opt-in with `IMM_UNITY_VK_USE_HOST_DEPTH=1`. The default Unity Vulkan path is now the ordered-overlay/background path: draw IMM without using Unity depth, then put Unity overlay content above it by render order.
  - The old red cube fixture failure was caused by the fixture switching the main camera to `SolidColor`. Because the plugin is attached at `AfterSkybox`, that prevented the plugin event from firing. The fixture now preserves the camera clear mode by default and only uses solid clear when `IMM_UNITY_EDITOR_OVERLAY_FIXTURE_SOLID_CLEAR=1` is set.
  - `artifacts/testing-matrix/unity-vulkan-second-camera-overlay-visible-editor-upright/unity-vulkan-composition.png` is the current inspected Unity Vulkan MSAA evidence for the ordered-overlay milestone. It uses the new defaults, an explicit `ImmPlayerManager` render camera filter, and a second Unity overlay camera. The image shows IMM picture/paint plus Unity overlay probes and a red Unity cube composited in front, saved in the corrected upright PNG orientation. The editor smoke reports `scene composition overlay probe passed`; the native log reports `rendered=1 drawCalls=36 paintDrawCalls=35 pictureDrawCalls=1 picture360DrawCalls=1`. This proves the practical overlay-camera route, not full host-depth interleaving.
  - `artifacts/unity-vulkan-overlay-regression/candidate-vulkan-msaa8-before-forward-no-host-depth.png` is a failed `BeforeForwardOpaque` control: native IMM draw calls were logged, but the final capture is Unity sky/ground only. That event is overwritten later in the current built-in pipeline and must not be treated as overlay evidence.
  - `artifacts/unity-vulkan-overlay-regression/candidate-d3d11-force-after-latest.png` is the current real D3D11 regression capture. The Unity log for that run includes `Forcing GfxDevice: Direct3D 11` and `Version: Direct3D 11.0`; the inspected capture is upright.
- Correctness without acceptable performance is not an acceptable final state. Do not promote an intermediate texture copy, forced resolve, or fullscreen overlay/composite path to the Unity Vulkan solution unless profiling and architecture show it is competitive with the existing GL/DX/Metal-style integration, including on mobile/tile GPUs. Such paths may be used only as diagnostics or explicitly documented fallbacks, not as parity completion evidence.
- The current Unity Vulkan prototype still uses `piRendererVulkan`'s owned-command-buffer draw helpers. Those helpers begin/end a native render pass and submit/wait their own Vulkan command buffer per draw helper. That may be tolerable for standalone smoke infrastructure, but it is not a credible final Unity scene-integration path. Unity Vulkan parity requires recording the IMM draw work into Unity's render context, or an equivalently efficient host-command-buffer integration, rather than adding per-draw queue submits from the plugin.

## Code-Backed Facts

These are verified from the current source and should guide the work.

- Unity and Godot both use `ImmShared::ImmEngineBridge::RenderCamera(...)`.
- Unity injects rendering through `ImmPlayerManager` command buffers / camera callbacks and native `iOnRenderEvent(...)`.
- Unity then calls `gImmUnityPlugin.mBridge.RenderCamera(cameraID, viewport, eyeID, true)` from the render event.
- Godot calls the same bridge path through `ImmGodot_RenderCamera(...)`.
- Godot already has external native frame entry points:
  - `ImmGodot_BeginVulkanFrame(...)`
  - `ImmGodot_EndVulkanFrame()`
  - `ImmGodot_BeginMetalFrame(...)`
  - `ImmGodot_EndMetalFrame()`
- Godot Vulkan already passes Godot's Vulkan instance, physical device, device, queue, queue family, color image, color image view, and color format into IMM.
- The Vulkan native renderer already has `BeginExternalImageFrame(...)`, which creates a color texture wrapper, creates its own `D1_32_FLOAT` depth texture, creates a render target from those, and sets it before IMM render.
- Therefore the core IMM renderer does not need to be taught how to render scene-integrated IMM content from scratch. The renderer already does the IMM draw work. The parity gap is in how the Godot host supplies targets and how the final Godot frame is composed.
- The current Godot compositor renders IMM into an intermediate color texture, then alpha-blends that texture over the Godot scene color unless explicit replacement mode is selected.
- The current Godot Vulkan external frame struct has optional depth image/depth image view/depth format fields.
- The current Godot compositor retrieves Godot scene depth from `RenderSceneBuffersRD` for the Vulkan depth-composite path and passes it to native IMM when available.
- Shared Vulkan player render order and shared Vulkan picture shader changes affect Unity Vulkan unless they are explicitly gated. They must not be used as a Godot-only shortcut.

## Target Architecture

Use a host-integration pipeline based on the existing shared IMM render path:

1. Keep using `ImmGodot_RenderCamera(...)` / `ImmEngineBridge::RenderCamera(...)`.
2. Make Godot's compositor preserve the existing Godot scene color when IMM renders transparent pixels.
3. Extend the Godot host frame boundary to provide Godot scene depth to IMM or provide enough IMM output depth for a correct post composite.

The first implementation now covers correct color/alpha composition and Godot Vulkan non-XR depth composition. Remaining work should focus on validation gaps, XR/multiview behavior, Metal feasibility, and runtime coverage for hosts affected by shared Vulkan changes.

Next standalone milestone:

- Add and validate an "ordered overlay/background" composition mode as its own milestone before full depth parity.
- In this mode, IMM is composed as a background or ordered scene layer and Unity/Godot elements render over it by host render order.
- Host depth is not sampled, attached, or used for the correctness claim.
- This milestone is valuable only if it works with normal platform configuration, including normal Unity Vulkan MSAA, and avoids avoidable full-frame copies/resolves.
- Passing this milestone does not prove that IMM can interleave with host geometry by depth. It only proves useful ordered composition.

Shared-code rule:

- Prefer fixing incorrect cross-platform behavior once if the same bug exists in multiple hosts.
- If a behavior change is only valid for Godot's compositor path, implement it through Godot-owned configuration or a host-depth/external-frame state that cannot silently change Unity, standalone, Android GLES, or XR behavior.
- If a Vulkan change is made in shared code, validate or explicitly reason about Unity Vulkan in the same change. Do not assume Unity is unaffected.
- Do not key behavior on `API == Vulkan` when the real condition is host-supplied external depth, Godot compositor mode, XR multiview, or another narrower state.
- Generated shader changes must have the same scoping as the C++ path that enables them. If a shader behavior is only needed for host-depth composition, use a specialization constant or equivalent gated state instead of changing the default shader output for every Vulkan host.

## Phase 1: Alpha Composite Parity

### 1. Preserve Godot Scene Color

Change `composite_texture_to_color(...)` so the Godot scene color is preserved and IMM is blended over it.

Implementation options:

- Preferred: enable render pipeline alpha blending and draw the IMM texture into the color framebuffer.
- Alternative: sample both the scene color and IMM texture in the fragment shader and write `mix(scene, imm, imm.a)`.

The preferred path is simpler and avoids read/write hazards from sampling the same color texture being rendered to. It requires the IMM intermediate texture to contain meaningful alpha.

Expected blend state:

- Color: `src = SRC_ALPHA`, `dst = ONE_MINUS_SRC_ALPHA`, op `ADD`
- Alpha: `src = ONE`, `dst = ONE_MINUS_SRC_ALPHA`, op `ADD`

If the IMM renderer writes premultiplied alpha, use:

- Color: `src = ONE`, `dst = ONE_MINUS_SRC_ALPHA`, op `ADD`
- Alpha: `src = ONE`, `dst = ONE_MINUS_SRC_ALPHA`, op `ADD`

The native Vulkan blend implementation already uses the standard straight-alpha factors when blending is enabled:

- Color: `SRC_ALPHA`, `ONE_MINUS_SRC_ALPHA`
- Alpha: `ONE`, `ONE_MINUS_SRC_ALPHA`

Use the same convention for Godot's compositor blend pipeline unless a fixture proves the intermediate texture is premultiplied.

### 2. Clear Intermediate Texture Correctly

Before `ImmGodot_RenderCamera(...)`, clear the intermediate texture to transparent black.

Without this, pixels not touched by IMM may contain undefined data or stale frame data. That would make alpha compositing unreliable. The existing Godot path creates/reuses `_intermediate_texture`; add an explicit clear before `ImmGodot_RenderCamera(...)`.

Add diagnostics for:

- Intermediate clear success.
- Intermediate format.
- Whether the alpha channel appears nonzero for rendered pixels.
- Whether untouched pixels remain transparent.

### 3. Add Explicit Composite Modes

Add a compositor mode enum to `ImmViewerCompositorEffect` and expose it through `ImmViewerNode` if useful:

- `Replace`: existing behavior, useful for debugging.
- `AlphaBlend`: default target behavior.
- `DepthComposite`: future phase.

Keep `Replace` available temporarily so regressions can be isolated.

### 4. Update Diagnostics

Extend `ImmViewerCompositorEffect::get_diagnostics()` with:

- `last_composite_mode`
- `last_intermediate_clear_result`
- `last_blend_enabled`
- `last_source_alpha_mode`
- `last_color_blend_pipeline_valid`
- `last_replace_pipeline_valid`

The diagnostics should make it obvious whether the compositor is running the new parity path or falling back to the old replacement path.

### 5. Smoke Test Alpha Blending

Add a Godot visual smoke scene or extend the current one:

- Render a saturated Godot background or simple colored geometry.
- Render IMM content with transparent areas.
- Capture output.
- Assert that both Godot background pixels and IMM pixels are visible.

The test should fail if IMM replaces the whole frame or if IMM disappears.

For automated image checks:

- Sample known background-only pixels.
- Sample known IMM-covered pixels.
- Sample partially transparent/edge pixels if the fixture is stable.

Use file-based logs for Godot diagnostics, not console-only logs.

## Milestone 2: Ordered Overlay / Background Composition

This is a deliberately scoped standalone milestone. It makes IMM useful in Unity and Godot scenes where host objects, controls, or UI are meant to appear over IMM, without requiring depth-aware interleaving between IMM and host geometry. It should be tracked, implemented, validated, and accepted separately from full depth parity.

Milestone status:

- Defined, not yet complete.
- This is the immediate compositor milestone for both Unity and Godot.
- Godot Vulkan has alpha/depth compositor evidence, but the ordered overlay fixture still needs its own explicit smoke and visual inspection before Godot can be counted as passing this milestone.
- Unity desktop Vulkan ordered overlay still needs passing automated evidence in the final CI report; current failed Vulkan overlay images disprove completion.
- The current Unity Vulkan host-command-buffer prototype can visibly write at `AfterEverything`, but that is too late for IMM-as-background with Unity elements over it.
- Earlier useful render-order events (`AfterSkybox`, `AfterForwardOpaque`, `BeforeImageEffectsOpaque`, `BeforeForwardOpaque`) still produce blank/single-color captures even for a minimal magenta clear. That output is failure evidence, not a partial pass.
- Full depth compositing is deferred and should stay out of the acceptance criteria for this milestone except as an explicitly documented non-goal.

Performance contract:

- The ordered overlay/background milestone must be GPU-native. Correctness that depends on CPU readback, CPU compositing, per-frame texture upload, or screenshot-style transfer is not useful for production and must not be accepted as the milestone implementation.
- The target architecture is IMM rendering on the same host graphics device into a host-owned or explicitly shared GPU image/render target, followed by host-render-graph composition so Unity/Godot scene elements draw over IMM by render order.
- Direct host render integration is the default target. A single GPU fullscreen composite pass is only a candidate if it stays on-GPU, is synchronized through the host render path, and is measured on mobile-class hardware or analyzed from concrete bandwidth/store-load data as competitive with direct integration.
- An independent IMM swapchain, plugin-side present path, staging-buffer readback, or forced full-frame resolve/copy may be used only as a diagnostic to prove renderer output. It is not acceptable as the Unity/Godot overlay solution.
- The implementation must not be a "fudge" that only makes desktop Vulkan screenshots look correct. A production candidate has to preserve the mobile performance shape of the existing plugin integrations: GPU-local resources, host-owned synchronization, and no hidden frame-wide transfers.
- Mobile performance is a first-class acceptance requirement. The design must avoid extra CPU/GPU synchronization points, avoid per-draw queue submit/wait behavior from the plugin, and avoid unnecessary MSAA resolves on normal Unity/Godot mobile configurations.
- If an intermediate texture is required, it must be a GPU resource with stable ownership and lifetime. It must not become a hidden per-frame allocation, readback, or upload path.
- The implementation should preserve the same broad performance model as the existing GL/DX plugin paths: one host/plugin render integration point per camera frame, no per-stroke host round trips, no per-draw queue ownership transfers, and no frame-wide memory movement outside GPU-local resources.
- On mobile/tile GPUs, any extra fullscreen pass, resolve, layout transition, or render-target store/load must be explicitly justified. If it cannot plausibly fit normal mobile frame budgets, the route is a diagnostic or fallback only, not the milestone solution.
- Correct-but-slow approaches are not acceptable milestone completion. A candidate solution must include at least a basic performance validation plan before acceptance, such as GPU timing around the IMM pass/composite pass, frame-time comparison against the existing GL/DX path where available, or documented reasoning tied to actual draw count and memory bandwidth.
- Desktop correctness alone is not enough for this milestone. A route that would obviously increase mobile bandwidth or synchronization cost must not be accepted merely because the immediate validation machine is Windows desktop Vulkan.

Mobile performance decision:

- The intermediate overlay milestone is useful only if it keeps the same performance shape we would expect from the final integration. It is not a license to ship a slow copy/composite path while deferring performance to the later depth milestone.
- Treat the `feature/vulkan` branch as useful evidence for Vulkan initialization and IMM rendering, not as proof that overlay composition is acceptable.
- Prefer direct rendering into Unity's active render context, or a tightly equivalent host-owned command-buffer path, because this keeps MSAA resolve, render ordering, and synchronization inside Unity's normal frame.
- The preferred Unity Vulkan route is therefore: record IMM background-layer work into Unity's render pass or a host-owned command buffer at the correct camera event, then let Unity continue drawing host elements over it. That is the route most likely to match GL/DX/Metal-style performance on mobile.
- Do not promote a separate Unity `RenderTexture` plus final blit/resolve as the default mobile route unless profiling shows that its bandwidth, render-target store/load cost, and synchronization cost are comparable to the existing GL/DX path on representative mobile hardware.
- A fullscreen GPU overlay pass may remain in the design only as an explicitly measured option. It is not automatically acceptable just because it is GPU-only.
- Any route that blocks Unity on plugin GPU work, waits on a plugin fence per frame, submits per IMM draw group, or forces the tile renderer to store and reload full-frame color unnecessarily should be considered a diagnostic/fallback until proven otherwise.
- If the only working approach requires a separate render target plus final fullscreen composite, the milestone remains incomplete until that path has mobile-relevant profiling showing acceptable cost. Correct pixels alone are insufficient.
- Before this milestone is called production-ready, collect at least desktop GPU timing plus one mobile-oriented validation item: Android Vulkan device timing, RenderDoc/AGI evidence of render-pass/load/store behavior, or a written bandwidth/synchronization analysis tied to the actual target resolution and MSAA setting.
- If the direct Unity render-pass/command-buffer path remains blocked, the next acceptable fallback investigation is not "make a copy path look correct"; it is to find the lowest-cost host-owned GPU composition path and prove the cost. That proof must include synchronization behavior, MSAA behavior, and whether tile memory is forced to store/reload.
- A Unity `RenderTexture` overlay route can be kept as a diagnostic to compare against `feature/vulkan`, but it should be labeled non-production until it has mobile evidence. It must not become the default milestone route by inertia.

Target behavior:

- IMM renders as a background or ordered layer.
- Unity/Godot elements can be rendered in front of IMM by render order.
- Host depth is not considered.
- This behavior is required for both Unity and Godot before the ordered-overlay milestone is complete.
- IMM opacity and alpha still behave correctly.
- Normal platform settings are used; in particular Unity Vulkan must work with the normal SampleScene MSAA configuration.
- The implementation must not rely on a forced single-sample diagnostic path, CPU-visible copy chain, independent swapchain, or extra resolve as the claimed production route unless profiling proves the cost is acceptable on desktop and mobile.

Non-goals for this milestone:

- Host-depth interleaving between IMM and Unity/Godot geometry.
- Godot XR/multiview correctness.
- Godot Metal correctness unless the same ordered path is specifically validated there.
- Unity Android Vulkan/OpenXR runtime correctness unless shared code touched for this milestone reaches that path and requires a guard build or runtime check.
- Any claim that a diagnostic no-MSAA path represents production behavior.

Unity Vulkan requirements for this milestone:

- Record IMM draw commands into Unity's active Vulkan command buffer/render pass, or an equivalent host-owned render context, so Unity owns MSAA resolve, ordering, and synchronization.
- Do not use plugin-owned per-draw queue submits inside Unity's render path.
- Do not treat a plugin-owned queue render followed by a blocking semaphore/fence wait on Unity as an acceptable production route unless profiling proves it matches the GL/DX integration cost on desktop and mobile.
- Reusing the working render setup from `feature/vulkan` is allowed only where it helps restore Vulkan device/rendering behavior. The missing capability is ordered composition with Unity elements on top using Unity's normal render path; do not regress into an independent plugin-presented image or a copy chain to hide that gap.
- When comparing against `feature/vulkan`, separate "can initialize and draw IMM with Vulkan" from "can compose correctly and efficiently inside Unity's camera frame." Only the latter satisfies this milestone.
- Use render order to place IMM behind selected Unity elements.
- Add diagnostics that distinguish this ordered-overlay path from the full depth-composite path.
- The visual smoke must fail if the capture contains only host skybox/ground, only IMM, or a blank/single-color frame.
- The acceptance capture must use the normal SampleScene camera/settings unless a separate fixture is intentionally created and documented.
- The image must be opened and compared against the expected visual layout before the result is counted.

Godot requirements for this milestone:

- Provide an explicit mode or test fixture for ordered/background composition that does not claim host-depth correctness.
- Preserve host color where host elements are intended to overlay IMM.
- Keep the existing alpha/depth compositor modes available and separately validated.
- The visual smoke must include host geometry or UI visibly in front of IMM without relying on depth sampling.

Acceptance for this milestone:

- Unity desktop Vulkan normal SampleScene MSAA capture shows IMM content and host elements correctly ordered, with the image opened and inspected.
- Godot Vulkan capture shows IMM content and host elements correctly ordered, with the image opened and inspected.
- Automated checks verify nonblank output and expected regions for IMM-only, host-over-IMM, and background/transparent areas.
- Unity/Godot performance evidence shows the overlay path is GPU-native and does not introduce blocking readback/upload, per-draw queue waits, or avoidable full-frame resolves. Desktop evidence is required before acceptance; mobile evidence or a concrete mobile validation task is required before the route can be considered production-ready.
- The implementation path has a credible mobile performance story before it is accepted: no hidden frame-wide memory movement, no plugin-side per-draw queue synchronization, no forced MSAA downgrade, and no required store/load cycle that would be expected to regress tile GPUs.
- Documentation and logs state that host-depth interleaving is not covered by this milestone.
- The milestone can be accepted even though full depth parity, XR/multiview, Godot Metal, and Android runtime validation remain future work.

## Phase 2: Depth-Aware Composition

### 1. Retrieve Godot Depth Resource

Add code to retrieve depth from `RenderSceneBuffersRD` in the same callback that currently retrieves color. This should be done in code, not treated as an open design question.

Specific work:

- Find whether `get_depth_texture(...)`, named depth buffers, or texture lookups expose the scene depth RID.
- Record the depth format and projection convention.
- Confirm whether the depth texture is readable in the compositor callback at `POST_TRANSPARENT`.

Add diagnostics:

- `last_had_depth_texture`
- `last_depth_texture_handle`
- `last_depth_format`
- `last_depth_size`

### 2. Match Projection And Depth Conventions

Unity already passes camera matrices into native IMM rendering and has platform-specific projection handling. Godot must do the same with the correct conventions:

- Vulkan and Metal use zero-to-one depth.
- OpenGL/GLES use negative-one-to-one depth where applicable.
- Godot camera transform handedness and projection must match the native IMM player expectations.

Do not reuse Unity projection assumptions blindly. Add a Godot-specific note in code near `make_perspective_projection(...)` explaining the depth convention and renderer API branch.

### 3. Extend The Godot External Frame ABI

The current `ImmGodotVulkanFrame` contains color:

- `colorImage`
- `colorImageView`
- `colorFormat`

And optional depth fields:

- `depthImage`
- `depthImageView`
- `depthFormat`

Keep the struct versioned. Either bump `version` or add a `size` field for any future ABI expansion so older callers fail clearly or remain compatible.

For Metal, extend `ImmGodotMetalFrame` only if Godot can provide a render pass descriptor or depth attachment compatible with the existing `BeginExternal...Frame` methods. The current Metal path already has external render pass entry points; do not invent a separate Metal renderer path unless the existing one cannot represent Godot's render pass.

### 4. Use Godot Depth In The Native External Render Target

The native Vulkan external-image implementation creates its own `D1_32_FLOAT` depth texture when no host depth is supplied. When a Godot depth image/view is supplied, the native renderer wraps that depth image and uses it in the external render target instead of creating a private depth buffer.

This keeps the existing IMM renderer path intact:

- Godot supplies color and depth attachments.
- `ImmGodot_BeginVulkanFrame(...)` begins an external frame using those attachments.
- `ImmGodot_RenderCamera(...)` calls the same shared bridge render code.
- IMM depth tests naturally run against the Godot scene depth.

If another Godot backend cannot attach its depth image for native rendering, then fall back to the two-texture post-composite approach:

- IMM renders color and depth into intermediate textures.
- Godot composite shader samples Godot depth and IMM depth.
- The shader writes IMM color only where IMM depth is in front.

This fallback is less direct than using Godot's depth as the native attachment and should not be the first choice.

### 5. Depth Test Smoke

Create a deterministic Godot test scene:

- A Godot opaque cube or wall in front of part of the IMM drawing.
- IMM content behind and in front of the Godot geometry.
- A fixed camera.

Automated checks:

- Pixels where Godot geometry should occlude IMM remain Godot-colored.
- Pixels where IMM is in front are IMM-colored.
- Background still shows where neither draws.
- The occluder region must be compared against a Godot-only control capture or an equivalent expected mask, so a tiny visible strip cannot satisfy the test.
- The captured image must be visually inspected before declaring the phase complete.

### 6. Shared Vulkan Host Impact

If the depth fix requires changing shared Vulkan render order, depth compare, picture-layer depth output, or generated SPIR-V:

- Determine whether Unity Vulkan and standalone Vulkan use the same path.
- Keep the default shared behavior unchanged unless the same bug is proven there.
- Gate host-depth-specific behavior on an explicit state such as "external frame uses host depth" rather than `API == Vulkan`.
- Add diagnostics that identify when the gated path is active.
- Run at least a Godot non-XR Vulkan capture and a non-Godot/shared smoke or build guard before considering the change ready.
- Treat Unity Vulkan as in scope unless the changed code path is demonstrably unreachable from Unity.
- For Android, preserve existing GLES paint state restoration and alpha-to-coverage behavior. Do not reuse Vulkan compositor fixes as GLES state fixes.
- The shared player render-order path must use an explicit renderer query such as `UsesExternalHostDepth()` rather than treating all Vulkan renderers as host-depth compositor frames.

## Phase 3: XR And Multi-View

The current compositor records `view_count` but renders one eye path through `ImmGodot_RenderCamera(..., eye=0, ...)`. The shared bridge already supports mono, stereo multipass, and stereo single-pass branches based on `camera.stereoType`, so the Godot work is to feed correct Godot XR matrices and target layers/views into the existing bridge.

For XR parity:

- Detect Godot multi-view target layout.
- Render each eye with the correct camera matrices.
- Composite into the correct array layer or view texture.
- Preserve per-eye projection and head pose.

Add diagnostics:

- `last_view_count`
- `last_rendered_eye_count`
- `last_eye_index`
- `last_view_layer`

Do this after mono alpha/depth behavior is stable.

## Phase 4: Regression Guards

Add verifier checks in `code/appImmGodotGDExtension/verify_local.py`:

- `ImmViewerCompositorEffect` must keep `POST_TRANSPARENT`.
- Composite code must expose `Replace` and `AlphaBlend` modes.
- Alpha blend mode must not disable blending.
- Intermediate texture must be cleared before render.
- Diagnostics must include composite mode and blend status.
- Depth smoke validation must include spatial/mask checks, not only global color counts.
- Shared Vulkan changes must not be keyed only on `GetAPI() == Vulkan` when the intended condition is Godot host-depth composition.

Add comments in `imm_viewer_compositor_effect.cpp` near the blend state:

- Explain that disabling blending returns the old replacement behavior.
- Explain that Unity parity requires preserving Godot scene color under transparent IMM pixels.
- Explain that depth parity is separate and must not be faked by fullscreen replacement.

## Phase 5: CI And Local Commands

Extend existing Godot smoke coverage:

- Desktop Vulkan visual smoke for alpha blend.
- Desktop Vulkan visual smoke for ordered overlay/background composition without host depth.
- Desktop Vulkan visual smoke for depth composition using a normal non-XR camera pose and `example1.imm`.
- Desktop Metal visual smoke if available on macOS runners/local machines.
- Unity Vulkan ordered overlay/background smoke using normal SampleScene MSAA settings.
- Unity Vulkan depth-composition smoke or an explicit build/runtime guard for any shared Vulkan renderer/player/shader change.
- Android Vulkan smoke only if device-gated infrastructure is available.

Keep tests explicit about renderer API. Avoid relying on `Auto` when checking compositor behavior.

Suggested local commands:

- `python code/appImmGodotGDExtension/verify_local.py`
- `pwsh -NoProfile -File code/projects/windows/build-unity-plugins.ps1 -Configuration Release`
- `pwsh -NoProfile -File code/appImmViewer/scripts/run-vulkan-sample1-smoke.ps1 -Configuration Release -DurationSeconds 45 -PresentSeconds 8`
- `IMM_UNITY_EDITOR_SMOKE_CAPTURE_PATH=<png> IMM_UNITY_SMOKE_FRAMES=360 "C:\Program Files\Unity\Hub\Editor\2022.3.62f2\Editor\Unity.exe" -projectPath code\ImmUnitySampleProject -force-vulkan -executeMethod ImmPlayer.Editor.BuildAutomation.RunWindowsVulkanEditorPlayModeSmoke -logFile <log>`
- `pwsh -NoProfile -File code/projects/windows/build-godot-extension.ps1 -Configuration Release`
- `pwsh -NoProfile -File code/projects/windows/run-godot-compositor-parity-smoke.ps1 -Configuration Release -GodotExe <path-to-godot> -Mode Alpha -SkipBuild -LogDir artifacts/godot-compositor-parity/final-alpha -TimeoutSeconds 90`
- `pwsh -NoProfile -File code/projects/windows/run-godot-compositor-parity-smoke.ps1 -Configuration Release -GodotExe <path-to-godot> -Mode Overlay -SkipBuild -LogDir artifacts/godot-compositor-parity/final-overlay -TimeoutSeconds 90`
- `pwsh -NoProfile -File code/projects/windows/run-godot-compositor-parity-smoke.ps1 -Configuration Release -GodotExe <path-to-godot> -Mode Depth -SkipBuild -LogDir artifacts/godot-compositor-parity/final-depth -TimeoutSeconds 90`

Run Godot alpha, overlay, and depth smokes serially. The scripts copy the extension DLLs, and concurrent Godot runs can hold those DLLs open.

## Current Validation Evidence

Last updated: 2026-06-12.

Validated locally after the final-composite UV fix:

- `python code/appImmGodotGDExtension/verify_local.py` passed after the Unity Vulkan guard change and now guards compositor mode, final-composite X/Y orientation, alpha clear/blend behavior, depth spatial checks, shared Vulkan host-depth scoping, and the explicit Unity Windows Vulkan unsupported path.
- `pwsh -NoProfile -File code/projects/windows/build-godot-extension.ps1 -Configuration Release` passed.
- `pwsh -NoProfile -File code/projects/windows/run-godot-compositor-parity-smoke.ps1 -Configuration Release -GodotExe "C:\Program Files\Godot_v4.6.1-stable_win64.exe\Godot_v4.6.1-stable_win64.exe" -Mode Depth -SkipBuild -LogDir artifacts/godot-compositor-parity/final-depth-clean -TimeoutSeconds 90` passed.
- The passing Godot Vulkan depth capture is `artifacts/godot-compositor-parity/final-depth-clean/godot-vulkan-depth.png`. It uses the normal scene camera pose (`last_camera_origin=(0.0, 1.6, 6.0)`) and reports observed red occluder bounds `x=664..956`, `y=237..716` against projected bounds `x=664..958`, `y=236..717`.
- `pwsh -NoProfile -File code/projects/windows/run-godot-compositor-parity-smoke.ps1 -Configuration Release -GodotExe "C:\Program Files\Godot_v4.6.1-stable_win64.exe\Godot_v4.6.1-stable_win64.exe" -Mode Alpha -SkipBuild -LogDir artifacts/godot-compositor-parity/fixed-alpha -TimeoutSeconds 90` passed.
- Windows Unity plugin Release build passed, including shared `libImmCore`, `libImmPlayer`, `appImmUnity`, and package DLL copy. Existing third-party Audio360 warnings remained, with no build errors.
- Standalone Vulkan sample1 smoke passed and captured `code/appImmViewer/exe/vulkan_sample1_smoke.ppm` with `nonblack=921148`, `nearVisible=555162`, and `maxRGB=255,255,255`.
- Unity Windows editor play-mode smoke was run with `-force-vulkan` using `ImmPlayer.Editor.BuildAutomation.RunWindowsVulkanEditorPlayModeSmoke`. The native plugin now initializes against Unity's Vulkan device and reaches IMM draw calls, but the normal SampleScene MSAA path is still invalid. The latest normal-MSAA capture `artifacts/unity-vulkan-smoke/windows-vulkan-editor-playmode-msaa-fallback-depth.png` failed with `colorBuckets=1`, `nonBlackRatio=0.0000`, and `maxLuma=2`, and the PNG is visually near-black.
- Unity Vulkan native diagnostics showed the normal Game camera custom-blit destination as `colorSamples=8` and `depthSamples=1`. After adding sample-count propagation and internal-depth fallback, the path no longer crashed and logged nonzero IMM draw calls, but the visible output remained black. That is runtime failure evidence, not a pass.
- Unity Vulkan `CommandRecordingState` diagnostics on 2026-06-12 narrowed the current failure boundary:
  - `artifacts/unity-vulkan-overlay/command-buffer-before-forward-opaque-skip-before-query.png` and `artifacts/unity-vulkan-overlay/sample-event1-wfe-isolated-skip-before-query-correct-flag.png` are visible sky/ground captures when the native event returns before calling `CommandRecordingState`.
  - `artifacts/unity-vulkan-overlay/command-buffer-query-no-modifies-state.png`, `artifacts/unity-vulkan-overlay/command-buffer-query-restore-inside.png`, `artifacts/unity-vulkan-overlay/command-buffer-after-everything-query-only.png`, `artifacts/unity-vulkan-overlay/command-buffer-before-forward-opaque-plain-event-query-only.png`, `artifacts/unity-vulkan-overlay/sample-event1-wfe-isolated-query-only-correct-flag.png`, and `artifacts/unity-vulkan-overlay/command-buffer-before-forward-opaque-dontcare-query-only.png` are flat near-black/single-color captures even though no IMM draw commands are recorded.
  - Turning off `ModifiesCommandBuffersState`, calling `EnsureInsideRenderPass()` after the query, switching from custom blit to plain plugin events, moving to `AfterEverything`, isolating a `WaitForEndOfFrame` sample-style event, and using a `DontCare` render-pass precondition did not recover the frame.
  - Therefore the immediate Unity Vulkan blocker is not IMM paint/picture draw correctness. It is the contract/state transition around Unity's Vulkan recording-state handoff.
- A diagnostic-only Unity Vulkan run with `IMM_UNITY_SMOKE_DISABLE_MSAA=1` rendered visibly into `artifacts/unity-vulkan-smoke/windows-vulkan-editor-playmode-no-msaa-diagnostic.png`. Its editor smoke metrics were `colorBuckets=2513`, `nonBlackRatio=0.9977`, and `maxLuma=255`. The PNG was opened and inspected; it shows the Unity scene and IMM content. This proves the current Unity Vulkan path can render in a single-sample diagnostic configuration, but it does not satisfy parity because normal SampleScene MSAA/depth integration still fails and forcing MSAA off is not an acceptable final solution.
- Source inspection of `piVulkan_Renderer.cpp` shows `iCreateRenderTargetObjects(...)` creates a native render pass with color/depth attachments but no MSAA resolve attachment. The draw helpers (`iSubmitStaticPaintDraw`, `iSubmitPictureDraw`, and `iSubmitPictureQuadDraw`) each begin that native render pass, submit a command buffer, and wait for the fence. This matches the observed Unity Vulkan MSAA failure mode: the plugin writes outside Unity's own render pass/resolve model.
- Android build guards passed for the shared native changes: `:appImmUnity:assembleDebug`, `:appImmStrokeReader:assembleDebug`, and non-VR `:appImmViewer:assembleDebug -PimmRendererApi=Vulkan -PimmBuildDir=build_vulkan` succeeded; non-VR `:appImmViewer:assembleDebug -PimmRendererApi=GLES -PimmBuildDir=build_gles_fallback` also succeeded. These are build/package checks only.

Root-cause evidence from the depth investigation:

- The failing depth capture before the fix showed observed red bounds `x=664..935`, `y=237..716` while the projected Godot quad was `x=664..958`, `y=236..717`.
- With final composite skipped, Godot's scene color contained the full red quad: `artifacts/godot-compositor-parity/depth-skip-final-composite-control/godot-vulkan-depth.png` had red bounds `x=664..956`, `y=237..716`.
- With paint disabled and 360 picture enabled, the same clipped red bounds remained: `artifacts/godot-compositor-parity/depth-skip-paint-control/godot-vulkan-depth.png` had `num_paint_draw_calls=0` and red bounds `x=664..935`, `y=237..716`.
- Pixel inspection showed a blue strip at `x=643..663`, `y=237..716` immediately left of the red occluder and a missing red strip at `x=936..956`.
- The cause was the Godot final-composite shader sampling the native intermediate with flipped X coordinates. That moved the correct transparent host-depth hole from the native intermediate into the wrong screen-space position.
- Changing the final-composite UV from `(1.0 - pos.x) * 0.5` to `(pos.x + 1.0) * 0.5` aligned the intermediate with Godot scene color and removed the 23-pixel depth-mask offset.
- Switching between callback timings, Godot depth source choices, and Vulkan host-depth compare diagnostics did not fix the offset; those results supported the final-composite UV diagnosis rather than a depth-compare fix.
- Shared Vulkan picture-before-paint ordering remains scoped through `piRenderer::UsesExternalHostDepth()`: the base renderer returns `false`, Vulkan returns the current external-frame host-depth state, and `player.cpp` no longer keys this path directly on `API::Vulkan`.

Not yet validated in this environment:

- Unity Android Vulkan/OpenXR runtime.
- Android GLES runtime after the shared changes.
- Godot Metal runtime.
- Godot XR / multiview compositor output.
- Android runtime smoke could not run locally because `adb devices -l` returned no attached device or emulator.

Known runtime implementation gap:

- Unity desktop Vulkan ordered overlay/background composition now has local evidence for the overlay-camera route with normal MSAA rendering: `artifacts/testing-matrix/unity-vulkan-second-camera-overlay-visible-editor-upright/unity-vulkan-composition.png`. Same-camera arbitrary Unity geometry over IMM is not yet a general feature; the validated route is base camera IMM plus a later Unity overlay camera or explicit command-buffer overlay draw.
- Unity desktop Vulkan full scene-depth composition now has local Windows Vulkan evidence for the runtime smoke fixture, but still needs CI integration, broader Unity scene coverage, Android/mobile validation, and XR validation before full parity can be claimed.
- 2026-06-17 Unity desktop Vulkan full-depth local pass:
  - `artifacts/unity-vulkan-full-depth-offset-fixture-default-20260617/unity-windows-vulkan-full-depth.png` was opened and inspected. It shows the foreground Unity probe visible, the center rear-occlusion probe mostly hidden by IMM content, and the rear-visible Unity probe still visible.
  - `artifacts/unity-vulkan-full-depth-offset-fixture-default-20260617/composition-status.json` reports `rendering=success`, `composition_mode=full_depth`, `depth_composition=success`, and `depth_interleaving=success`.
  - The pass uses Unity's host render pass with a populated camera `RenderTexture` diagnostic target and normal 8x MSAA sample count. The persistent camera target remains a diagnostic capture route until the same render-buffer/depth handoff is proven on Unity's display backbuffer path.
  - Fixes involved correcting the Unity scene-probe fixture to place the rear-occluded probe behind/within IMM content, making Vulkan picture backdrop depth convention-aware, honoring Vulkan `piSTATE_DEPTH_TEST`, and using the less/equal host-depth convention for Unity Vulkan host render-pass composition by default.
- 2026-06-17 Unity desktop Vulkan visible display-backbuffer full-depth pass:
  - `code/ImmUnitySampleProject/Assets/Scripts/ImmUnityRuntimeSmoke.cs` now freezes loaded IMM documents for composition probes at deterministic tick `37800` by default. The overrides are `IMM_UNITY_SMOKE_FREEZE_PLAYBACK=0` and `IMM_UNITY_SMOKE_FREEZE_TIME_TICKS=<ticks>`.
  - A rebuilt Windows Vulkan smoke player was run visibly against the display path, not the diagnostic camera `RenderTexture`: `artifacts/unity-vulkan-full-depth-display-visible-freeze-playback-20260617/unity-windows-vulkan-full-depth-display-freeze-playback.png`.
  - The PNG was opened and inspected. It shows the magenta foreground probe in front, the yellow rear-visible probe still visible, and the cyan rear-occlusion probe effectively hidden by IMM content.
  - `artifacts/unity-vulkan-full-depth-display-visible-freeze-playback-20260617/composition-status.json` reports `rendering=success`, `composition_mode=full_depth`, `depth_composition=success`, and `depth_interleaving=success`.
  - The managed log reports `source=display` and `composition playback freeze documents=1 ticks=37800`; the native log reports `hostDepth=1`, the historical diagnostic alias `assumeHostDepth=1`, `Vulkan renderer began host render pass frame with host depth`, and repeated `Unity Vulkan host render: ... rendered=1`.
  - This closes the earlier "black hidden display capture" diagnostic as a harness/process issue, not a rendering proof. The production-facing CI contract has since been narrowed to `IMM_UNITY_VK_HOST_RENDER_PASS_HAS_DEPTH=1`, which means IMM records into Unity's active Vulkan render pass and uses its depth attachment contract rather than a separately accessible display depth render-buffer pointer.
- 2026-06-17 local full-depth evidence report:
  - `artifacts/full-depth-local-evidence/FULL_DEPTH_LOCAL_EVIDENCE.md` embeds the current Unity Windows Vulkan and Godot Windows Vulkan full-depth captures.
  - Both sections report `composition_mode=full_depth`, `depth_composition=success`, and `depth_interleaving=success`.
  - The images were opened from `artifacts/full-depth-local-evidence/aggregate/captures/...` and visually inspected. Unity shows the foreground probe visible, the rear-occlusion probe mostly hidden by IMM, and the rear-visible probe still visible. Godot shows IMM paint/picture content, the cyan host-depth occlusion region, and visible foreground/rear probe behavior in the expected spatial layout.
  - This is local evidence only; completion still requires committed validation coverage and any required CI/hardware-gated evidence.
- 2026-06-17 Unity desktop Vulkan full-depth CI coverage added:
  - `.github/workflows/ci-engine.yml` now has a `unity-windows-vulkan-full-depth` lane that reuses the Unity Vulkan smoke player, runs `-force-vulkan` with `IMM_UNITY_VK_USE_HOST_DEPTH=1` and `IMM_UNITY_VK_HOST_RENDER_PASS_HAS_DEPTH=1`, captures `unity-windows-vulkan-full-depth.png` from the display path, classifies it with `--composition-mode full_depth`, stages the image/status as evidence, and verifies managed/native log markers for display-source host-depth rendering.
  - The managed log contract requires `source=display`, `composition playback freeze documents=...`, and `scene composition probe passed`; the native log contract requires `hostRenderPassHasDepth=1`, `assumeHostDepth=0`, `Vulkan renderer began host render pass frame with host depth`, and non-skipped rendered draw calls.
  - `tests/tools/verify_workflow_matrix.py`, `tests/tools/test_composition_probe_contracts.py`, and `tests/tools/write_visual_evidence_report.py` were updated so this lane is required by workflow verification and appears separately from ordered-overlay evidence in the final validation report.
  - `.github/workflows/ci-validation.yml` now runs `tests/tools/verify_full_depth_evidence_report.py` after generating `VALIDATION_REPORT.md` when both engine and GPU validation ran. This final-report gate requires embedded Unity and Godot full-depth images plus `composition_mode=full_depth`, `depth_composition=success`, and `depth_interleaving=success` status JSONs. It rejects missing images, ordered-overlay-only evidence, and tiny placeholder PNGs below 320x180.
  - `.github/workflows/ci-core.yml` runs `tests/tools/test_write_visual_evidence_report.py` and `tests/tools/test_verify_full_depth_evidence_report.py` as CI tool self-tests, and `tests/tools/verify_workflow_matrix.py` now fails if those commands are removed.
  - Local verifier runs passed: `python tests/tools/verify_workflow_matrix.py`, `python tests/tools/test_composition_probe_contracts.py`, `python tests/tools/test_classify_composition_modes.py`, and `python tests/tools/test_write_visual_evidence_report.py`.
  - This CI lane is intended to prevent regression of the currently validated Unity Windows Vulkan display-path full-depth smoke. It does not by itself close Android/OpenXR, Godot Metal/XR, the production answer for Unity's null display render-buffer pointers, or performance criteria.
- 2026-06-17 Unity desktop Vulkan display full-depth pass with named host-render-pass depth contract:
  - `code/appImmUnity/src/main.cpp` now distinguishes the old diagnostic alias `IMM_UNITY_VK_ASSUME_HOST_DEPTH` from the production-facing `IMM_UNITY_VK_HOST_RENDER_PASS_HAS_DEPTH` contract and logs both `hostRenderPassHasDepth` and `assumeHostDepth`.
  - `appImmUnity.vcxproj` was built directly with `BuildProjectReferences=false`, `TrackFileAccess=false`, and the real `SolutionDir`; the build completed and copied `ImmUnityPlugin.dll` into the Unity sample package. Direct `cl /c /O2` codegen for `code/appImmUnity/src/main.cpp` also completed. The only compiler warnings were the existing `UnityTextureID` pointer-size conversion warnings.
  - A copied source-built Windows Vulkan smoke player was updated with the freshly built `ImmUnityPlugin.dll` and run with `IMM_UNITY_VK_USE_HOST_DEPTH=1`, `IMM_UNITY_VK_HOST_RENDER_PASS_HAS_DEPTH=1`, and no `IMM_UNITY_VK_ASSUME_HOST_DEPTH`.
  - Evidence: `artifacts/unity-vulkan-full-depth-display-host-render-pass-depth-20260617/unity-windows-vulkan-full-depth-display-host-render-pass-depth.png`, `composition-status.json`, `managed-log-contract.json`, `native-log-contract.json`, and `render-report.md`.
  - The PNG was opened and inspected. It shows the magenta foreground probe visible, the yellow rear-visible probe visible, and the cyan rear-occlusion probe reduced to a small exposed strip while IMM content occupies its region.
  - `composition-status.json` reports `rendering=success`, `composition_mode=full_depth`, `depth_composition=success`, and `depth_interleaving=success`. The native log contract reports `hostRenderPassHasDepth=1` and `assumeHostDepth=0`, plus repeated non-skipped host renders with paint and picture draw-call markers.
- 2026-06-17 current local full-depth aggregate evidence:
  - `artifacts/full-depth-current-local-evidence/view/VALIDATION_REPORT.md` was regenerated from the current Unity named-contract display evidence and the guarded Godot Windows Vulkan full-depth evidence using `tests/tools/write_visual_evidence_report.py`.
  - `tests/tools/verify_full_depth_evidence_report.py --report artifacts/full-depth-current-local-evidence/view/VALIDATION_REPORT.md` passed, proving the aggregate report contains embedded Unity and Godot full-depth images plus success status JSONs. The verifier now also rejects placeholder PNGs below 320x180, so this check proves the report contains viewable evidence-sized images rather than just any image file.
  - The aggregate Unity and Godot PNGs were opened and inspected. Unity shows the expected foreground/rear-visible/rear-occluded probe relationship for the display host-render-pass-depth path. Godot shows paint, host content, and the magenta/cyan/yellow probe regions in the expected depth-interleaving fixture layout.
  - This remains local aggregate evidence. It is not a substitute for a committed CI validation run artifact.
- 2026-06-17 Godot Vulkan full-depth regression pass after the shared Vulkan changes:
  - `artifacts/godot-vulkan-full-depth-missing-probe-guard-20260617/godot-vulkan-full-depth.png` was converted from the smoke PPM, opened, and inspected.
  - The Godot smoke reported `last_depth_aware_vulkan_composite=true`, `last_depth_aware_vulkan_composite_result=true`, and passed scene-probe validation.
  - The Windows Godot Vulkan CI lane's full-depth capture is now treated as a composition fixture: it records candidate render metrics and writes a status/report, but does not compare that probe-heavy image against the older committed DirectX spatial baseline. The scene-probe composition status is the authoritative full-depth pass/fail signal for this lane.
  - `code/projects/windows/run-godot-vulkan-visual-baseline-smoke.ps1` now treats missing scene-composition diagnostics as a composition failure. For `full_depth`, the log must contain `visual smoke scene composition diagnostics` or `visual smoke PPM scene composition diagnostics`; for `ordered_overlay`, it must contain `ordered overlay IMM diagnostics`. `tests/tools/verify_workflow_matrix.py` enforces these guard strings so they cannot be removed silently.
  - The generated status remains local Windows Vulkan evidence only; Godot Metal, Godot XR/multiview, and Android runtime coverage are still outstanding.
- 2026-06-17 Unity sample package-resolution cleanup:
  - `com.unity.purchasing` was removed from `code/ImmUnitySampleProject/Packages/manifest.json` and `packages-lock.json`. Older Unity smoke logs repeatedly reported `The "path" argument must be of type string. Received undefined` through `UnityPurchasingEditor`, and the sample project has no `UnityEngine.Purchasing` or IAP code references.
  - The IMM package layout checks still pass for both embedded Unity packages after the removal. This change keeps validation focused on the existing sample project and avoids an unused editor package interfering with smoke runs.
- 2026-06-17 Unity desktop Vulkan full-depth diagnostics:
  - The Unity Vulkan host-render-pass path no longer requires a source edit to test render-pass color-format compatibility. `IMM_UNITY_VK_HOST_COLOR_FORMAT=<VkFormat>` overrides the default host color format (`44`, `VK_FORMAT_B8G8R8A8_UNORM`) and the native `[IMM_UNITY_VK_HOST_RT_20260612]` diagnostic logs the selected `colorFormat`. This is a diagnostic for display-backbuffer compatibility, not completion evidence by itself.
  - `artifacts/unity-vulkan-full-depth-after-forward-opaque-correct-env-assume-host-depth-visible-20260617/unity-windows-vulkan-full-depth.png` shows IMM picture/paint and Unity probes, but all probes render on top of IMM. The status JSON reports `depth_composition=expected_failed` because the rear occlusion probe remains visible (`share=0.777`). This is ordered-overlay behavior, not full-depth composition.
  - `artifacts/unity-vulkan-full-depth-after-forward-opaque-linear01-assume-host-depth-visible-20260617/unity-windows-vulkan-full-depth.png` changes the output substantially, hiding the probes and rendering a blocky IMM slice. That proves the host-depth pipeline state is affecting rendering, but the depth convention/result is still wrong.
  - Binding `CameraTarget` with `BuiltinRenderTextureType.Depth` before the Vulkan plugin event is not a valid normal-MSAA fix. `artifacts/unity-vulkan-full-depth-depth-target-bind-after-forward-visible-20260617/unity-windows-vulkan-full-depth.png` displays Unity's development-console error `BeginRenderPass: Attachment AA sample counts must match: 8 vs 1 in attachment 1`, and the status still reports `depth_composition=expected_failed`.
  - Disabling runtime MSAA for diagnosis removes the 8x/1x attachment mismatch, but does not produce full-depth composition. `artifacts/unity-vulkan-full-depth-depth-target-bind-nomsaa-visible-20260617/unity-windows-vulkan-full-depth.png` still shows all Unity probes in front and reports `depth_composition=expected_failed` with rear occlusion leakage (`share=0.788`). Therefore the current Unity depth binding is either not the populated camera scene depth attachment or is not compatible with the plugin render event at that point.
  - `artifacts/unity-vulkan-full-depth-display-backbuffer-diagnostic-20260617/unity-windows-vulkan-full-depth-display.png` is a failed display-backbuffer diagnostic from a copied smoke player run without `IMM_UNITY_SMOKE_CAPTURE_CAMERA_TEXTURE`. The image is fully black, `composition-status.json` reports `rendering=failed`, `composition_mode=full_depth`, `depth_composition=expected_failed`, and `depth_interleaving=expected_failed`, and the native log contains plugin initialization/load but no `Unity Vulkan host render` marker. Because this run was started as a hidden background process, treat it as evidence that the current automated display-capture path is inadequate, not as final proof about visible-window display-backbuffer rendering.
  - A persistent camera `RenderTexture` diagnostic makes Unity pass non-null render buffers into the plugin: `artifacts/unity-vulkan-full-depth-persistent-camera-rt-hostpass-visible-20260617/native.log` reports `colorRB=<non-null>`, `depthAttachment=1`, `hostDepth=1`, `assumeHostDepth=0`, and `colorSamples=8`. The capture still fails as ordered-overlay-like output (`rear occlusion share=0.777`), so the remaining problem is not merely missing render-buffer handles.
  - The same persistent camera `RenderTexture` diagnostic with the alternate linear-depth compare, `artifacts/unity-vulkan-full-depth-persistent-camera-rt-hostpass-linear01-visible-20260617/unity-windows-vulkan-full-depth.png`, hides the probes behind a blocky 360/background slice. That confirms the valid-render-buffer path has the same depth convention/value problem as the display path.
  - Paint-only persistent camera `RenderTexture` diagnostics narrow the failure further. With `IMM_RENDER_SKIP_PICTURE=1` and the linear compare, captures `artifacts/unity-vulkan-full-depth-persistent-camera-rt-hostpass-linear01-skip-picture-visible-20260617/unity-windows-vulkan-full-depth.png` and `artifacts/unity-vulkan-full-depth-persistent-camera-rt-hostpass-linear01-skip-picture-nomsaa-visible-20260617/unity-windows-vulkan-full-depth.png` partially interleave paint and Unity geometry, but still fail: the front probe is partly covered (`share=0.276`) and the rear occlusion probe remains too visible (`share=0.318` to `0.329`). This is independent of MSAA and points to IMM paint depth/projection not matching Unity scene depth closely enough.
  - Later visible display-backbuffer diagnostics showed the remaining rear-occlusion leak was partly caused by validating against a moving IMM animation frame. Without freezing playback, the same display path produced real rendered frames but failed the rear-occlusion probe (`share=0.141` at 240 frames, `0.175` at 180 frames, and `0.232` at 600 frames). Freezing playback makes the spatial probe deterministic and produced the passing display capture listed above.
  - Full-depth Unity Vulkan must not be implemented by merely assuming a depth attachment or by binding Unity's resolved/single-sample depth texture into an incompatible MSAA render pass. The handoff must prove matching sample counts, populated scene depth, correct depth convention, and valid render-pass/subpass compatibility.
- Forced no-MSAA diagnostic captures and copy-based fullscreen composites must not be counted as the production overlay milestone unless separately profiled and accepted for mobile-class hardware.
- 2026-06-12 Unity Vulkan host-depth update:
  - Passing Unity depth availability into the host render-pass path and using normal IMM ordering (`paint -> 360 picture`) produces the first correct local desktop Vulkan image at `samples=1`: `artifacts/unity-vulkan-overlay/host-depth-order-normal.png`.
  - Forced `samples=8` with the same host-depth path blacks out the capture, including picture-only and paint-only controls. Evidence:
    - `artifacts/unity-vulkan-overlay/host-depth-order-force-msaa8.png`
    - `artifacts/unity-vulkan-overlay/host-depth-msaa8-picture-only.png`
    - `artifacts/unity-vulkan-overlay/host-depth-msaa8-paint-only.png`
  - Forced `samples=8` with host rendering skipped captures Unity sky/ground, so the black frame is caused by the IMM host-depth draw path rather than the screenshot tool: `artifacts/unity-vulkan-overlay/force-msaa8-skip-host-baseline.png`.
  - Forced `samples=8` through the existing external-image path avoids black but renders only Unity sky/ground, not IMM content: `artifacts/unity-vulkan-overlay/external-image-force-msaa8-normal.png`.
  - Conclusion: the host-depth approach remains a valid 1x diagnostic but is not the MSAA overlay path.
- 2026-06-12 Unity Vulkan ordered-overlay update:
  - `artifacts/unity-vulkan-overlay/transientubo-afterskybox-msaa8-clear-then-normal.png` and `artifacts/unity-vulkan-overlay/transientubo-afterskybox-msaa8-normal.png` are failed 8x MSAA overlay captures. They prove that picture and paint are now present, but they show visible depth/order problems and do not satisfy the milestone.
  - Native trace for the normal MSAA capture logged `picture_gpu=290` and `paint_gpu=9847`, so the current problem is composition/order, not absent draw submission.
  - `artifacts/unity-vulkan-overlay/transientubo-no-msaa-aftereverything-normal.png` shows the earlier 1x host-depth diagnostic path still renders.
  - `artifacts/unity-vulkan-overlay/regression-d3d11-normal.png` shows the D3D11 Unity sample still renders after the shared player/renderer changes, but the capture is Y-flipped.

Unity Vulkan implementation requirements:

- Import or vendor Unity's `IUnityGraphicsVulkan.h` for the Unity plugin build, matching the existing local Unity 2022.3 PluginAPI header.
- During `kUnityGfxDeviceEventInitialize`, capture `IUnityGraphicsVulkan`/`IUnityGraphicsVulkanV2` and `UnityVulkanInstance` so `appImmUnity` can initialize `piRendererVulkan` through `piVulkanExternalDevice` instead of creating an owned swapchain.
- Add an explicit render-target handoff. The current Unity command-buffer event does not pass a Vulkan image/view/depth target, and the Vulkan renderer draw path records and submits its own command buffer. A valid implementation must either:
  - render directly into Unity's current Vulkan recording state/render pass/framebuffer so Unity owns MSAA resolve, depth, ordering, and synchronization, or
  - use a host-command-buffer integration that batches IMM draws without per-draw queue submits and has measured performance comparable to existing GL/DX/Metal-style scene integration.
- Do not count a separate IMM overlay `RenderTexture`, forced single-sample path, extra resolve, or Unity full-screen alpha blit as parity. These can be useful as diagnostic fallbacks, but they add full-frame bandwidth/synchronization cost, miss scene-depth composition unless depth is also handed over, and are especially suspect on mobile/tile GPUs.
- For the overlay milestone, the engineering target is direct host render integration first. A fallback copy/composite route should be kept behind diagnostics or an explicit fallback flag until performance data proves it is not a mobile regression.
- Keep this separate from Godot's `BeginExternalImageFrame(...)` path unless the same contract can represent Unity's current render target and synchronization. Godot supplies an explicit intermediate image/depth image; Unity's current path does not.
- Strengthen the Unity Vulkan visual smoke so it samples expected picture, paint, and host-geometry regions instead of relying on nonblank/color-bucket checks. The current smoke now produces useful captures, but the pass/fail heuristic is still weaker than visual inspection.
- Split Unity Vulkan per-camera preparation so the pre-camera managed path can prepare camera matrices, visibility, and immutable CPU draw state without touching Vulkan command state, descriptor state, render passes, queues, or GPU uploads. GPU-facing updates must either be contained inside Unity's render event/recording context or be done at a separately validated safe point with explicit state restoration.

Shared-code changes must remain scoped until these are tested. A build-only pass is not runtime proof.

## Risks

- Godot alpha blending and non-XR Vulkan depth composition now pass local smoke tests, but they have not been validated on Godot Metal or Godot XR.
- The current Godot Vulkan frame ABI passes a single color layer and a single depth image to native code; multiview XR still needs separate validation.
- Godot depth is attachable for the local Vulkan smoke, but other drivers/platforms may need different synchronization or image-layout handling.
- Sampling and rendering to the same color target can create undefined behavior if a shader-composite approach samples Godot scene color while writing to it.
- Multi-view XR may require array-layer handling in `BeginExternalImageFrameWithView(...)`; the current Godot compositor passes a single color layer.
- Vulkan image layout transitions may need explicit handling because native IMM writes to a Godot-owned texture and Godot reads/presents it afterward.
- Metal and Vulkan may need separate synchronization behavior.
- Shared Vulkan render-order or shader-depth changes can affect the Unity plugin, not just Godot.
- A bad automated image check can report success while the output is visibly wrong. Spatial validation and human inspection are required while this test is being hardened.

## Acceptance Criteria

Phase 1 local Godot Vulkan acceptance requires:

- Godot scene color is visible behind transparent IMM content.
- IMM content is visible over Godot scene color.
- The old full-frame replacement behavior is available only as an explicit debug mode.
- A visual smoke test fails if the compositor replaces the whole Godot color buffer.

Phase 2 local Godot Vulkan acceptance requires:

- Godot opaque geometry can occlude IMM content according to depth.
- IMM content in front of Godot geometry remains visible.
- A visual smoke test fails if depth is ignored.
- The rendered non-XR Godot image has been opened and inspected against the expected composition.

Ordered overlay/background milestone acceptance requires:

- IMM content is visible.
- Host Unity/Godot elements are visibly in front of IMM by render order.
- Host depth is not required and is not claimed.
- Unity Vulkan passes with normal SampleScene MSAA settings.
- No forced no-MSAA, extra full-screen copy chain, or unprofiled resolve path is used as the claimed production implementation.
- Mobile-class performance risk has been assessed with timing, frame-debugger/RenderDoc/AGI evidence, or a concrete bandwidth/synchronization analysis tied to the chosen path.
- Captures have been opened and inspected against expected composition.

Relationship to merged CI composition coverage:

- The CI Engine Matrix now has a Unity Windows DirectX composition lane that builds/runs the Unity smoke player, compares the render capture against the committed DirectX baseline, writes a composition report, and classifies the known full-depth composition failure as `expected_failed` when rendering still passes.
- That CI lane is a regression guard for Unity package import, DirectX render parity, smoke capture orientation/size, and the shared scene-probe contract. It does not prove the Unity Windows Vulkan ordered-overlay route.
- The current Unity Vulkan overlay-camera evidence remains local evidence until a dedicated Vulkan overlay lane exists. A future automated lane should run `-force-vulkan` with the overlay probe enabled, use normal SampleScene MSAA settings, and require `compositing=success` for the ordered-overlay route rather than `expected_failed`.
- Local Vulkan overlay captures and CI reports should keep using the same smoke log markers, projected probe regions, and render-metric reports so failures are comparable across local and CI evidence.
- The visual smoke classifiers now use one shared composition-status vocabulary: `composition_mode=full_depth` for true depth-interleaving expectations, `composition_mode=ordered_overlay` for host-over-IMM render-order expectations, and `composition_mode=render_only` when composition is deliberately not tested. Reports retain the legacy `compositing` summary but also include `composition_contract`, `ordered_overlay`, `depth_composition`, and `depth_interleaving` so the tested contract is explicit.

Full parity is complete only when:

- Mono Godot rendering matches Unity's scene composition behavior for color, alpha, camera transform, projection, and depth.
- Shared-code changes have been validated on Unity Vulkan, Unity Android Vulkan/OpenXR, and Android GLES where those paths are reachable.
- Godot Metal has either passed an equivalent runtime smoke or the Metal path has been explicitly scoped out with a documented platform limitation.
- XR Godot rendering handles each eye correctly.
- Regression guards prevent returning to fullscreen replacement behavior accidentally.

Current status against these criteria: Phase 1 and Phase 2 have Godot Vulkan evidence. The Unity desktop Vulkan ordered overlay/background milestone has CI evidence for the two-camera overlay route on Windows Vulkan MSAA, with DirectX composition passing as a regression guard. Unity desktop Vulkan full-depth composition has CI evidence for the corrected runtime fixture, and the CI Engine Matrix includes a Unity Windows Vulkan full-depth lane that captures a PNG, writes a composition status, emits a visual report, and checks managed/native host-depth log contracts. The top-level validation report verifier requires both Unity and Godot full-depth image/status evidence when engine and GPU validation run.

The scoped `FULL_DEPTH_COMPOSITOR_GOAL.md` milestone is satisfied for Windows desktop non-XR Vulkan by CI run `27725343425` at commit `759081e54fd1bfcb3fbaf63872f5feba802b4655`: `artifacts/ci-validation-evidence-759081e/VALIDATION_REPORT.md` embeds both full-depth captures, `tests/tools/verify_full_depth_evidence_report.py` passes against that report, and the Unity and Godot status JSONs both report `composition_mode=full_depth`, `depth_composition=success`, and `depth_interleaving=success`. The final Unity and Godot PNGs were opened and visually inspected; both show a host-front probe, an IMM-front occlusion probe, and a host-visible reference in the expected layout.

Full cross-platform parity remains incomplete because Unity Android Vulkan/OpenXR, Android GLES runtime, Godot Metal, Godot XR/multiview, broader arbitrary-scene coverage, and production GPU timing/RenderDoc/AGI evidence remain deferred as documented in the Full Depth Platform Disposition and Full Depth Performance Assessment sections.
