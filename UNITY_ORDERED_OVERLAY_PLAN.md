# Unity Ordered Overlay Plan

## Purpose

Implement and verify the Unity plugin equivalent of the ordered overlay/background milestone.

This plan is intentionally narrower than `GODOT_UNITY_COMPOSITOR_PARITY_PLAN.md`. It covers Unity ordered overlay only. It does not include full depth compositing, XR/multiview, or arbitrary same-camera host-depth interleaving.

## Milestone Definition

Ordered overlay means:

- IMM renders as the background or lower ordered layer.
- Normal Unity content renders visibly above IMM by Unity render order.
- Unity scene depth is not sampled, tested, or claimed.
- The result is GPU-native and suitable for production architecture.

The milestone is useful when Unity objects, controls, UI, or a later overlay camera should appear over an IMM scene, without requiring depth-aware interleaving between IMM and Unity geometry.

## Deferred Work

The following are explicitly deferred:

- Full depth compositing between IMM and Unity scene geometry.
- Same-camera arbitrary geometry interleaving by Unity depth.
- XR/multiview support.
- Unity Android Vulkan/OpenXR runtime validation unless shared code touched by this work reaches that path.
- CPU readback, screenshot compositing, per-frame CPU texture upload, forced full-frame resolve, or fullscreen copy paths as the accepted implementation.

Diagnostic versions of those paths may be used to isolate failures, but they are not acceptance evidence for this milestone.

## Current Starting Point

From the current parity plan and CI state:

- Unity DirectX composition is a regression guard and should continue to pass.
- Unity Vulkan ordered overlay has CI coverage, but the current CI lane fails.
- Godot Vulkan ordered overlay is separately implemented and verified.
- Local Unity Vulkan experiments have shown useful overlay-camera evidence, but local evidence alone is not enough for this milestone.
- Earlier Unity Vulkan images with blank frames, host-only frames, missing paint, bad depth/order, flipped captures, or host-depth opt-in do not prove this milestone.

## Target Behavior

The accepted Unity Vulkan ordered-overlay output must show:

- IMM picture/background content.
- IMM paint content where expected.
- Unity content visibly rendered above IMM.
- No claim of Unity depth interleaving.
- No blank, single-color, Unity-only, IMM-only, flipped, or obviously mis-composited frame.
- Normal Unity Vulkan SampleScene/MSAA configuration unless this plan is explicitly amended to accept a narrower configuration.

The preferred route is a GPU-native Unity render-order solution:

- Render IMM in a Unity-controlled render context at the correct point for a background layer.
- Let Unity render overlay content after IMM.
- Keep Unity in charge of MSAA resolve, render ordering, synchronization, and presentation.

The practical overlay-camera route may be accepted only if the CI evidence makes the contract explicit: base camera renders IMM as background, later Unity overlay camera renders host elements above it, and the status says this is ordered overlay rather than depth composition.

## Implementation Requirements

Unity Vulkan requirements:

- Use Unity's Vulkan plugin API or an equivalent Unity-owned render context for render-target access.
- Avoid per-draw queue submit/wait behavior as the accepted production path.
- Avoid CPU readback or screenshot-style compositing.
- Avoid forcing MSAA off for acceptance.
- Keep host depth disabled by default for this mode.
- Make any host-depth path opt-in and report it separately from ordered overlay.
- Preserve DirectX/D3D11 behavior and capture orientation.
- Gate Unity-specific behavior at the Unity host boundary unless a shared renderer change is deliberately required.

If shared IMM renderer, player, or shader code is changed, validate or explicitly guard other reachable hosts:

- Unity DirectX/D3D11.
- Unity Vulkan.
- Godot Vulkan where shared Vulkan code is reachable.
- Android GLES/Vulkan builds where shared code is reachable.

## Evidence Requirements

Completion requires automated CI evidence, not only local screenshots.

The final CI visual evidence report must include a Unity Vulkan ordered-overlay section with:

- The capture image embedded in the report.
- `composition_mode: ordered_overlay`
- `composition_contract: ordered_overlay`
- `ordered_overlay: success`
- `depth_composition: not_claimed`
- `depth_interleaving: not_claimed`
- `rendering: success`
- no composition failures

The image must be opened and visually inspected before claiming completion.

The expected image must show IMM content behind Unity content. Text logs, nonblank pixel counts, color bucket counts, and draw-call counts are not sufficient by themselves.

## CI Requirements

Required CI outcomes:

- Unity Vulkan ordered-overlay lane passes.
- Unity DirectX composition lane passes as a regression guard.
- Final visual evidence report is generated and embeds the Unity Vulkan ordered-overlay image.
- The status JSON for Unity Vulkan ordered overlay reports success for ordered overlay and does not claim depth composition.

The overall workflow may still contain unrelated deferred or expected-failure lanes, but any failure in the Unity Vulkan ordered-overlay lane blocks this milestone.

## Local Validation

Local validation is useful before CI, but not sufficient for completion.

A local run should:

- Use the Unity Vulkan editor/player smoke path.
- Use normal SampleScene/MSAA settings unless explicitly testing a diagnostic.
- Capture a PNG.
- Write status JSON using the shared composition vocabulary.
- Open and inspect the capture.
- Compare Unity logs against the current clock time so stale errors are not treated as current results.
- Use a unique log prefix for any new diagnostic logging.

Local diagnostic images should be labeled as diagnostic unless they meet the acceptance contract.

## Non-Goals

- Proving full compositor parity.
- Proving XR or multiview.
- Proving Android Unity Vulkan runtime behavior.
- Accepting a host-depth image as ordered overlay evidence.
- Accepting a no-MSAA diagnostic as normal Unity Vulkan ordered-overlay evidence.
- Accepting a CPU copy/readback workaround.
- Refactoring unrelated rendering code.

## Done Criteria

This milestone is complete only when all of the following are true:

- Unity Vulkan ordered-overlay implementation is committed.
- Unity Vulkan ordered-overlay CI lane passes.
- Unity DirectX composition CI lane passes.
- Final CI visual evidence report embeds the Unity Vulkan ordered-overlay capture.
- The capture has been opened and visually inspected.
- The capture shows IMM picture/background and paint behind Unity overlay content.
- Status JSON reports `composition_mode=ordered_overlay`.
- Status JSON reports `ordered_overlay=success`.
- Status JSON reports `depth_composition=not_claimed`.
- Status JSON reports `depth_interleaving=not_claimed`.
- The implementation does not depend on CPU readback, screenshot compositing, per-frame CPU upload, forced MSAA disablement, or a full-frame copy path as the accepted solution.
- Any shared-code changes have appropriate regression validation or explicit host-boundary gating.

If any item above is missing, failed, or only indirectly proven, the milestone is not complete.

