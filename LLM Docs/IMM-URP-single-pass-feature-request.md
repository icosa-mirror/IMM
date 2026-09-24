# Feature request: URP support with single-pass instanced stereo in `com.immersive-foundation.imm-unity`

## Summary

`ImmPlayerManager` only renders automatically under the Built-in Render Pipeline. Under URP,
the only supported path is the sample's `RenderPipelineManager.endCameraRendering` fallback,
which is mono-only and draws after URP has finished the camera. URP XR projects (OpenXR with
the default *Single Pass Instanced* render mode) cannot display IMM content correctly in a
headset.

We need first-class URP support: a renderer feature that submits IMM inside URP's frame,
renders both eyes in one pass into Unity's texture-array eye target, and depth-tests against
(and composites with) Unity geometry.

## Motivation

Open Brush (Unity 6000.6, URP 17.6, OpenXR `renderMode: SinglePassInstanced`, targets PC VR
on D3D11, Quest on Vulkan/GLES, and flat macOS/iOS on Metal) already imports `.imm` files via
`imm-stroke-reader` and converts them to its own strokes. We want to add an option to keep
imported documents as native IMM objects played back by `imm-unity`, placed in the scene
alongside Open Brush strokes, models and images. That requires the player to work in a URP
XR app on the same terms as in the Built-in pipeline.

## Current behaviour

Observed in `Runtime/ImmPlayerManager.cs` and `appImmUnity` / `appImmShared` at `upm`
(`36a8161f`):

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

## Requested changes

### 1. URP renderer feature (package side)

Ship an `ImmRendererFeature : ScriptableRendererFeature` (in its own assembly, conditionally
compiled on `com.unity.render-pipelines.universal`, so Built-in users are unaffected) that:

- Enqueues an `ImmRenderPass` at a configurable `RenderPassEvent`, defaulting to
  `AfterRenderingOpaques` so Unity transparents blend over IMM and IMM occludes/is occluded
  by Unity opaques.
- Supports the RenderGraph API (mandatory path in Unity 6 / URP 17). Use an unsafe pass,
  since `IssuePluginEvent` needs a raw `CommandBuffer`, that declares read/write on the
  active camera colour and depth attachments so RenderGraph does not cull, reorder or merge
  it away. A Compatibility Mode `Execute` path is optional.
- Issues the plugin event via `cmd.IssuePluginEvent(renderEventFunc, eventId)` rather than
  `GL.IssuePluginEvent`, so it is ordered inside URP's command stream.
- Passes the *actual* URP camera attachments (native colour/depth pointers, size, sample
  count, slice count) to the native side each frame. Generalise the existing
  `SetVulkanCameraRenderBuffers` / `SetVulkanCameraEyeRenderBuffers` idea to all backends.
  Under URP the bound target is an intermediate texture, not the backbuffer, and on
  Vulkan/Metal the plugin cannot infer it.
- Leaves the active render target, viewport and pipeline state as URP expects after the event
  (or documents which state it clobbers so the pass can restore it).
- Filters cameras: a camera allow-list or layer/tag rule, plus skipping Preview, Reflection
  and SceneView cameras by default. Apps typically have extra cameras (thumbnails, snapshots,
  spectator views) that must not each pay for an IMM render.

`ImmPlayerManager` should detect URP and defer to the feature instead of silently doing
nothing, and log a clear warning if URP is active but the feature is missing from the active
renderer.

### 2. Per-eye matrices from URP's XR pass

- Take view and projection per eye from URP's XR pass (`cameraData.xr.GetViewMatrix(i)` /
  `GetProjMatrix(i)`, `viewCount`) so IMM uses exactly the pose URP renders with, including
  asymmetric Quest frusta.
- Keep the existing one-pose-per-frame guarantee (`ImmCameraMatrixFrameGate`).
- Use the render-into-texture projection convention (`GL.GetGPUProjectionMatrix(p, true)`)
  where URP renders to an intermediate target, and handle the Y-flip once, in one place.
  Today this is resolved by `ImmProjectionDestinationResolver` for Built-in cases only.
- Expose a public `SetStereoCameraMatrices` overload that also sets the viewport, and a public
  way to allocate and release a camera ID. Apps currently cannot reach `GetOrCreateCameraInfo`
  / `_cameras` / `_renderEventFunc`.

### 3. Native texture-array / multiview stereo (plugin side)

#### What "single pass" means here

The point of this mode is that CPU and driver cost stay close to mono: the scene is walked
once and each draw is submitted once for both eyes. An implementation meets this only if all
of the following hold for a stereo camera in a frame:

- **One plugin event** per camera per frame, not one per eye.
- **One traversal** of layers, one visibility/LOD/culling decision set (against a combined
  frustum covering both eyes), and one set of state and buffer updates.
- **One submission per draw.** Each draw call (or indirect draw) reaches both eyes by
  hardware broadcast: multiview (`VK_KHR_multiview`, `GL_OVR_multiview2`, Metal vertex
  amplification) or instancing where the eye is derived from the instance index and routed
  to a layer (`SV_RenderTargetArrayIndex`, `gl_Layer`, `render_target_array_index`). For
  draws that are already instanced, double the instance count and derive
  `eye = instanceID & 1` and `instance = instanceID >> 1`, as Unity does.
- **One render pass / framebuffer** bound to both layers of the eye target for the whole
  IMM draw sequence.

The following do **not** count as single pass and must not be shipped under this stereo
mode, even if the result looks identical:

- Looping over eyes on the native side within one event, re-issuing the draw list, or calling
  `RenderStereoMultiPass` (or equivalent) twice.
- Binding each array slice or per-layer image view in turn and rendering into it separately.
- Rendering to two separate per-eye targets (or two halves of a wide target) and then
  copying or blitting into the array slices.
- Issuing two draws per object (one per eye) inside the same render pass.
- Falling back silently to any of the above on a backend or device that lacks the required
  extension. If broadcast is unavailable, the plugin must report that single-pass is
  unsupported so the app can pick Multi-pass, and log it once.

Add a new stereo type (e.g. `StereoMode.SinglePassInstanced = 3`) distinct from double-wide:

- **D3D11 / D3D12:** instance ×2 and write `SV_RenderTargetArrayIndex = instanceID` into a
  2-slice array RTV and DSV at full per-eye viewport, instead of doubling the viewport and
  splitting with `SV_ClipDistance0`. The target must be created or bound as an array view
  from the pointer Unity supplies.
- **OpenGL / GLES:** write `gl_Layer` on desktop GL. On GLES/Quest use `GL_OVR_multiview2`
  with `gl_ViewID_OVR` (the `appImmViewer` Android path and several `*.es.glsl` shaders
  already reference multiview), matching Unity's multiview framebuffer.
- **Vulkan (Quest):** render with `VK_KHR_multiview` (`viewMask = 0b11`) in a single render
  pass whose attachments are 2-layer image views of Unity's eye image. Reconcile this with
  the current offscreen-target, dedicated-queue and composite-quad path, which was built
  around `TwoPass` and renders each eye separately.
- **Metal:** `render_target_array_index` (or vertex amplification where available).
- All paint brush types (static and pretessellated), pictures (2D, equirect, cubemap 360) and
  models need the layered variant, since each currently has its own `STEREOMODE==2` code.
- Honour the attachment's MSAA sample count (URP's MSAA is on in Open Brush), or define
  the resolve contract if the native renderer requires its own MSAA target.
- Reverse-Z and depth format must match Unity's attachment so depth testing works both ways.

`ResolveStereoMode` should then map `SinglePassInstanced` and `SinglePassMultiview` to the new
mode, and keep `TwoPass` for URP *Multi-pass*, which the renderer feature should also support
by issuing one event per XR pass or eye.

### 4. Documentation and sample

- A URP sample scene (flat and XR) with the renderer feature configured, and README
  instructions for adding it to a URP Renderer asset.
- Document supported combinations explicitly: pipeline × stereo mode × graphics API.

## Acceptance criteria

1. In a URP 17 / Unity 6 project with the renderer feature added and no app-side render code,
   a loaded `ImmDocument` renders in the Game view (flat) and in a headset (OpenXR, Single Pass
   Instanced).
2. Stereo is correct in both eyes: correct IPD and fusion, no head-locked content, no eye
   swap, tested on Quest 3 (Vulkan and GLES) and PC VR (D3D11).
3. An opaque Unity cube intersecting IMM strokes occludes and is occluded correctly in both
   directions. A transparent Unity object at `BeforeRenderingTransparents` or later blends
   over IMM.
4. With URP MSAA enabled, IMM strokes are antialiased and depth-tested against the MSAA
   attachment without validation errors.
5. The single-pass mode really is single pass, verified on each backend:
   - A GPU capture (RenderDoc on D3D11 and Vulkan, Xcode on Metal) of one stereo frame shows
     one IMM render pass bound to both eye layers, with each IMM draw appearing once,
     instanced ×2 or multiview. There is no second per-eye draw sequence, no per-slice render
     pass and no copy or blit into the eye texture.
   - IMM draw-call count in `GetPerformanceInfo` for a stereo frame equals the mono count
     for the same view, within a small fixed overhead that is documented. It is not roughly
     double.
   - Exactly one IMM plugin event per stereo camera per frame.
   - On a device without the required broadcast support, the plugin reports single-pass as
     unsupported rather than rendering per eye.
6. Multi-pass XR under URP also renders correctly, using the per-eye `TwoPass` path.
7. Secondary cameras excluded by the filter do not trigger IMM renders, measured via
   `GetPerformanceInfo`.
8. No managed allocations per frame in the pass, and no RenderGraph warnings or culled-pass
   issues.
9. The Built-in pipeline behaviour is unchanged.

## Open questions

- Should the native side render directly into Unity's attachments (preferred, lowest cost) or
  into its own layered target that the pass then composites with a depth-aware blit, as the
  Android Vulkan Built-in path does today? If composite, the IMM render into that target must
  itself meet the single-pass rules above, being a layered target rendered by broadcast. The
  composite must also be a single instanced or multiview draw covering both layers, using a
  texture-array, URP-compatible variant of `ImmVulkanDepthComposite`, not one blit per eye.
- On Quest Vulkan, can the dedicated-queue model coexist with rendering inside URP's render
  pass, or does the URP path need to use the host queue?
- Does the native renderer need depth *written* into Unity's buffer, for Unity transparents to
  sort against IMM and for post-effects such as depth of field, or is depth *test only*
  sufficient?
