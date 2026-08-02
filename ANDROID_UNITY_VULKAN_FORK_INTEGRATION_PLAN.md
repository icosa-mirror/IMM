# Android Unity Vulkan Fork Integration Plan

## Objective

Make the 2D Unity Android Vulkan validation jobs render correctly on real Firebase Test Lab hardware, including both the render-only and Unity-composition scenes.

Success requires a physical-device screenshot that visually matches the approved baseline within calibrated perceptual tolerances. Internal render-target captures and log messages are diagnostic evidence only; they cannot make a job pass without the external visual check.

## Current evidence

- IMM renders correctly into the explicit Android Vulkan render target used by the Unity sample.
- The latest internal render and composition captures contain recognizable, substantially correct scene content.
- The Firebase external-screen capture is completely black. The remaining immediate failure is therefore between the correct offscreen result and Unity's final Android surface presentation.
- Direct access to `Display.main.colorBuffer` is not portable on Android Vulkan. Normal Android devices can expose it to a Unity native plug-in as a 1x1 placeholder while the actual camera surface is full resolution.
- Recent attempts based on direct recording into Unity's active command buffer and several presenter-camera variants have not produced pixels on the physical Firebase screen.
- Sleepy-Pete's `vr-main` branch reports a visually verified Unity 6 + Android + Vulkan implementation on Quest hardware. It is architecturally closer to IMM's earlier offscreen/own-submission strategy than to the direct-recording strategy pursued recently.

## Architectural decision

Do not reset or revert the repository to an old commit. The current branch contains necessary CI validation improvements, corrected evidence handling, and useful renderer fixes.

Instead, retain the current CI and validation infrastructure while restoring the earlier Android Vulkan architecture as a coherent subsystem:

1. IMM renders into an explicit offscreen Unity `RenderTexture`.
2. IMM records and submits its Vulkan work independently of Unity's active render pass.
3. A real synchronization bridge orders Unity's read after IMM's write.
4. Unity composites the completed texture into its original camera target from a camera command buffer.
5. Unity remains responsible for its Android swapchain, orientation, color conversion, frame pacing, and final presentation.

The fork implementation must first be tested close to its verified form. Simplifying it before establishing a passing physical-device result would make it unclear whether a failure came from the architecture or from our adaptation.

## Relevant fork work

The implementation reference is the `vr-main` branch of `Sleepy-Pete/IMM`, not that fork's obsolete default `main` branch.

Relevant commits include:

- `7a77346cbea2` — dedicated second queue, explicit offscreen targets, and Unity camera-pass composite.
- `28e28724e364` — persistent external-image wrappers, tracked layouts, and corrected `VkImageMemoryBarrier.sType`.
- `41b6be11c723` — eye-frame batching and Vulkan pipeline-variant caching.
- `b6c5995dbdf0` — removal of blocking per-eye round trips and reversed-Z correction for external targets.
- `012f635c28a7` — command-buffer/fence resource separation and pipelined batch slots.
- `0dae82635d94` — semaphore-based same-frame bridge from IMM's queue to Unity's graphics queue.
- `94515a21a6ca` — same-frame single-buffer presentation becomes the verified default.
- `0f52123d766a` — picture descriptor and descriptor-pool corrections.
- `c3b26b249d9d` — Vulkan 2D-picture transform and per-eye projection corrections.
- `e6314a2a24a3` — picture color-space correction.
- `0837477b9301` — picture depth-write correction.

These commits have dependencies and should not be cherry-picked blindly or individually without reviewing the complete resulting Vulkan state machine.

## Integration procedure

### Phase 1: Preserve evidence and establish a controlled branch

1. Keep the current external-screen Firebase gate unchanged.
2. Preserve the render-only and composition internal captures as diagnostics.
3. Create a dedicated integration branch from current `main`.
4. Remove or disable the unverified presenter-camera experiments on that branch without deleting the CI evidence code.
5. Record the last known partially rendering Android Vulkan revision as an A/B reference, not as the new branch base.

### Phase 2: Compare the complete runtime paths

Compare current `main` with the fork for the full Android Vulkan path, including:

- `ImmPlayerManager.cs`
- `ImmNativePlugin.cs`
- `appImmUnity/src/main.cpp`
- `piVulkan_Renderer.cpp` and `piVulkan_Renderer.h`
- Vulkan shader sources and generated SPIR-V includes
- Android boot configuration
- Unity Android graphics and render-target settings
- Packaged ARM64 native plug-in binary

Produce a dependency map before editing. Separate presentation-critical changes from XR pose handling, controls, audio, document-specific diagnostics, and other unrelated fork work.

### Phase 3: Port the coherent offscreen renderer

1. Allocate an explicit, stable, non-MSAA Unity `RenderTexture` for IMM output.
2. Pass its current native render-buffer identity to the plug-in without using the display render buffer.
3. Re-query or update native render-buffer identities whenever Unity recreates a texture.
4. Port persistent Vulkan wrapper caching and real image-layout tracking.
5. Correct all barriers and attachment layouts without relying on stale Unity state.
6. Port the fork's batched external-image render path and resource-lifetime separation.
7. Preserve reverse-Z semantics expected by Unity's Vulkan projection.
8. End the IMM image in the layout required by Unity's sampling operation.

### Phase 4: Establish cross-queue synchronization

Port the fork's synchronization architecture as an initial control:

1. Request an additional Vulkan graphics queue before device creation.
2. Confirm at runtime that queue index 1 was actually created and differs from Unity's graphics queue.
3. Submit IMM rendering on the dedicated queue.
4. Signal a per-frame/per-slot semaphore from the IMM submission.
5. Submit the corresponding wait onto Unity's graphics queue before its composite command executes.
6. Ensure semaphore reuse and failure handling cannot produce unmatched signal/wait pairs.
7. Retain a safe failure path when a second created queue is unavailable.

This is the principal portability risk. The fork requests the additional queue through an XR-oriented Unity boot setting and was verified on an Adreno 740 Quest 3. A non-XR Firebase device may not expose or create the same queue configuration. Physical cloud results, not physical queue-family capacity alone, decide whether this path generalizes.

If the dedicated-queue control fails because Unity did not create queue index 1, evaluate these fallbacks in order:

1. A CPU-fenced offscreen submission that completes before the Unity composite, used initially to prove correctness and measure cost.
2. A supported same-command-buffer implementation that retains the fork's explicit offscreen resource model but records all work into Unity's command buffer.
3. Device-gated queue strategies only if a portable default remains available.

Do not silently call `vkGetDeviceQueue` with an index Unity did not request.

### Phase 5: Composite through the original Unity camera

1. Keep the original Unity camera's physical output target under Unity ownership.
2. Add the composite to that camera's command buffer at `CameraEvent.AfterImageEffectsOpaque`, matching the fork's verified event.
3. Blit the explicit IMM texture to `BuiltinRenderTextureType.CameraTarget` using an explicit composite material.
4. Do not use `Graphics.Blit(texture, null)`; Unity may resolve `null` through `Camera.main.targetTexture`, which is ambiguous when the main camera owns an offscreen target.
5. Do not attempt to acquire, retain, or present Unity's Android swapchain image directly.
6. Confirm orientation, alpha semantics, and color space on the physical capture.

### Phase 6: Validate in the cloud after each meaningful change

Every pushed experiment must retain these gates:

1. Fast-fail build, initialization, API, and Vulkan-error checks.
2. Internal render-target capture for diagnosis.
3. Internal Unity-composition capture for diagnosis.
4. External Firebase physical-screen capture as the mandatory final gate.
5. Perceptual comparison against the approved baseline, using calibrated tolerances rather than binary pixel equality.

The first cloud milestone is simple: the external screenshot must contain the same scene already visible in the internal presentation capture. A non-black but visually incorrect image is not a pass.

### Phase 7: Restore composition correctness

Once physical presentation works:

1. Verify the render-only scene against its baseline.
2. Verify the composition scene with Unity geometry both in front of and behind IMM content.
3. Add overlapping depth probes so constant near/far depth cannot satisfy the test accidentally.
4. Remove the temporary forced-near-depth diagnostic.
5. Calibrate perceptual thresholds using known-good and known-bad captures.
6. Require all visual tests to pass on pixels; keep logs as fast-fail diagnostics only.

### Phase 8: Clean up only after proof

After both Android Unity Vulkan jobs pass their physical visual gates:

1. Remove superseded presenter-camera and direct-display experiments.
2. Remove temporary high-volume diagnostics and retain concise uniquely prefixed failure logs.
3. Document the supported Android Vulkan lifecycle and synchronization contract.
4. Confirm OpenGL, Metal, Windows Vulkan, Godot, and WASM validation have not regressed.
5. Rebuild and verify the committed Android ARM64 plug-in binary matches the final source.

## Required proof of completion

The work is complete only when all of the following are true in the same current CI revision:

- The Unity Android Vulkan render-only APK builds and launches on Firebase hardware.
- Its external Firebase screenshot contains the expected scene and perceptually matches the approved render baseline.
- The Unity Android Vulkan composition APK builds and launches on Firebase hardware.
- Its external Firebase screenshot perceptually matches the approved composition baseline, including correct front/behind depth ordering.
- No step passes solely from log messages, file existence, internal captures, or colored diagnostic geometry.
- Known-bad all-black, partial-scene, wrong-camera, wrong-depth, and diagnostic-square captures fail the visual gate.
- The workflow explicitly selects Vulkan and records the active Android graphics API in its evidence.
- The result is portable across the Firebase device coverage selected for the validation suite, or unsupported devices fail explicitly rather than rendering black while passing.

## Immediate next action

Discard the current uncommitted presenter-only experiment, create the controlled integration branch, and compare the fork's complete Android Vulkan subsystem against current `main`. The first implementation target is a close port of the fork's offscreen render, synchronization bridge, and original-camera composite while preserving the current external-screen CI gate.
