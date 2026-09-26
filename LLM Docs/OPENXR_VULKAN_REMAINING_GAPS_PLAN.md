# OpenXR/Vulkan Remaining Gaps Plan

## Purpose

This document tracks the remaining work needed to make OpenXR + Vulkan a real, render-correct runtime path rather than a probe or smoke-test path.

The immediate goal is not to replace every legacy backend at once. The goal is to make one OpenXR/Vulkan path render `sample1.imm` correctly in a headset, with reliable validation around color, depth, orientation, stereo eye placement, swapchain presentation, and teardown.

## Current State

### Confirmed Working

- Unity Windows XR rendering is good in the sample project.
- Unity Windows DX11 non-XR rendering is upright and nonblack after fixing the native depth convention and Unity projection convention.
- Standalone Windows legacy Oculus/OpenGL works when forced/defaulted to slow stereo.
- Standalone Windows legacy Oculus/OpenGL fast stereo has incorrect eye positions and is now treated as a legacy debug option only.
- Standalone Vulkan non-VR has smoke/validation coverage for `sample1.imm`.
- Standalone settings can parse `RenderingAPI=Vulkan` and `XRRuntime=OpenXR`.
- Standalone OpenXR probe code can locate the OpenXR loader, enumerate runtime/system capabilities, query Vulkan extension requirements, enumerate views, and exercise some session/swapchain calls.

### Not Yet Working

- Standalone Windows OpenXR + Vulkan is not a real renderer path.
- `XRRuntime=OpenXR` in `appImmViewer` currently enters `iProbeStandaloneOpenXR()` and then returns false with:
  - `OpenXR standalone startup probe passed; the OpenXR VR backend is not implemented yet`
  - or `OpenXR standalone startup probe failed; the OpenXR VR backend is not implemented yet`
- The OpenXR probe uses fake Vulkan handles for session creation diagnostics:
  - fake instance
  - fake physical device
  - fake logical device
- There is no OpenXR runtime object equivalent to the legacy `piVRHMD` path.
- There is no production bridge between OpenXR swapchain images and the Vulkan renderer's render target model.
- There is no headset visual validation for standalone OpenXR/Vulkan.

## Backend Matrix

| Host | Graphics | XR Runtime | Status | Intended Role |
| --- | --- | --- | --- | --- |
| Unity Windows | DX11 | OpenXR | Working in sample validation | Production Unity path for now |
| Standalone Windows | OpenGL | Legacy Oculus | Working in slow stereo | Legacy fallback |
| Standalone Windows | OpenGL | Legacy Oculus fast stereo | Known eye-position bug | Debug only |
| Standalone Windows | Vulkan | None | Smoke-tested non-VR path | Vulkan renderer validation |
| Standalone Windows | Vulkan | OpenXR | Probe/stub only | Primary remaining target |
| Standalone Windows | OpenGL | OpenXR | Not a target unless needed | Avoid unless product need appears |
| Android | GLES | Legacy Quest/Oculus | Existing separate path | Leave isolated |
| Android | Vulkan/OpenXR | Probe/non-VR scaffolding exists | Separate later target |
| macOS | Metal | No OpenXR | Not applicable | Metal renderer validation |

## Required Outcome

OpenXR + Vulkan is ready when the standalone Windows player can:

- Start with `RenderingAPI=Vulkan`, `XRRuntime=OpenXR`, `EnableVR=true`.
- Use a real OpenXR runtime and real Vulkan graphics binding.
- Render `sample1.imm` into OpenXR swapchain images.
- Present both eyes correctly in headset.
- Preserve correct image orientation.
- Use correct eye poses, IPD separation, and per-eye projections.
- Preserve expected color output, including sRGB handling.
- Preserve expected depth behavior.
- Shut down cleanly without leaking/dangling swapchain, session, Vulkan, renderer, or document resources.
- Produce logs specific enough to debug runtime, swapchain, image, pose, and render failures without user console inspection.

## Workstream 1: Replace Probe With Real OpenXR Backend

### Current Gap

`appImmViewer/src/mymain.cpp` has a standalone OpenXR probe, but no runtime backend. The probe is useful as diagnostics, but it is not a player.

### Plan

1. Introduce a small standalone OpenXR backend object, separate from `iProbeStandaloneOpenXR()`.
2. Keep the probe as a diagnostic mode or internal validation helper.
3. Move production startup into an object with explicit lifetime:
   - loader resolution
   - instance creation
   - system selection
   - Vulkan requirement query
   - session creation
   - reference spaces
   - view configuration
   - swapchains
   - frame loop
   - teardown
4. Do not force it through the legacy `piVRHMD` interface unless that interface is extended deliberately. OpenXR needs explicit frame state, spaces, swapchain images, and predicted display timing.

### Acceptance Criteria

- `XRRuntime=OpenXR` no longer immediately returns false after the probe.
- Failure logs distinguish loader, instance, system, Vulkan requirement, session, swapchain, frame timing, and presentation failures.
- OpenXR startup can be smoke-tested without drawing content, then with clear-only, then with real IMM content.

## Workstream 2: Real Vulkan Graphics Binding

### Current Gap

The OpenXR probe uses fake Vulkan handles. A production OpenXR Vulkan session must bind the actual Vulkan instance, physical device, logical device, queue family, and queue index used for rendering.

### Plan

1. Extend `piRendererVulkan` to expose the required native handles through a narrow API:
   - `VkInstance`
   - `VkPhysicalDevice`
   - `VkDevice`
   - graphics queue family index
   - graphics queue index
   - graphics queue, if needed for synchronization/debug
2. Ensure the renderer is created with OpenXR-required instance and device extensions.
3. Query required extensions before creating the Vulkan renderer:
   - `xrGetVulkanInstanceExtensionsKHR`
   - `xrGetVulkanDeviceExtensionsKHR`
4. Feed these extensions into Vulkan renderer creation.
5. Validate Vulkan API version compatibility with `xrGetVulkanGraphicsRequirementsKHR`.

### Acceptance Criteria

- OpenXR session creation uses real Vulkan handles.
- No fake Vulkan handle constants remain on the production path.
- Logs include the required instance/device extensions and the selected physical device.
- Runtime session creation succeeds on the active OpenXR runtime.

## Workstream 3: OpenXR Swapchain Integration

### Current Gap

OpenXR owns the images presented to the headset. The current Vulkan renderer owns its own render target/swapchain model and does not yet wrap OpenXR swapchain images as render targets.

### Plan

1. Add a Vulkan renderer API for wrapping external `VkImage` handles from OpenXR swapchains.
2. Represent each OpenXR swapchain image as a render target that can be bound by the existing renderer.
3. Track per-image layout transitions:
   - acquired OpenXR image state
   - color attachment state during render
   - release/presentation-compatible state
4. Create one color swapchain per eye initially.
5. Add depth handling:
   - local per-eye depth images managed by the renderer, or
   - OpenXR depth swapchain only after color path is stable.
6. Use the OpenXR-recommended dimensions and sample counts.
7. Start with no MSAA unless the runtime/sample requirements force it.

### Acceptance Criteria

- A clear color appears in both headset eyes through OpenXR/Vulkan.
- The renderer can render into OpenXR swapchain images without placeholder or unsupported-feature diagnostics.
- Swapchain acquire/wait/release ordering is correct and logged.
- Resize/recreate behavior is defined for runtime/view configuration changes.

## Workstream 4: Frame Loop And View/Pose Handling

### Current Gap

The legacy path builds head/eye matrices through `piVRHMD`. OpenXR requires `xrWaitFrame`, `xrBeginFrame`, `xrLocateViews`, render, then `xrEndFrame`.

### Plan

1. Add a frame loop branch for OpenXR:
   - poll events
   - handle session state
   - wait frame
   - begin frame
   - locate views for `XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO`
   - render each eye
   - end frame with projection layer
2. Build per-eye view matrices from `XrView.pose`.
3. Build per-eye projection matrices from `XrView.fov`.
4. Verify coordinate convention mapping against the IMM viewer/player convention:
   - handedness
   - forward axis
   - vertical orientation
   - clip-depth range
   - row/column matrix usage
5. Add logs for the first few frames:
   - session state
   - predicted display time
   - view count
   - per-eye pose position
   - per-eye FOV
   - render target dimensions
6. Do not spam every frame after startup unless an env var is set.

### Acceptance Criteria

- Both eyes render from correct positions.
- Head movement updates view correctly.
- No upside-down or mirrored presentation.
- No eye swap.
- No incorrect IPD scale.

## Workstream 5: Player/Renderer Compatibility

### Current Gap

The Vulkan renderer still has unsupported paths and fallback draw paths. OpenXR needs the subset used by `sample1.imm` to be native and stable enough for headset rendering.

### Plan

1. Identify the exact player features used by `sample1.imm`:
   - static paint
   - picture 2D
   - picture 360/equirect/cubemap, if present
   - model layers, if present
   - sound layers
2. For the OpenXR first milestone, render visual layers only; keep sound independent.
3. Ensure Vulkan renderer supports:
   - static paint draw submission
   - picture layer draw submission
   - constant buffer updates
   - descriptor updates
   - indexed draws needed by IMM layers
   - unit quad mirror/debug rendering if used
4. Track unsupported Vulkan paths and decide whether each is:
   - blocker
   - acceptable for `sample1.imm`
   - later feature

### Acceptance Criteria

- `sample1.imm` renders in headset without Vulkan placeholder/failure diagnostics.
- Any remaining unsupported Vulkan renderer calls are either unreachable in this scenario or explicitly logged as non-blocking.

## Workstream 6: Color, sRGB, And Present Correctness

### Current Gap

OpenXR swapchain formats and Vulkan renderer formats must agree. Incorrect sRGB handling can make the image visibly wrong even if geometry and poses are correct.

### Plan

1. Enumerate OpenXR swapchain formats and choose a Vulkan format deliberately.
2. Prefer an sRGB format only if the renderer writes in the matching color space.
3. Document and assert the chosen mapping:
   - IMM texture color space
   - renderer framebuffer format
   - OpenXR swapchain format
   - shader output expectation
4. Add a minimal color-bar or known-color validation mode.
5. Compare headset/mirror output against standalone Vulkan non-VR validation captures where practical.

### Acceptance Criteria

- No washed-out or double-gamma output.
- Known-color test produces expected values within tolerance.
- `sample1.imm` visually matches the accepted baseline closely enough for headset inspection.

## Workstream 7: Depth And Projection Correctness

### Current Gap

Recent Unity work exposed backend-specific depth convention problems. OpenXR/Vulkan must not repeat that failure.

### Plan

1. Confirm the Vulkan renderer clip-depth convention used by the player.
2. Confirm OpenXR projection matrix construction uses Vulkan depth range.
3. Add a near/far depth sanity scene or diagnostic layer.
4. Validate depth ordering in `sample1.imm`.
5. Keep comments near projection/depth conversion code explaining:
   - source coordinate convention
   - target graphics API convention
   - whether Y is flipped
   - whether depth is `[0, 1]` or `[-1, 1]`

### Acceptance Criteria

- No black rendering caused by depth convention mismatch.
- Foreground/background ordering is correct.
- Projection does not invert, mirror, or vertically flip the image.

## Workstream 8: Mirror Window And Developer Diagnostics

### Current Gap

Headset rendering needs a usable developer feedback path. The legacy Oculus path mirrors the left eye. OpenXR/Vulkan needs an equivalent or a clear validation capture path.

### Plan

1. Add a mirror path after headset rendering is correct enough to inspect.
2. Start with left-eye mirror.
3. Keep mirror rendering separate from OpenXR submission so mirror bugs do not block headset rendering.
4. Add env-controlled diagnostics:
   - log first N frame poses/FOVs
   - dump chosen OpenXR formats/extensions
   - capture one eye to PPM/PNG if renderer readback supports it
   - optionally auto-exit after N frames for smoke tests

### Acceptance Criteria

- Developer can verify nonblank rendering without wearing the headset for every test.
- Automated smoke can detect nonblank output.
- Logs use a unique prefix for OpenXR diagnostics.

## Workstream 9: Tests And Validation

### Manual Validation

1. OpenXR runtime active and headset connected.
2. Launch standalone player with:
   - `RenderingAPI=Vulkan`
   - `XRRuntime=OpenXR`
   - `EnableVR=true`
   - `sample1.imm`
3. Verify:
   - app starts without probe-only failure
   - both eyes render
   - image is upright
   - stereo separation is comfortable/correct
   - head pose tracking works
   - no black frame
   - no obvious sRGB/gamma issue
   - clean exit

### Automated Smoke

Add a script similar to existing Vulkan smoke scripts that can run in these modes:

- `-ProbeOnly`: loader/runtime/system diagnostics, no rendering requirement.
- `-ClearOnly`: OpenXR session + swapchain + clear submit.
- `-RenderSample`: full `sample1.imm` render, local/headset-gated.

Expected artifacts:

- log file
- selected runtime/loader info
- selected Vulkan device info
- selected swapchain format/dimensions
- optional mirror/capture image
- pass/fail summary

### CI Expectations

Hosted CI probably cannot run headset OpenXR rendering. CI should still cover:

- build
- settings parse
- probe with fake loader where possible
- Vulkan non-VR rendering smoke
- no accidental regression of Unity DX11/OpenXR sample behavior

Headset OpenXR rendering remains a local hardware-gated test.

## Proposed Milestones

### Milestone 1: Production Startup Skeleton

- Add OpenXR backend object.
- Keep probe as diagnostic mode.
- Create real OpenXR instance/system/session path up to graphics binding requirements.
- No content rendering yet.

Exit criteria:

- Clear failure modes.
- No fake Vulkan handles on production path.

### Milestone 2: Real Vulkan Binding

- Create Vulkan renderer with OpenXR-required extensions.
- Bind real Vulkan handles into `XrGraphicsBindingVulkanKHR`.
- Create OpenXR session successfully.

Exit criteria:

- Runtime accepts session.
- Logs show real Vulkan device and extension set.

### Milestone 3: Clear To Headset

- Create per-eye OpenXR swapchains.
- Wrap swapchain images as Vulkan render targets.
- Clear each eye to distinct colors.
- Submit projection layer.

Exit criteria:

- Headset shows expected clear output.
- Mirror/capture confirms nonblank output if available.

### Milestone 4: Render `sample1.imm`

- Connect OpenXR views to `Viewer`/`Player`.
- Render visual IMM content into both eyes.
- Validate pose, projection, depth, and orientation.

Exit criteria:

- `sample1.imm` visible and stable in headset.
- No eye-position regression like legacy fast stereo.

### Milestone 5: Harden And Document

- Add smoke scripts.
- Add runtime settings file for standalone OpenXR/Vulkan.
- Add comments at backend convention boundaries.
- Document known unsupported features and explicit legacy fallback.

Exit criteria:

- A developer can build, run, inspect logs, and understand expected failures without source archaeology.

## Explicit Non-Goals For This Phase

- Fixing legacy Oculus fast stereo.
- Reworking Unity rendering beyond regression checks.
- Implementing OpenXR + OpenGL unless a concrete need appears.
- Making Android OpenXR/Vulkan production-ready.
- Implementing all Vulkan renderer unsupported features before the subset needed by `sample1.imm` is proven.
- Refactoring the whole viewer/player architecture before proving headset rendering.

## Main Risks

- Renderer creation order may need changes because OpenXR-required Vulkan extensions are known before Vulkan instance/device creation.
- The current renderer may assume it owns swapchain images and layouts.
- Vulkan renderer unsupported paths may surface only after real headset frame submission.
- OpenXR pose/projection convention bugs can look like renderer bugs.
- sRGB and depth issues can pass nonblank smoke tests but still be visually wrong.
- Hosted CI cannot prove real headset behavior.

## Recommended Next Step

Start with Milestone 1 and Milestone 2 together only as far as necessary to replace the fake Vulkan binding:

1. Extract OpenXR probe code into a reusable backend/diagnostic structure.
2. Add renderer extension negotiation so OpenXR-required Vulkan extensions feed renderer creation.
3. Expose real Vulkan native handles from `piRendererVulkan`.
4. Make `XRRuntime=OpenXR` create a real session or fail with a specific production-path diagnostic.

Do not begin content rendering until real OpenXR session creation with real Vulkan handles is reliable.
