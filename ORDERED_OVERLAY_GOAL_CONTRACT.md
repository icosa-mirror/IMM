# Ordered Overlay Goal Contract

## Use This As The Goal

Objective: complete the immediate ordered-overlay compositor milestone for both Unity and Godot.

This goal is complete only when the Done Criteria section below is fully satisfied. A partial Unity-only result, a partial Godot-only result, local-only evidence, or a visually wrong CI capture means the goal is still incomplete.

The active target is ordered overlay/background composition:

- IMM is the background or lower ordered layer.
- Unity or Godot content is rendered visibly above IMM by host render order.
- Full depth interleaving is not part of this goal.
- XR/multiview and mobile runtime parity are not part of this goal unless a shared-code change requires regression validation or explicit gating.

## Purpose

This document is the goal contract for the immediate compositor milestone across Unity and Godot.

Use this document when the active goal is ordered overlay/background composition. Do not use it as a contract for full depth compositing, XR/multiview, Android runtime parity, or same-camera arbitrary host-depth interleaving.

Detailed supporting plans:

- `GODOT_UNITY_COMPOSITOR_PARITY_PLAN.md`
- `GODOT_ORDERED_OVERLAY_FIRST_PLAN.md`
- `UNITY_ORDERED_OVERLAY_PLAN.md`

If those plans conflict with this document, this document defines the completion criteria for the immediate ordered-overlay goal.

## Milestone Definition

Ordered overlay means:

- IMM renders as the background or lower ordered layer.
- Normal Unity or Godot content renders visibly above IMM by host render order.
- Host scene depth is not used.
- Host scene depth is not claimed.
- The implementation is GPU-native and suitable for a production rendering path.

The milestone is valuable because it allows Unity or Godot controls, objects, UI, and overlay content to appear over an IMM scene before full depth interleaving is solved.

## Deferred Goals

The following are explicitly deferred:

- Full depth compositing between IMM and host geometry.
- Host geometry and IMM geometry interleaving by depth.
- Same-camera arbitrary Unity geometry depth interleaving.
- XR and multiview compositor behavior.
- Godot Metal runtime validation.
- Unity Android Vulkan/OpenXR runtime validation.
- Android GLES runtime validation beyond build or smoke guards required by shared-code changes.

Deferred work must not block this milestone, but it also must not be claimed as complete by ordered-overlay evidence.

## Required Behavior

A valid ordered-overlay result must show:

- IMM picture or 360 background content.
- IMM paint content where the fixture expects paint.
- Host-engine content visibly above IMM.
- Correct image orientation.
- A coherent composed frame, not a blank, single-color, host-only, IMM-only, flipped, or visibly mis-composited image.

The result must not depend on:

- CPU readback.
- Screenshot compositing.
- Per-frame CPU texture upload.
- Forced MSAA disablement.
- An unprofiled full-frame copy path.
- A host-depth mode mislabeled as ordered overlay.

Diagnostic captures may use those approaches to isolate failures, but they are not acceptance evidence.

## Required Evidence

Completion requires CI evidence and visual inspection.

For each accepted Unity and Godot ordered-overlay lane, the final CI evidence report must include:

- An embedded capture image.
- A `composition-status.json` or equivalent status block.
- `composition_mode=ordered_overlay`.
- `composition_contract=ordered_overlay`.
- `ordered_overlay=success`.
- `depth_composition=not_claimed`.
- `depth_interleaving=not_claimed`.
- `rendering=success`.

The capture image must be opened and inspected before claiming completion.

Logs, draw-call counts, nonblank pixel counts, and color-bucket counts are useful diagnostics, but they are not sufficient by themselves. A false-positive status JSON with a visually wrong image is a failed milestone.

## Required CI Coverage

The ordered-overlay milestone is complete only when CI proves all of the following:

- Godot Vulkan ordered overlay passes or reports a clearly diagnosed failure with image evidence.
- Unity Vulkan ordered overlay passes or reports a clearly diagnosed failure with image evidence.
- The final visual evidence report embeds the ordered-overlay captures.
- Unity DirectX composition passes as a regression guard.
- Existing Godot Vulkan alpha/depth lanes keep their expected behavior.
- Any shared renderer, player, shader, package, or fixture change has appropriate regression validation or explicit host-boundary gating.

If either Unity Vulkan ordered overlay or Godot Vulkan ordered overlay is still failed, the cross-engine ordered-overlay goal is not complete.

## Platform And Performance Constraints

The accepted implementation must be credible for production use, including mobile-class hardware where that path is intended to run.

Before accepting a copy, resolve, intermediate texture, or extra compositor pass as production behavior, the evidence must include one of:

- Measurement on representative hardware.
- Frame-debugger, RenderDoc, AGI, or equivalent GPU evidence.
- A concrete bandwidth and synchronization analysis tied to the actual implementation.

Correctness without a credible performance path is not enough for this milestone.

Shared-code changes must be treated as cross-platform until proven otherwise. Changes under shared renderer, player, generated shader, or package code require validation or explicit gating for affected hosts.

## Done Criteria

This goal is complete only when all of the following are true:

- Unity Vulkan ordered-overlay implementation is committed.
- Godot Vulkan ordered-overlay implementation is committed.
- Unity Vulkan ordered-overlay CI lane passes.
- Godot Vulkan ordered-overlay CI lane passes.
- Unity DirectX composition CI lane passes.
- The final CI evidence report embeds Unity and Godot ordered-overlay captures.
- Both captures have been opened and visually inspected.
- Both captures show IMM picture/background and paint behind host overlay content.
- Both status outputs report `composition_mode=ordered_overlay`.
- Both status outputs report `composition_contract=ordered_overlay`.
- Both status outputs report `ordered_overlay=success`.
- Both status outputs report `depth_composition=not_claimed`.
- Both status outputs report `depth_interleaving=not_claimed`.
- The implementation does not depend on CPU readback, screenshot compositing, per-frame CPU upload, forced MSAA disablement, or an unaccepted full-frame copy path.
- Any shared-code change has suitable regression validation or explicit host-boundary gating.
- Performance risk has been addressed with measurement or a concrete implementation-specific analysis.

If any item is missing, failed, visually wrong, or only indirectly inferred, this goal is not complete.

## Not Completion Evidence

The following do not complete this goal:

- A green workflow with a visually wrong capture.
- A capture that shows only host content.
- A capture that shows only IMM content.
- A blank or single-color capture.
- A flipped capture.
- A host-depth image reported as ordered overlay.
- A no-MSAA diagnostic reported as normal Vulkan evidence.
- Local-only evidence without matching CI report coverage.
- A status JSON that says success while the image contradicts it.
