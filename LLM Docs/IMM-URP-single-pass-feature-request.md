# Feature request: URP support with single-pass instanced stereo in `com.immersive-foundation.imm-unity`

## Summary

`ImmPlayerManager` only renders automatically under the Built-in Render Pipeline. Under URP,
the only supported path is the sample's `RenderPipelineManager.endCameraRendering` fallback,
which is mono-only and draws after URP has finished the camera. URP XR projects (OpenXR with
the default *Single Pass Instanced* render mode) cannot display IMM content correctly in a
headset.

We need first-class URP support: a renderer feature that submits IMM inside URP's frame,
renders both eyes with one native draw sequence, and integrates colour and depth with Unity
geometry. Both direct rendering into Unity's texture-array eye target and a layered offscreen
render followed by one stereo composite are permitted under the contracts below.

Windows D3D12 support is a prerequisite workstream: IMM currently implements a D3D11
renderer, not a D3D12 renderer. The existing D3D12 device-discovery hook does not provide
working D3D12 rendering. Scope and estimates must include that backend work as well as
URP integration and layered stereo.

The CI validation workflow is critical to delivery. Every implementation stage must add
or update its automated validation and fix CI regressions as they arise. The difficulty of
automating headset VR validation does not exempt non-VR changes from CI.

## Motivation

URP applications need to place native IMM documents alongside Unity geometry in flat and
XR scenes. Open Brush is one motivating consumer: it currently converts imported `.imm`
documents into strokes and could also offer native IMM playback. The package must provide
a general URP integration; its API, platform contract and acceptance tests must not depend
on Open Brush's project settings, custom renderer features or checkout.

## Scope and constraints

1. The required URP matrix is Windows D3D12, macOS Metal, iOS Metal and Android Vulkan,
   as detailed below. Metal is required, not optional follow-up work. D3D11, OpenGL and GLES
   are not required in the new URP integration. This does not require deleting existing
   native backends or dropping standalone/other-engine CI coverage. Inventory existing
   checks in `tests/matrix_status.json` and `.github/workflows/ci-{engine,gpu,device,ios}.yml`;
   migrate affected Unity checks to the new contract while retaining their rendering
   assertions. Existing D3D11 evidence does not satisfy D3D12 acceptance. Distinguish build,
   simulator, synthetic-stereo and physical-device evidence; an existing job is not proof
   that the new URP path works.
2. Require RenderGraph for Unity integration, with flat mono and single-pass instanced or
   multiview XR. URP Multi-pass and legacy double-wide stereo are out of scope.
3. There are no existing package users requiring backwards compatibility. The package may
   require URP and a specific supported Unity/URP version, change managed/native APIs,
   and replace Built-in hooks and legacy Unity stereo paths. Update the sample and relevant
   CI harnesses together. Preserve platform coverage and rendering correctness, not old
   Unity integration mechanisms. Shared renderer changes must retain existing standalone
   and other-engine CI functionality.
4. Implement one rendering route per backend: direct rendering or one layered offscreen
   render plus stereo composite. Do not expose a route selector or require both routes on
   each backend. The existing dedicated-queue design is not a compatibility requirement.
5. Use the fixed insertion point `AfterRenderingOpaques`. Verify general URP composition
   with opaque/alpha-tested geometry, transparent geometry, a skybox and later renderer
   features. Do not expose an arbitrary `RenderPassEvent` setting or require application-
   specific renderer hooks. Consumer integration checks may supplement the package sample.
6. Depth-of-field and other effects that sample camera depth are deferred. The required
   depth contract covers opaque intersections and occlusion of later Unity transparents.
7. The Windows native C++ player and Godot plugin remain on Vulkan for their primary
   rendering path. Implement D3D12 in IMM's shared C++ renderer with a Unity-specific
   adapter for Unity's device, attachments and command submission. Shared scene/stereo
   improvements can benefit all hosts; this plan does not require a D3D12 migration for
   the standalone player or Godot. Preserve their existing backend validation.

| Platform | Required graphics API | Required rendering modes |
| --- | --- | --- |
| Windows | Direct3D 12 | Flat mono and OpenXR single-pass instanced stereo |
| macOS | Metal | Flat mono |
| iOS | Metal | Flat mono |
| Android | Vulkan | Flat mono and OpenXR multiview stereo on Quest |

Use Unity 6.6 / URP 17.6 / OpenXR 1.18 as the initial integration and validation baseline.
Supporting earlier package versions is not required. Unity OpenXR 1.17 changed the preferred
Windows API from DX11 to DX12 for Oculus PC/Link and SteamVR; the installed 1.18 documentation
retains that recommendation. Windows D3D12 is therefore an explicit target, not shorthand
for either DirectX backend. New URP support for other APIs or Metal stereo is separate work.

## Current behaviour

The original pipeline/stereo observations were made at `upm` (`36a8161f`) and checked
against `main` (`fc859f51`) during this review, including the Windows renderer dispatch:

1. **No pipeline hook under URP.** `OnEnable` sets `_useCommandBufferRendering` and
   subscribes `Camera.onPreCull` only when `GraphicsSettings.currentRenderPipeline == null`.
   Under URP, neither `Camera.onPreCull` nor `CameraEvent` command buffers run, so nothing is
   submitted.
2. **The SRP fallback is mono.** `ImmPlayerExample.OnEndCameraRendering` calls
   `SetCameraMatrices(cameraId, camera, stereoMode)`, which passes `null` for both eye
   matrices, then `IssueRenderEvent`, which uses immediate `GL.IssuePluginEvent`. The draw
   happens after URP's final blit, against whatever target is bound at that point, so there is
   no defined depth relationship with Unity geometry.
3. **Instanced single-pass is explicitly unsupported.** `ResolveStereoMode` maps only the
   legacy `"SinglePass"` (double-wide) mode to `StereoMode.SinglePass` and forces everything
   else, including `SinglePassInstanced` and `SinglePassMultiview`, to `TwoPass`, with the
   comment *"The native plugin doesn't support instanced single-pass"*. URP and the OpenXR
   plugin do not offer double-wide at all, and URP's Single Pass Instanced path renders both
   eyes in one camera pass, so `TwoPass` has no second pass to draw into.
4. **Native `stereoType == 2` is double-wide.** In `ImmEngineBridge::RenderCamera` the viewport
   width is doubled, and the paint, picture and model vertex shaders instance twice. They use
   `SV_InstanceID` plus `SV_ClipDistance0` (HLSL) or `gl_ViewportIndex = gl_InstanceID`
   (GLSL) to place each eye in half of a single 2D target. Unity's instanced single-pass target
   is a 2-slice `Texture2DArray` (or a multiview framebuffer on Quest), where each eye is a
   separate layer at full viewport size.
5. **The Windows DirectX backend is D3D11.** `piRenderer::Create(API::DX)` constructs
   `piRendererDX11`. Unity build automation selects `Direct3D11`, and the DirectX CI runtime
   uses `-force-d3d11`. `appImmUnity` also retrieves a device for `kUnityGfxRendererD3D12`,
   but routes it into `API::DX`, whose implementation treats the device as `ID3D11Device`.
   This is not a D3D12 implementation and must not be used as evidence of D3D12 support.

## Requested changes

### 1. URP renderer feature (package side)

Ship an `ImmRendererFeature : ScriptableRendererFeature` in a package that may depend
directly on `com.unity.render-pipelines.universal`, with an internal integration API, that:

1. Enqueues an `ImmRenderPass` at `AfterRenderingOpaques`.
   IMM tests against Unity opaque depth and writes visible opaque IMM depth so
   subsequent Unity transparents are occluded correctly.
2. Implements the URP integration exclusively through RenderGraph (`RecordRenderGraph`).
   RenderGraph must be enabled; do not implement a Compatibility Mode `Execute` fallback.
   URP 17.6 removes Compatibility Mode; no compatibility-setting check is needed on this baseline.
   Use an unsafe pass to obtain the raw `CommandBuffer` needed for the plugin event. Declare the
   actual texture reads/writes to establish dependencies, explicitly disable pass culling
   for the native event's side effects, and bind the attachments in the execution function.
   Resource declarations alone do not guarantee that a pass cannot be culled.
3. Issues the plugin event through the pass's command buffer, using `IssuePluginEvent` or
   `IssuePluginEventAndData` as required by the submission-data lifetime contract, rather
   than immediate `GL.IssuePluginEvent`.
4. Resolves the *actual* URP camera attachments during RenderGraph pass execution and
   passes backend-specific colour/depth handles to the native side. Generalise the existing
   `SetVulkanCameraRenderBuffers` / `SetVulkanCameraEyeRenderBuffers` idea to all backends.
   Do not assume that URP always uses an intermediate texture or that a Unity render-buffer
   handle is interchangeable with a native texture or image-view pointer. Include formats,
   allocated dimensions, viewport origin and extent, render scale, sample count, slice count
   and XR view-to-layer mapping. Reacquire resources when targets are recreated or resized.
5. Defines ownership and lifetime through native event execution and GPU completion.
   Each queued submission must retain immutable camera, XR-pass, matrix and attachment
   metadata until consumed; subsequent camera/eye submissions must not overwrite it.
   Define backend resource transitions, queue synchronization and release/reuse rules, and
   keep RenderGraph resources alive for every operation that uses them. A dedicated native
   queue must complete or synchronize its writes before URP consumes them.
6. Leaves the active render target, viewport and pipeline state as URP expects after the
   event. Document clobbered state and implement any required restoration in the pass.
7. Renders only cameras carrying an explicit opt-in component (for example `ImmCamera`).
   Applications register the main view and any thumbnail, snapshot or spectator cameras
   that need IMM by adding this component. Preview, Reflection and SceneView cameras are
   excluded. Do not add parallel allow-list, layer or tag filtering mechanisms.

The renderer feature owns render submission. Document its required setup and supply a
configured sample; automatic detection of a missing feature is out of scope. Report
unsupported configurations encountered by the installed feature clearly, once per condition.

### 2. Per-eye matrices from URP's XR pass

1. Take per-eye views and view/layer identities from URP's active XR pass so IMM uses the
   pose URP renders with, including asymmetric Quest frusta. Handle mono and the supported
   two-view single-pass layout explicitly; reject unsupported XR layouts.
2. Capture both eyes together from that XR pass and retain their matrices and layer mapping
   in one submission. Do not independently poll a new XR pose between eyes. The existing
   `ImmCameraMatrixFrameGate` implementation need not be preserved.
3. Derive the GPU projection from URP camera data so target-dependent Y-flip,
   reverse-Z and applicable jitter agree with URP. In URP 17.6 the GPU accessor is internal:
   use `GetProjectionMatrix(viewIndex)` and convert once with `GL.GetGPUProjectionMatrix`,
   using `IsRenderTargetProjectionMatrixFlipped` on the actual attachments, as URP's own
   RenderObjects pass does. Apply any further IMM coordinate
   conversion exactly once. Do not apply `GL.GetGPUProjectionMatrix` again to an already
   converted GPU projection or hard-code `renderIntoTexture = true` for every URP target.
4. Keep camera registration/release, matrices, viewport and native event access in an
   internal bridge used by the feature. Applications configure documents and opt cameras
   in; they do not allocate native camera IDs or issue render events themselves.
5. Provide a reusable, allocation-free submission API for the renderer feature. The current
   public matrix setters allocate arrays and cannot be the per-frame path unchanged.

### 3. Native backend and layered stereo (plugin side)

#### Windows D3D12 prerequisite

1. Implement a working D3D12 backend in the shared C++ renderer, with explicit backend
   selection and a Unity-specific host adapter. Do not pass an
   `ID3D12Device` into the existing D3D11 renderer. Until the D3D12 path is available,
   reject it clearly rather than entering the incompatible renderer.
2. Define the Unity D3D12 integration for command recording/submission, resource states,
   descriptors, pipeline state and fences. Honour Unity resource ownership and retain
   transient resources until GPU completion. This is backend implementation work, not
   just a shader change or replacement enum value.
3. Establish flat D3D12 rendering with all required IMM layer types and Unity colour/depth
   composition before adding layered stereo. Reuse the backend's resource and submission
   contract for the single-pass implementation.
4. Add explicit D3D12 CI build and render checks. Record the actual runtime graphics API
   and fail if it is not D3D12; a successful D3D11 fallback cannot pass these checks.

#### What "single pass" means here

The point of this mode is that CPU and driver cost stay close to mono: the scene is walked
once and each draw is submitted once for both eyes. An implementation meets this only if all
of the following hold for a stereo camera in a frame:

1. **One plugin event** per camera per frame, not one per eye.
2. **One traversal** of layers, one visibility/LOD/culling decision set (against a combined
   frustum covering both eyes), and one set of state and buffer updates.
3. **One submission per draw.** Each draw call (or indirect draw) reaches both eyes by
   Vulkan multiview or D3D12 instancing where the eye is derived from the instance index
   and routed to a layer with `SV_RenderTargetArrayIndex`. For
   draws that are already instanced, double the instance count and derive
   `eye = instanceID & 1` and `instance = instanceID >> 1`, as Unity does.
4. **One native scene render pass / framebuffer** bound to both layers for the whole IMM
   scene draw sequence. This can be Unity's eye target or IMM's layered offscreen target.
   The offscreen route additionally permits one stereo composite draw in a separate pass;
   it must not repeat scene traversal or scene draws. Account for that draw separately.

The following do **not** count as single pass and must not be shipped under this stereo
mode, even if the result looks identical:

1. Looping over eyes on the native side within one event, re-issuing the draw list, or calling
   `RenderStereoMultiPass` (or equivalent) twice.
2. Binding each array slice or per-layer image view in turn and rendering into it separately.
3. Rendering to two separate per-eye targets (or two halves of a wide target) and then
   copying or blitting into the array slices.
4. Issuing two draws per object (one per eye) inside the same render pass.
5. Falling back silently to any of the above on a backend or device that lacks the required
   extension. If broadcast is unavailable, the plugin must report that single-pass is
   unsupported and log it once. Do not fall back to Multi-pass.

Define an explicit layered single-pass stereo mode. Legacy Unity enum values and entry
points need not be preserved; update managed/native callers together. The following are
backend requirements follow the platform matrix:

1. **D3D12:** instance ×2 and derive `eye = instanceID & 1` and
   `instance = instanceID >> 1`; route the eye through the supplied layer mapping to
   `SV_RenderTargetArrayIndex`. Use array RTV/DSV views at full per-eye viewport size.
   Check device support for the selected shader-stage array-index output; otherwise use
   a supported route that still submits each draw once or report the mode unsupported.
   Create compatible views of the supplied resource, respecting its
   format and subresources; do not create replacement Unity eye textures.
2. **Vulkan (Android/Quest):** use enabled Vulkan multiview support (`viewMask = 0b11` for two
   adjacent views) in one scene render pass with compatible layered attachments, either
   Unity's images or the permitted IMM offscreen images. Reconcile this with
   the current offscreen-target, dedicated-queue and composite-quad path. Replace its
   per-eye approach as needed; retaining that queue architecture is not required.
3. **Metal (macOS/iOS):** integrate the existing native Metal renderer with the URP
   mono colour/depth attachments and submission lifecycle. Layered Metal shaders and
   vertex amplification are not required for the flat-only Metal scope.
4. All paint brush types (static and pretessellated), pictures (2D, equirect, cubemap 360) and
   models must render on every required backend. D3D12 and Vulkan also need their layered
   variants; do not implement stereo for paint alone.
5. Honour the attachment's MSAA sample count, or define
   the colour/depth resolve contract if the native renderer requires its own MSAA target.
   The offscreen route must preserve correct coverage and depth at intersections; resolving
   IMM colour and then copying one depth value is not assumed equivalent to shared MSAA.
6. Reverse-Z and depth format must match Unity's attachment so depth testing works both ways.

#### Colour and depth contract

1. Visible opaque and alpha-tested IMM surfaces must update the active Unity depth
   attachment before Unity transparents render. In the direct route, preserve existing
   Unity opaque colour/depth and use compatible depth testing and writing. In the offscreen
   route, the stereo composite must compare depths and write the winning visible opaque
   depth as well as colour, while preserving Unity depth where IMM has no opaque coverage.
2. The existing `ImmVulkanDepthComposite` uses `ZWrite Off` and outputs only colour.
   A URP variant therefore needs depth output as well as texture-array sampling; a sampling
   conversion alone does not meet this contract.
3. Translucent IMM surfaces must not acquire opaque depth writes merely to satisfy the
   integration. Preserve their intended blending/depth-write policy and document limits on
   interleaving translucent IMM layers with Unity transparents. This feature does not
   promise per-object transparency sorting across the two renderers.
4. Active attachment depth and URP's sampled camera depth texture are separate concerns.
   This feature requires the active attachment contract only. Sampled-depth effects are
   deferred and must be documented as unsupported for IMM until separately implemented.
   Depth writing provides occlusion; it does not change Unity's transparent object sorting.

Resolve mono and supported single-pass XR from the active URP pass. Report Multi-pass and
legacy double-wide configurations as unsupported; do not retain a two-pass URP path.

### 4. Documentation and sample

1. A URP sample scene (flat and XR) with RenderGraph enabled and the renderer feature
   configured, and README instructions for enabling RenderGraph, adding the feature to
   a URP Renderer asset and opting cameras in.
2. Document supported combinations explicitly: pipeline × stereo mode × graphics API,
   including tested Unity/URP versions, direct/offscreen route, MSAA and depth-effect limits.

## CI validation throughout implementation

1. Treat `.github/workflows/ci-validation.yml` and its build, engine, GPU, device and iOS
   workflows as part of the implementation, not a final verification task. Record the
   starting CI state and coverage in `tests/matrix_status.json`; distinguish pre-existing
   failures and deferred hardware checks from regressions introduced by this work.
2. Validate all non-VR changes in CI as they are introduced. Update or add the necessary
   build, package-import, native-backend and rendering checks alongside the code. Cover
   Windows D3D12, macOS/iOS Metal and Android Vulkan, including scene content, colour/depth
   composition and MSAA. A successful build alone does not validate rendering behaviour.
3. Stage the work so each substantial change has CI evidence before the next stage depends
   on it: establish flat D3D12 rendering, integrate URP colour/depth submission, then add
   layered stereo. Run relevant shared-renderer regression checks for the standalone
   player and Godot, including their Windows Vulkan paths, throughout these stages.
4. Investigate and fix CI regressions as they occur. Do not accumulate known regressions
   for an end-of-project cleanup, disable failing checks, weaken assertions, or relabel
   failures as unsupported to obtain a passing result. Intentional changes to the Unity
   API/pipeline contract require corresponding test migrations that retain the underlying
   rendering checks. Update visual baselines only for reviewed, intentional output changes.
5. Automated VR validation remains difficult. Keep available synthetic-stereo and
   hardware-backed automation, and validate stereo code, shaders and submission behaviour
   in CI wherever feasible. Clearly record the remaining headset checks, device/runtime
   requirements and evidence gaps. Flat or synthetic tests do not prove headset pose,
   fusion or runtime correctness; use explicit hardware validation for those criteria.
   A blocked headset check must not block independent non-VR validation or excuse failures
   in that validation, and must not be reported as a VR pass.
6. Associate CI evidence with the tested commit and native plugin artifacts. Retain runtime
   API identification, captures, metrics and logs so failures can be traced to the actual
   build. Require passing non-VR CI for completion, with remaining VR evidence reported
   separately and the headset acceptance criteria still outstanding until demonstrated.

## Acceptance criteria

1. On the specified Unity/URP baseline with RenderGraph enabled, the renderer feature added,
   camera opt-in configured and no app-side render code, a loaded `ImmDocument` renders in
   the Game view (flat) and in a headset using the required single-pass XR mode.
2. Stereo is correct in both eyes: correct IPD and fusion, no head-locked content, no eye
   swap, tested on Quest 3 with Vulkan and PC VR with D3D12.
3. An opaque Unity cube intersecting opaque IMM strokes occludes and is occluded correctly
   in both directions. A Unity transparent object drawn afterwards blends over IMM when in
   front and is rejected by IMM depth when behind. Test both eyes and the direct/offscreen
   route selected for each backend. Use the standalone URP package sample to verify
   opaque/alpha-tested and transparent geometry, skybox ordering and subsequent renderer
   features. No Open Brush-specific scene or configuration is required for acceptance.
4. With URP MSAA enabled, IMM strokes are antialiased and depth-tested against the MSAA
   attachment without validation errors.
5. The single-pass mode really is single pass on D3D12 and Vulkan:
   A GPU capture using RenderDoc or an equivalent tool shows each IMM scene draw once,
   instanced ×2 or multiview, together with both-eye visual verification. Record the tool
   and device. Metal acceptance is mono and does not require a stereo capture.
   The direct route has one IMM scene pass targeting both Unity eye layers and no composite.
   The offscreen route has one layered IMM scene pass plus exactly one stereo composite
   draw into Unity's eye target, with correct colour and depth. Neither route has a second
   per-eye scene sequence, per-slice pass or per-eye copy/blit.

   Compare scene draw counts using a fixed fixture entirely visible to both eyes, with
   matched LOD and identical visible draw sets in mono and stereo. Counts must match;
   report the offscreen composite separately. Stereo may legitimately draw more objects
   than mono for other fixtures because its combined frustum covers both eyes.

   Add submission counters or trace markers keyed by camera, frame and XR pass/eye to
   verify exactly one native scene event for each included single-pass stereo camera.
   `GetPerformanceInfo` alone lacks that attribution. On a device without the required
   broadcast support, report single-pass unsupported rather than rendering per eye.
6. Multi-pass XR and legacy double-wide stereo produce an unsupported-configuration
   diagnostic and no incorrect fallback rendering.
7. Cameras without the opt-in component produce no IMM submissions, verified using the
   camera-attributed counters/markers. Exercise multiple opted-in cameras in the same
   frame to verify queued matrices and targets cannot overwrite each other.
8. No managed allocations per frame in the pass, and no RenderGraph warnings or culled-pass
   issues. All URP rendering acceptance checks run with RenderGraph enabled on URP 17.6.
   No Compatibility Mode rendering path is implemented; that mode is absent on this baseline.
9. Migrate affected Unity samples and CI harnesses to the new platform/API contract while
   retaining their composition, depth and content checks. Do not preserve Built-in behaviour
   or legacy package APIs solely for compatibility. Shared renderer changes continue to
   pass relevant standalone and other-engine validation on their existing backends.
10. The minimum URP matrix (Windows D3D12, macOS Metal, iOS Metal and Android Vulkan)
    has explicit build and rendering evidence. Windows checks prove the runtime uses D3D12,
    rather than accepting D3D11 fallback. Both macOS and iOS have Metal render evidence;
    compilation alone is insufficient. Migrate Unity CI as above; existing standalone and
    other-engine D3D11/GL/GLES checks do not impose those APIs on the new URP feature.
    Record flat versus XR coverage explicitly; a flat test does not establish stereo support.
    Do not drop required combinations or classify them as optional to reduce implementation.
11. The CI validation workflow passes for all affected non-VR paths, with no unresolved
    regressions introduced by this work. New backend and URP behaviour has automated
    coverage added during implementation. Existing standalone/Godot validation remains
    passing, and VR automation limitations and outstanding hardware checks are explicit.

## Open questions

1. Which backends can use direct rendering, and which require the permitted layered
   offscreen route? Select one route per backend. Prefer direct rendering where verified;
   measure the cost of any offscreen allocation, synchronization, resolve and composite
   before selecting it.
2. On Quest Vulkan, can the dedicated-queue model meet the resource lifetime and
   synchronization contract, or does the URP path need to use Unity's host queue?

## Phase 1 progress: D3D12 foundation

1. **Baseline:** implementation started from `dd0a5f82` on 2026-09-28. The preceding
   [CI validation run](https://github.com/icosa-mirror/IMM/actions/runs/36437592444)
   completed successfully. The
   [baseline-commit run](https://github.com/icosa-mirror/IMM/actions/runs/36438018979)
   was still running when inspected, with no failed jobs at that point; Windows build and
   standalone D3D11/Vulkan/OpenGL validation had passed. This is baseline evidence, not
   evidence for the D3D12 foundation increment.
2. **First increment:** shared `piDX12CommandContext` provides typed D3D12 device/queue
   validation, three reusable command allocators/lists, completion fences, explicit resource
   transitions and GPU-lifetime retention. It is submission infrastructure, not a completed
   `piRenderer` backend or Unity host adapter. Managed and native Unity initialization now
   reject D3D12 explicitly instead of entering the incompatible D3D11 renderer.
3. **Local evidence:** the WARP D3D12 smoke renders and reads back nine submissions,
   verifies reversed-Z occlusion and distinct frame data, exercises allocator reuse and
   fence-timeout recovery, and passes with the D3D12 debug layer enabled. The production
   shared-core and Unity native source compile checks and workflow matrix verifier pass.
   Reproduce with `code/projects/windows/test-d3d12-submission.ps1`; evidence, source and
   executable hashes are written under `artifacts/d3d12-submission/`.
4. **CI integration:** the Windows build workflow now runs this smoke unconditionally and
   uploads its capture, result and logs as `Windows-D3D12-Submission`. The workflow matrix
   verifier requires these steps. The hosted smoke and evidence upload passed for
   `9ec939a8` in [run 36443675062](https://github.com/icosa-mirror/IMM/actions/runs/36443675062):
   nine D3D12/WARP submissions verified with the debug layer enabled. The initial CI run
   exposed a Visual Studio 2022-only toolchain assumption; the script now selects the
   installed Visual Studio version and matching CMake generator. That pipeline completed
   successfully. Existing rendering jobs remain in place; this run does not establish
   Unity RenderGraph or headset support.
5. **Buffer upload increment:** `UploadBuffer` records CPU-to-default-heap copies and
   retains staging and destination resources through fence completion. The indexed-draw
   smoke consumes uploaded vertex, index, constant and structured buffers, with caller
   references released before submission. Nine readbacks verify depth and per-frame data.
   This helper is for immutable resource initialization; per-frame streaming allocation,
   IMM shader bindings and the scene renderer remain to be implemented. Local validation
   passes; full hosted validation of `f96c95bc` is tracked in
   [run 36933435201](https://github.com/icosa-mirror/IMM/actions/runs/36933435201).
6. **Texture upload increment:** `UploadTexture2D` initializes single-plane 2D textures,
   including mip chains, arrays and block-compressed formats, using device-calculated
   copy footprints. Local RGBA8/BC1 readback checks verify eight subresources with padded
   CPU rows after source data is overwritten and caller texture references are released.
   This establishes transfer correctness only; sampled picture rendering remains pending.
   The hosted D3D12 smoke passed at `c0f76c63` in
   [run 36933955915](https://github.com/icosa-mirror/IMM/actions/runs/36933955915),
   verifying all four buffer uses and eight texture subresources with the debug layer
   enabled. The remaining platform pipeline jobs were still running when recorded.
7. **Shader binding increment:** `piDX12ShaderBindings` maps IMM's existing HLSL constant,
   resource and sampler registers to D3D12 descriptor tables. Each bind retains an immutable
   descriptor snapshot and its resources through GPU completion. Local validation compiles
   the production 2D picture vertex/fragment shaders and verifies sampled colours across
   nine submissions alongside the independent buffer/depth checks. This is shader-level
   evidence, not a completed picture layer or IMM scene renderer. The hosted smoke passed
   at `7936bfa1` in [run 36934540139](https://github.com/icosa-mirror/IMM/actions/runs/36934540139),
   with all nine frames and the production picture-shader assertion verified and the debug
   layer enabled. The broader pipeline was still running when recorded.
8. **Renderer adapter started:** `piRendererDX12` implements typed device/queue setup and
   `piRenderer` buffer creation, mapping, partial updates and constant/structured bindings.
   Resource creation works outside a frame; updates retain distinct queued GPU versions.
   Local readback verifies 18 versions of one logical buffer through the `piRenderer` API.
   Missing operations currently fail explicitly, and the backend is not registered in
   `piRenderer::Create`. Complete draw state, texture methods, streaming allocation and full layer support
   remain incomplete; the initial buffer allocation strategy is not a performance result.
   Hosted validation of this adapter increment is pending.
9. **Adapter draw increment (local):** source/binary HLSL shaders, indexed/non-indexed draws,
   constant/structured descriptor binding and host colour/depth attachments are implemented.
   Local readback verifies nine `piRenderer` frames with reversed-Z occlusion and changing
   constants, alongside the existing upload and production picture-shader checks. Attachment
   resource states are restored at frame end. Indexed readback covers 16/32-bit indices,
   separate vertex/instance streams and nonzero base vertex, instance and index offsets.
   Complete render states and layer rendering remain incomplete; this backend remains
   unregistered. The draw increment's hosted pipeline completed successfully in
   [run 36937569488](https://github.com/icosa-mirror/IMM/actions/runs/36937569488).
10. **CI follow-up:** full validation run `36933435201` failed Android standalone GLES after
   a capture was written but the final edit-validation marker was not observed. The Android
   GLES job and the full pipeline passed on same-commit attempt 2 (job `110618594429`); the intermittent marker
   failure's cause remains unresolved and assertions are unchanged. Run `36935570560` also reported a web
   visual failure whose scene capture included the browser XR-availability button. Visual-test
   mode now hides that overlay, retaining the existing scene thresholds; the full local
   browser harness passes. Hosted extended web validation passed for the separate fix in
   [run 36936815988](https://github.com/icosa-mirror/IMM/actions/runs/36936815988), which completed successfully.
11. **Texture adapter increment (local):** sampled 2D/array texture creation, sampler
   filtering/wrapping and immutable binding snapshots now use the `piRenderer` API.
   GREY/RGBA mip chains are generated at upload, with linear-light filtering for sRGB.
   Nine local frames verify a sampled mip whose value differs from the base-level texel.
   Texture updates and complete layer initialization remain unfinished. Current layer
   texture allocations use 2D, 2D-array and cube textures; blue noise is a 2D array.
   Hosted validation passed in [run 36938102368](https://github.com/icosa-mirror/IMM/actions/runs/36938102368).
12. **Render-state increment (local):** raster/depth/blend state objects, common state
   flags, separate RGB/alpha blend equations and the picture unit quad are implemented.
   Pipeline variants include these state values. Nine readback frames verify disabled
   depth testing and additive blending through the unit-quad path, alongside the existing
   occlusion checks. Hosted validation is pending; this is not complete layer coverage.
13. **Cube texture increment (local):** `TCUBE` maps its six contiguous image faces to a
   D3D12 cube SRV, retaining the existing per-face mip upload path. Local shader sampling
   verifies all six axis directions and texture lifetime after caller destruction. This
   covers the texture backend; cube-picture layer shaders and scene-level validation still
   need integration. Hosted validation of this increment is pending.
14. **Player routing increment:** D3D12 now selects the player render-state objects,
   picture constant-buffer updates, and the HLSL paint vertex-array creation/destruction
   paths for static and pretessellated drawings. The native player library builds locally
   with zero warnings/errors. This is compile evidence; actual layer rendering remains
   unverified. Legacy D3D11 shader-cleanup exceptions do not apply to D3D12.
15. **Mesh layout increment (local):** the D3D12 adapter now accepts IMM's
   `piRArrayLayout` mesh inputs and maps them to the HLSL `CHANA`, `CHANB`, ...
   semantics. It preserves vertex strides and instance divisors and rejects unsupported
   DXGI formats. Three GPU-readback frames exercise a padded vertex stream and a separate
   instance stream; the Windows CI harness requires this evidence. This closes the model
   mesh vertex-array API gap, but does not establish model-layer or scene-level rendering.
16. **Panoramic shader interface increment (local):** corrected the equirectangular
   HLSL vertex/fragment direction semantic and two invalid HLSL identifiers in the stereo
   image branch. Four D3D12 pipeline checks compile and link the real shaders for mono and
   top/bottom stereo images in linear/gamma colour modes; CI requires their result and
   records the shader source hashes. The native player library also builds cleanly. These
   are pipeline checks, not panoramic scene readback or headset single-pass evidence.
17. **Player initialization increment (local):** a headless WARP check now links the
   native player/core/importer/exporter libraries and runs player initialization and cleanup
   for static/pretessellated paint in linear/gamma colour spaces. All four configurations
   pass with the D3D12 debug layer enabled and no reported errors. The Windows harness
   requires this check and uploads its log, result, executable hash, and native-library
   hashes. This exercises layer initialization; document loading and scene rendering are
   still unverified. The native Windows solution must be built before running this harness.
18. **Document lifecycle increment (local):** the headless player check now loads
   `exampleImmFiles/sample1.imm` through `GlobalRender`/`GlobalWork`, verifies the loaded
   state and nonempty layer list, and unloads it in all four paint/colour configurations.
   The test exposed and fixed a null sound-engine dereference during sound-layer updates.
   Local checks pass with no D3D12 debug errors. CI requires four completed loads and
   records the fixture hash. This does not yet call `RenderMono` or verify scene pixels;
   resources whose upload is deferred until drawing remain outside this evidence.
19. **First scene readback (local):** the native player check now calls `RenderMono`
   for `sample1.imm`, using a bounds-framed perspective camera and 8x MSAA colour/depth
   attachments. All four paint/colour configurations submit paint geometry and produce
   nonempty resolved pixel captures without D3D12 debug errors. CI requires four scene
   frames and uploads each capture. Local visual inspection found consistent composition
   between the static and pretessellated paths. This is not an approved visual-baseline
   comparison, complete layer coverage, or a host-depth composition test. An orthographic
   test camera initially produced no draws because the existing projection conversion
   assumes perspective; orthographic camera support remains unverified.
20. **Native host-depth composition (local):** four additional MSAA scene captures
   prefill the left half of the host depth attachment at the nearest reversed-Z value.
   IMM leaves that half untouched while rendering in the other half. Four further captures
   draw host geometry at far depth after IMM; the host background appears while IMM's
   scene pixels remain in front. All twelve scene checks pass with no D3D12 debug errors,
   and CI requires both depth checks and their captures. This establishes native sample
   paint depth interaction; Unity-owned targets, all layer types, and XR remain unverified.
21. **Unity D3D12 submission boundary (local, mocked host):** updated the vendored
   Unity D3D12 interface from Unity's official NativeRenderingPlugin header and added a
   v7 adapter that configures one queue-access event with command-buffer flushing and
   worker synchronization. Renderer initialization, work and shutdown belong inside that
   event; the RenderGraph caller must bind the actual attachments before issuing it, and
   the renderer restores their incoming resource states. The WARP harness mocks the Unity
   interface, verifies event configuration and runs all twelve scene/depth captures through
   the adapter's device/queue initialization. This does not verify Unity event ordering or
   resource-state tracking. Public initialization remains guarded until the actual Unity
   event and RenderGraph target integration is validated. API contract source:
   [Unity's D3D12 plugin header](https://github.com/Unity-Technologies/NativeRenderingPlugin/blob/master/PluginSource/source/Unity/IUnityGraphicsD3D12.h).
22. **Shared bridge ownership increment (local):** `ImmEngineBridge` can now borrow
   a host-owned renderer with explicit lifetime rules, defer player initialization until
   the host has initialized graphics, and shut down without destroying that renderer.
   D3D12 receives the DirectX clip/depth/front-face configuration. An opt-in sound-disable
   setting allows headless bridge validation without opening an audio device; existing
   callers retain sound by default. The native check verifies deferred initialization and
   a successful GPU submission after bridge shutdown, alongside the twelve scene checks.
   CI requires the lifecycle result and records the bridge log and source hashes. Actual
   Unity event wiring remains unfinished.
23. **Unity target adapter (local, mocked lookup):** the adapter resolves colour/depth
   render buffers through `TextureFromRenderBuffer`, checks device ownership, flat target
   dimensions, samples, attachment flags and view formats, and creates typed RTV/DSV views.
   The twelve scene/depth captures now use this path with typeless MSAA resources; invalid
   and typeless RTV formats are rejected. CI requires the mocked target-binding result.
   The caller must establish render-target/depth-write states before the event; actual
   Unity handle resolution and event ordering remain unverified. Full bridge validation
   run `36944346484` was still running without a reported failed job when checked.
24. **Native RenderGraph event entry point (local):** added a versioned 224-byte
   request containing operation, camera matrices, viewport, colour/depth render buffers
   and view formats. D3D12 device initialization configures the queue event; its callback
   initializes the borrowed-renderer bridge, renders a request, or shuts it down. The
   callback publishes completion after consuming the request; callers must retain packet
   memory until that acknowledgement. `GetRenderGraphEventFunc` and
   `GetRenderGraphPacketSize` are exported. Local native compilation and packet lifecycle
   checks pass, alongside the twelve scene/depth captures. The scene captures still use
   mocked Unity lookup, and no managed RenderGraph caller exists yet. Legacy `Init`
   remains guarded; RenderGraph shutdown must use its rendering event rather than `End`.
25. **Managed packet transport (local):** added a C# transport for initialization,
   per-camera rendering and shutdown requests. It copies matrices/targets into owned native
   packets, polls completion through the plugin's acquire operation, reuses acknowledged
   storage, and refuses disposal while requests or the native session remain active.
   Compilation against the installed Unity 6.6 assemblies passes. A Windows CI check loads
   the built plugin and verifies packet size, acknowledgement, result/fence offsets and
   invalid-ABI rejection; local checks pass. This is transport code only: the URP renderer
   feature, camera opt-in and session owner still need to call it and drain shutdown.
   Full bridge validation passed in
   [run 36944346484](https://github.com/icosa-mirror/IMM/actions/runs/36944346484).
26. **CI publication revision matching:** run `36945701101` built the native packet API
   before the managed polling export was added, but binary synchronization checked out the
   newer branch head and compared those older binaries against newer declarations. Pin
   synchronization to the build SHA and skip publication if source changes arrive during
   the build; generated-binary-only updates remain mergeable. Export validation remains
   required. The workflow matrix check and publication shell syntax check pass locally;
   hosted verification of this workflow fix is pending.
27. **Internal URP pass (local compile only):** added `ImmRenderPass` in a dedicated URP
   assembly. It records an uncullable unsafe pass after opaques, declares colour/depth
   read/write dependencies, resolves and binds actual attachments during execution, and
   submits through the managed transport. It requests intermediate attachments explicitly
   and converts URP's projection using their Y-flip state. Compilation against installed
   Unity 6.6 and URP 17.6 assemblies passes. No renderer feature enables this pass yet:
   camera opt-in, session ownership, configured sample and hosted URP execution remain
   required. Temporary restrictions are mono perspective, base/full-viewport cameras,
   matching 2D 8x MSAA targets and no dynamic resolution; these are implementation gaps,
   not reductions to the requirements above.
28. **Unity validation baseline migration:** moved the sample and engine/iOS CI jobs to
   Unity 6.6.0f1, URP 17.6.0 and OpenXR 1.18.0. URP is a required player-package dependency,
   and the package import harness now asserts that the URP submission assembly loads.
   Removed the obsolete built-in VR module, updated legacy input helpers and the affected
   Unity editor packages, and migrated diagnostic object IDs to the Unity 6.6 API.
   Local Unity 6.6 batch import exits successfully and produces `ImmUnity.URP.dll`.
   Hosted validation is pending; this does not establish URP rendering correctness
   or change the outstanding session/camera work.
29. **Camera opt-in and registration:** `ImmCamera` now opts game cameras into the internal
   URP pass. The pass skips missing/disabled opt-ins and excluded camera types. Camera IDs
   are private, stable while registered and released on disable/destruction, including edit
   mode; queued pass data retains its own ID. A focused Unity 6.6 PlayMode test verifies
   opt-in, distinct camera IDs, stable reuse and release to another camera. It passes locally
   and is required by the CI package-import job. Renderer-feature/session wiring and actual
   multi-camera native rendering are still unverified.
30. **Session lifecycle on real Unity D3D12:** added a shared managed owner that submits
   native initialization/shutdown events, waits at lifecycle boundaries using GPU readback,
   and requires packet acknowledgement before releasing memory. Normal rendering does not
   use this wait. It handles application quit and editor assembly reload; failed shutdown
   retains ownership for recovery. A Unity 6.6 D3D12 PlayMode test passes initialization,
   duplicate-session rejection, shutdown and recreation using the locally built plugin.
   The Windows CI smoke player now has a required D3D12 lifecycle run, alongside its existing
   D3D11 composition runs; hosted results are pending. Quit/reload failure paths and scene
   rendering remain unverified, and the renderer feature/manager still need to adopt this owner.
31. **Renderer feature wired (local rendering evidence):** `ImmRendererFeature` now submits
   opted-in cameras using the manager-owned session. D3D12 manager initialization requires
   URP, and disable/shutdown drains that session before releasing document input memory.
   Renderer assets share the session without owning its shutdown. A real Unity 6.6 D3D12
   PlayMode test renders `sample1.imm` through URP into an 8x MSAA target, reads visible
   content, then verifies that camera opt-out stops submissions and clears the image.
   It passes locally; the capture is `artifacts/d3d12-submission/urp-first-frame.png`.
   Integration exposed a bounds-export bug: valid transformed document bounds were being
   replaced with a union of unrelated layer-local bounds. Valid document bounds now take
   precedence. The required CI package-import check also verifies the renderer-feature type.
   Hosted URP scene execution, a configured sample, opaque depth composition and the rest of
   the platform/stereo matrix remain outstanding; the existing CI lifecycle probe alone
   does not prove rendering correctness. Temporary native clear instrumentation was removed.
32. **Open CI regression:** the Unity 6.6 baseline run
   [36948748398](https://github.com/icosa-mirror/IMM/actions/runs/36948748398) fails iOS Metal
   simulator validation because the face-orientation document never reaches render readiness.
   The process stays alive; captures are missing. Its artifact omitted the native player log.
   CI now preserves that log before failing, and the smoke failure reports native loading,
   sequence and spawn-area readiness state. The failure gate is unchanged. Root cause and
   a passing rerun are still required; this regression is not accepted as a new baseline.
33. **Unity opaque depth consumed (local evidence):** the real Unity 6.6 D3D12 URP
   rendering test now places a black opaque Unity quad ahead of the document, verifies
   that no IMM pixels remain, then moves it behind the document and requires visible
   IMM content again. This passes on the current mono 8x MSAA path; the required-test
   verifier confirms execution in `artifacts/d3d12-submission/urp-depth-tests.xml`.
   The test also moves an alpha-one black surface to URP's later transparent pass:
   IMM remains visible when that surface is behind it, and is covered when it is in
   front. This passes in `artifacts/d3d12-submission/urp-depth-write-tests.xml`, establishing
   that native depth writes survive into Unity's transparent pass for this fixture.
   Other MSAA settings, translucent IMM materials, stereo and hosted URP scene validation
   remain unverified.
34. **Configured URP sample (local evidence):** added `Assets/Scenes/SampleSceneURP.unity`,
   a serialized URP asset and renderer containing `ImmRendererFeature`, an opted-in camera,
   and a minimal loader for `sample1.imm`. Assets were generated through Unity Editor APIs.
   The loader selects the scene's pipeline and restores the prior quality pipeline on
   destruction; its manager is a separate object so the manager's persistent lifetime
   cannot prevent scene cleanup. A Unity 6.6 D3D12 PlayMode test loads the actual scene,
   verifies visible native content, unloads it and requires session shutdown. That test
   and the existing colour/depth/opt-out test both pass, with required-test verification
   in `artifacts/d3d12-submission/urp-sample-tests.xml`. The package README explains setup
   and current limitations. A hosted D3D12 scene-validation gate remains outstanding.
35. **Packaged URP scene CI gate:** the Windows DirectX build now also produces a
   dedicated D3D12 player for the configured URP scene. A required validation step
   checks visible content, captures the rendered attachment, verifies camera opt-out
   and requires destruction of the sample's persistent manager without logged errors.
   Hidden-player automatic camera rendering did not advance native GPU loading locally;
   the probe uses URP's explicit `SingleCameraRequest` to render its offscreen attachment.
   This still executes the serialized renderer feature and RenderGraph native pass.
   The flat build temporarily removes XR loaders to avoid native OpenXR pre-initialization,
   restoring the project's loaders afterward. The final standalone player passes locally
   with exit code zero and `[IMM_URP_SMOKE] PASS`; evidence is in
   `artifacts/d3d12-submission/urp-player.log` and `urp-player-frame.png`.
   Workflow matrix verification passes. Hosted execution is pending; this pixel-presence
   gate is not a colour baseline or a complete platform/layer/depth-composition gate.
36. **iOS regression localized to JPEG CPU decoding:** run
   [36952627984](https://github.com/icosa-mirror/IMM/actions/runs/36952627984) preserves
   native loading evidence. Both sample1 and face-orientation fail CPU asset loading;
   the latter reports `loading=Failed sequenceReady=False` when its spawn-area image
   cannot load. The player log twice reports `JPEG parameter struct mismatch: library
   thinks size is 600, caller expects 632`. This identifies a codec ABI mismatch before
   rendering, not a Metal attachment failure. A collision between host and bundled JPEG
   symbols is a candidate explanation; the simulator build now retains Xcode linker maps
   to establish symbol ownership before selecting a fix. The regression remains open.
   The same run's Android standalone GLES failure is an HTTP 500 fetching the Gradle
   distribution, before device validation; it supplies no rendering-regression evidence.
37. **Private iOS JPEG symbols (hosted confirmation pending):** inspection of the bundled
   Mach-O decoder shows its size check expects 632 bytes, matching IMM's caller rather
   than the 600-byte decoder reported by the failed Unity player. The Unity iOS archive
   build now enumerates the JPEG dependency's defined globals and rewrites those symbols
   and references throughout the combined plugin with an `_imm_unity` prefix. This avoids
   binding to a host codec with a different ABI without editing third-party sources.
   The build verifies the complete rewritten symbol set, rejects remaining original
   codec references and preserves other plugin exports before replacing its output.
   A real bundled archive passes the local rewrite audit for 259 symbols across 61 codec
   objects; a small Mach-O probe also verifies caller references follow the renamed symbols.
   iOS Unity build/link/simulator workflows install the required LLVM tools. Their link
   and visual results remain required before the JPEG regression can be marked resolved.
38. **Queued camera metadata snapshot:** the RenderGraph pass no longer retains URP's
   mutable `UniversalCameraData` object. It captures the target-texture presence alongside
   its copied matrices and camera ID, then applies URP 17.6's non-XR game-camera flip rule
   to the resolved attachment during execution. The real Unity D3D12 configured-sample
   and colour/depth/opt-out tests both pass with required-test verification in
   `artifacts/d3d12-submission/urp-camera-snapshot-tests.xml`. This does not establish
   multi-camera or XR correctness; those cases still need their own integration evidence.
39. **Two-camera rendering (local evidence):** the D3D12 URP integration test now runs
   two opted-in cameras with separate 8x MSAA targets and opposing views. The first must
   show IMM while the second stays black. Turning the second toward the document and
   opting out the first must swap those results without stopping the second camera.
   Opting out both must stop native submissions and clear both outputs. These checks,
   the existing depth checks and the configured sample test pass locally with required-test
   verification in `artifacts/d3d12-submission/urp-multicamera-tests.xml`. This verifies
   the tested mono camera pair; hosted multi-camera, resize and stereo cases remain open.
40. **Attachment recreation (local evidence):** the same integration test now releases
   and recreates the second camera's render texture in place at 192x128, replacing its
   native colour/depth resources while retaining the managed object. It requires visible
   IMM content afterward and verifies that the opted-out first camera remains clear.
   The sample, depth, multi-camera and resize checks pass with required-test verification
   in `artifacts/d3d12-submission/urp-resize-tests.xml`. Hosted resize validation and
   dynamic resolution remain outstanding.
41. **Windows model colour-space variants corrected:** the model shader's colour
   conversion is in its pixel stage, but the Windows selector applied the colour-space
   offset to its vertex shader and release builds compiled no pixel colour variants.
   Windows configurations now build three stereo vertex variants and six stereo/colour
   pixel variants, with compile-time checks and selection matching that layout. Bytecode
   inspection confirms the linear variant applies the 2.2 conversion and gamma does not.
   The native plugin builds and the D3D12/WARP checks pass, including four player
   configurations and twelve scene/depth frames. These existing fixtures do not prove
   model-layer rendering; a dedicated model fixture/readback remains required.
42. **Model renderer readback (native CI check):** the existing D3D12/WARP player check
   now constructs a two-triangle unlit model mesh and exercises `LayerRendererModel`'s
   upload, visibility preparation, shader selection and draw path. Its 0.5-grey input
   must read back as approximately RGB 55 in linear mode and RGB 128 in gamma mode.
   Both checks pass locally; the Windows CI script retains `d3d12-model-linear.ppm`,
   `d3d12-model-gamma.ppm` and `model_frames_verified=2` in the player result. The twelve
   existing scene/depth readbacks still pass. This proves the synthetic mono model path,
   not model-file import, other model content or stereo. The model exporter is unfinished;
   a suitable imported-model IMM fixture has been requested for document-level coverage.
43. **Windows stereo-image panorama shader selection:** the Windows shader build now
   generates both mono and top/bottom stereo-image formats for equirectangular pictures,
   and the picture renderer initializes the previously missing stereo-image shader slot.
   Variant counts are checked at compile time; both supported colour spaces are retained.
   The native build and D3D12/WARP checks pass locally, including pipeline creation for
   both image formats and colour spaces, plus the existing scene/model/depth readbacks.
   This is shader-interface and initialization evidence; a stereo panorama image readback
   and headset single-pass rendering remain unverified.
44. **Stereo panorama readback and cleanup (native CI check):** the D3D12/WARP
   player check now decodes a synthetic top/bottom image and exercises the actual picture
   renderer's upload, shader selection, sphere draw and cleanup in both colour spaces.
   The upper half is red and the lower half green; mono output must select the upper half
   and read back approximately RGB (55, 0, 0) in linear mode or (128, 0, 0) in gamma mode.
   This exposed uninitialized unused picture shader slots during destruction; the shader
   table is now zero-initialized. Both panorama checks and existing scene/model/depth
   checks pass locally. CI retains two panorama captures and `panorama_frames_verified=2`.
   This proves mono rendering of a stereo-format image, not headset single-pass rendering.
45. **Windows cubemap picture path (native CI check):** added the missing HLSL cubemap
   pixel shader and Windows shader selection for cross-layout and vertical-strip pictures.
   Both use the existing panorama vertex shader and cube mesh. Cubemap uploads now create
   their sampler, and unload clears texture/sampler handles so subsequent uploads recreate
   them. The D3D12/WARP check decodes and renders synthetic cubemaps in both colour spaces;
   all four new readbacks and the existing checks pass locally. CI retains four cubemap
   captures and `cubemap_frames_verified=4`. Uniform-colour faces establish the upload,
   draw, colour conversion and cleanup paths; face orientation, seams and XR remain open.
46. **Cubemap face-order readbacks (native CI check):** replaced the uniform-face
   probe with six distinct face colours and six orthonormal viewing directions for each
   layout and colour space. All 24 readbacks pass locally through the picture renderer,
   establishing that cross conversion and strip uploads select the expected +X, -X, +Y,
   -Y, +Z and -Z faces. CI retains each capture and `cubemap_frames_verified=24`.
   Within-face rotations/reflections, seams and XR still require separate evidence.
47. **Target-free GPU maintenance (native foundation):** RenderGraph packet operation 3
   now starts a D3D12 command frame, advances document GPU loading/unloading and drawing
   retirement, and submits without camera matrices or render targets. `Player::MaintainGPU`
   performs this work under the existing document mutex without preparing camera visibility.
   The native CI check loads and asynchronously unloads `sample1.imm` solely through CPU
   work and maintenance packets; it passes locally alongside all rendering checks and records
   `target_free_maintenance_verified=true`. The managed manager still issues the legacy
   unload-drain event; routing it through this operation and validating camera-free unload
   in Unity remain required before this lifecycle gap is closed.
48. **Managed camera-free unload (local Unity evidence):** the manager now routes
   pending native unloads through the RenderGraph session's maintenance operation instead
   of the legacy render callback. Submission is asynchronous, permits only one outstanding
   maintenance packet, and retains the existing packet acknowledgement/lifetime handling.
   The D3D12 PlayMode integration test disables both opted-in cameras, requests document
   unload, requires the native document to become inactive within 30 seconds, and verifies
   that no additional camera draw was submitted. Both required rendering tests pass locally
   in `urp-maintenance-tests.xml`. Hosted coverage of this Unity unload check remains open;
   the native CI maintenance check remains required.
49. **iOS CI after codec isolation:** run 36956328324 linked both Unity's JPEG symbols
   and IMM's prefixed codec symbols, exported and built the simulator application, and
   completed sample CPU loading without the earlier JPEG ABI mismatch. The app exited
   before captures; the last native message was `Loading in SPU...`. That does not identify
   the cause. Visual and face-orientation gates remain failed. The simulator harness now
   retains launch exit status, whether the harness stopped the process, filtered system
   logs and newly generated app crash reports on missing captures. YAML and extracted
   Bash syntax checks pass locally; the new diagnostics require another hosted run.
50. **Packaged Windows unload gate:** the required URP player smoke now requests
   document unload after camera opt-out and requires native inactivity within 30 seconds
   before destroying the sample. CI requires separate unload and final-shutdown success
   markers. A rebuilt local standalone player passed both markers, produced its capture
   and exited 0. The earlier configured URP scene and session lifecycle gates also passed
   on hosted run 36955372120 (job 110681785150); the new unload gate awaits hosted results.
   That run's Windows Vulkan build exited 137 without a compiler diagnostic and has been
   retried; the existing Vulkan validation requirements remain unchanged.
51. **Deferred unload without cameras:** the integration test reproduced a 30-second
   stall when requesting unload immediately after loading a document with no opted-in
   camera. The manager deferred unloading until loading completed, but only submitted
   maintenance for already-queued native unloads. It now also submits maintenance while
   deferred unloads exist. The same test passes after the fix, with no additional camera
   submissions (`urp-deferred-unload-before.xml` fails; `urp-deferred-unload-after.xml`
   passes). The packaged smoke now exercises this case and CI requires its separate
   success marker; hosted validation of the addition is pending. The Windows Vulkan
   build retry also exited 137 during Editor initialization and needs further diagnosis.
52. **Windows Vulkan CI termination investigation:** both attempts of the older
   Unity Vulkan player build exited 137 during Editor assembly initialization, after
   restoring the same Library cache. The workflow could publish that cache on failure.
   Library publication now requires a successful build, and a new cache namespace forces
   a clean generated Library on the next run while retaining downloaded packages. The
   job also retains host memory pressure and recent kernel OOM diagnostics. This fixes
   cache publication policy and improves evidence; neither cache corruption nor OOM is
   yet established as the cause. YAML and Bash syntax checks pass locally; hosted build
   and Vulkan rendering validation remain required.
53. **D3D sample-count-aware alpha coverage (native foundation):** all six HLSL
   fragment shaders (both paint techniques, model and three picture shaders) now derive
   their coverage width and rotation from `GetRenderTargetSampleCount()` instead of
   hard-coding eight bits. This avoids multiplying shader variants or extending the
   constant-buffer ABI. New half-opacity model readbacks pass at 2x, 4x and 8x MSAA, and
   existing scene/model/picture/depth checks still pass locally. CI retains those captures
   and `model_half_opacity_sample_counts_verified=3`. The managed 8x attachment guard is
   still present until 1x behavior and Unity composition at other sample counts are
   validated; other layer types also need coverage-specific readbacks.
54. **D3D12 attachment sample counts (local native and Unity evidence):** the native
   half-opacity check now includes 1x targets, with correct single-sample depth views and
   direct-copy readback. It requires binary covered/uncovered pixels and 50% spatial
   coverage within two percentage points. All four native sample-count checks pass.
   The managed pass now accepts matching 1x, 2x, 4x and 8x colour/depth attachments.
   Both required PlayMode rendering tests pass in `urp-msaa-tests.xml`, including near/far
   Unity opaque occlusion and later-transparent occlusion at every sample count, followed
   by the existing multi-camera, resize and unload checks. The package README reflects
   these limits. Hosted URP coverage still uses 8x; other layer types need dedicated
   fractional-coverage checks before claiming full sample-count validation.
55. **Picture fractional coverage (native CI check):** the picture renderer probe now
   covers all five image formats: 2D, mono/stereo equirectangular, cubemap cross and cubemap
   strip. Each renders at half opacity into 1x, 2x, 4x and 8x attachments. All 20 readbacks
   pass locally, including binary-pixel and 50% spatial-coverage checks for 1x targets.
   CI retains the captures and `picture_half_opacity_frames_verified=20`. Existing model,
   scene/depth and cubemap face-order checks still pass. This validates layer opacity with
   opaque source texels in linear colour space; source alpha, paint fractional coverage
   and XR remain separate checks.
56. **Packaged URP sample-count CI gate:** the Windows player smoke now recreates
   its attachment at 1x, 2x, 4x and 8x, sets the URP sample count to match, and requires
   visible IMM output at each setting. It writes four additional captures, and CI requires
   every capture and success marker before accepting the unload/shutdown result. A rebuilt
   local player passed all four rendering checks, both camera-free unload paths and final
   shutdown with exit 0. Hosted execution is pending. This adds packaged scene-presence
   coverage; the more detailed bidirectional depth checks at all sample counts currently
   remain local PlayMode evidence.
57. **Projection-independent native frustum conversion:** the zero-to-one clip-depth
   conversion assumed a perspective matrix, causing orthographic scene culling failures.
   It now applies `z_clip = 2*z_clip - w_clip` to the complete matrix row, preserving X/Y
   and handling reversed depth without perspective-specific coefficients. Twelve new
   orthographic scene/depth readbacks pass locally across both paint techniques and colour
   spaces, as do the existing perspective checks. CI retains these captures and
   `orthographic_frames_verified=12`. The managed perspective-only guard remains until
   Unity orthographic integration and distance-based layer culling are verified; these
   native checks alone do not establish complete orthographic support.
58. **Orthographic cameras and distance culling:** paint (both techniques), model and
   picture renderers now skip their perspective-only distance/size rejection when the
   projection has constant clip W; frustum culling remains active. Native orthographic
   scene/depth checks pass with the camera 1,000 scene radii away. The managed pass now
   accepts mono orthographic cameras. Both required Unity rendering tests pass locally in
   `urp-orthographic-tests.xml`, including a distant orthographic view and native depth
   occlusion of later transparents, followed by a return to perspective and existing
   lifecycle checks. Package documentation reflects the camera support. The native CI
   check now includes the distant case; hosted results and Unity orthographic evidence
   remain pending.
59. **Layered D3D12 target foundation:** the Unity host adapter now has an explicit
   two-slice binding mode, creates array RTV/DSV views for single-sample and multisampled
   attachments, and checks vertex-stage array-index capability. Mono binding still rejects
   arrays. A synthetic native check draws red and green into separate slices in one
   instanced draw, then requires later blue geometry behind both to fail depth testing.
   All pixels in both slices pass at 1x, 2x, 4x and 8x, with D3D12 debug validation enabled;
   existing checks also pass. CI retains four captures and `layered_target_frames_verified=4`.
   This is attachment/submission groundwork, not IMM stereo rendering: managed XR packets,
   per-eye matrices and production layered paint/picture/model shaders remain outstanding.
60. **Remaining phase 1 work:** implement IMM's D3D12 resource/shader/draw backend and the
   Unity submission/target adapter, render all IMM layer types with colour/depth composition,
   and add scene-level D3D12 CI evidence. Only then remove the temporary initialization
   guards and mark flat D3D12 supported. Single-pass stereo remains a later stage.

## API references

1. [Unity 6.0 URP unsafe render passes](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/render-graph-unsafe-pass.html).
2. [URP 17 UniversalCameraData GPU projection API](https://docs.unity3d.com/Packages/com.unity.render-pipelines.universal@17.0/api/UnityEngine.Rendering.Universal.UniversalCameraData.html).
3. [RenderGraph pass culling control](https://docs.unity3d.com/Packages/com.unity.render-pipelines.core@17.0/api/UnityEngine.Rendering.RenderGraphModule.RenderGraphBuilder.html).
4. [Unity 6.6 DirectX feature comparison](https://docs.unity3d.com/6000.6/Documentation/Manual/UsingDX11GL3Features.html).
5. [Unity OpenXR 1.17 preferred graphics APIs](https://docs.unity3d.com/Packages/com.unity.xr.openxr@1.17/manual/index.html#runtimes).
