# Godot-First Ordered Overlay Plan

## Purpose

Use Godot as the next implementation target for the immediate compositor milestone: ordered overlay/background composition.

The agreed milestone split is:

- Immediate: ordered overlay/background composition for both Godot and Unity.
- Deferred: full depth compositing for both Godot and Unity.

Ordered overlay means IMM renders as the background or lower ordered layer, and normal host-engine elements render visibly above it by render order. Host depth is not used and is not claimed.

## Why Godot First

Godot is the better next target because it already has an explicit compositor path and existing Vulkan alpha/depth smoke infrastructure. The Godot path has a natural place to add or validate an ordered overlay mode without solving Unity's harder Vulkan render-pass, command-buffer, and MSAA ownership problems first.

This does not solve Unity Vulkan ordered overlay. It creates a proven implementation and evidence pattern for the shared milestone, then leaves Unity as the remaining host-specific Vulkan integration problem.

## Current Evidence State

- Godot Vulkan alpha composition has local smoke evidence.
- Godot Vulkan full-depth composition has local evidence, but that belongs to the deferred depth-compositing goal.
- Godot does not yet have explicit ordered-overlay CI evidence using `composition_mode=ordered_overlay`.
- Unity DirectX composition is back to passing as a render-regression guard.
- Unity Vulkan ordered overlay currently fails in CI; the evidence image is a solid magenta frame.

## Target Behavior

For the Godot ordered-overlay milestone:

- IMM content is visible as the background/lower layer.
- A normal Godot object or UI element is visibly rendered over IMM.
- Host depth is not sampled, tested, or claimed.
- Transparent IMM regions and host-background regions remain visually sane.
- The output is not blank, single-color, host-only, or IMM-only.
- The path stays GPU-native and does not rely on CPU readback, screenshot compositing, or per-frame CPU upload.

## Implementation Approach

1. Identify the current Godot compositor modes and smoke fixture wiring.
2. Add or confirm an explicit ordered-overlay mode in the Godot compositor path.
3. Build a deterministic visual fixture:
   - Stable camera pose.
   - Known IMM content in the background.
   - A saturated Godot overlay object or UI element in front.
   - Regions that can be sampled for IMM pixels, host overlay pixels, and background/transparent behavior.
4. Ensure the ordered-overlay path does not enable host-depth composition.
5. Write file-based Godot logs under `user://debug.log` or `user://xr_debug.log`; do not rely on console output.
6. Extend the Godot classifier/report path to use:
   - `composition_mode=ordered_overlay`
   - `composition_contract=ordered_overlay`
   - `ordered_overlay=success` on pass
   - `depth_composition=not_claimed`
7. Add the capture image and `composition-status.json` to the final CI visual evidence report.

## Validation

Local validation should run the Godot compositor parity smoke in ordered overlay mode, for example:

```powershell
pwsh -NoProfile -File code/projects/windows/run-godot-compositor-parity-smoke.ps1 -Configuration Release -GodotExe <path-to-godot> -Mode Overlay -SkipBuild -LogDir artifacts/godot-compositor-parity/ordered-overlay -TimeoutSeconds 90
```

Acceptance requires opening and inspecting the output image. Text logs and metrics are not enough.

The expected image should show:

- IMM picture/paint/background content.
- A clearly visible Godot overlay object or UI element above IMM.
- No full-screen replacement by solid color.
- No missing IMM content.
- No claim of host-depth interleaving.

Automated checks should verify:

- Capture dimensions are correct.
- Output is not blank or single-color.
- IMM sample regions are present.
- Godot overlay sample regions are present.
- The status JSON reports ordered overlay, not full depth.

## CI Work

Add a dedicated Godot ordered-overlay evidence lane or extend the existing Godot Vulkan lane with a separate ordered-overlay artifact.

The final report must include:

- The ordered-overlay capture image.
- The status JSON fields.
- A clear section label, for example `Windows Godot Vulkan Ordered Overlay`.
- `composition_mode: ordered_overlay`
- `ordered_overlay: success` or `failed`
- `depth_composition: not_claimed`

Do not hide a failed image. Failed ordered-overlay evidence is still useful and must remain visible in the final report.

## Non-Goals

- Full host-depth interleaving.
- Godot XR/multiview.
- Godot Metal, unless the same ordered-overlay path is explicitly validated there.
- Unity Vulkan ordered overlay.
- Unity same-camera arbitrary geometry interleaving.
- Any copy/readback path that only makes a screenshot look correct.

## Risks

- Godot's compositor timing may make it unclear whether host content is drawn before or after the IMM composite. The fixture must prove the ordering visually.
- Existing Godot depth mode could accidentally be reused and mislabeled as ordered overlay. The status fields must make `depth_composition=not_claimed` explicit.
- A fullscreen composite pass may be acceptable for Godot only if it remains GPU-native and fits the performance contract. It must not become a CPU copy path.
- Shared Vulkan renderer changes can still affect Unity. Any shared-code edit requires Unity build/smoke guards even if the feature target is Godot.

## Done Criteria

The Godot-first milestone is complete when:

- A Godot Vulkan ordered-overlay smoke produces a capture with IMM behind Godot content.
- The image has been opened and inspected against the expected layout.
- The classifier reports `composition_mode=ordered_overlay`.
- The final CI report embeds the capture image and status fields.
- The lane passes or, if it fails, the failure image and status clearly explain why.
- Unity Vulkan remains tracked separately as the next host-specific ordered-overlay target.
