# Full Depth Compositor Goal

## Use This As The Goal

Objective: implement and validate full depth compositing between IMM content and host-engine content for both Unity and Godot, using `GODOT_UNITY_COMPOSITOR_PARITY_PLAN.md` as the detailed plan.

Suggested goal wording:

`Implement and validate FULL_DEPTH_COMPOSITOR_GOAL.md.`

This goal is complete only when the Done Criteria below are fully satisfied.

## Scope

Full depth compositing means:

- Unity and Godot host geometry can occlude IMM content when the host geometry is closer.
- IMM content remains visible when IMM content is closer than host geometry.
- The result is real depth interleaving, not ordered overlay, screenshot compositing, host-only rendering, IMM-only rendering, or a diagnostic copy path.
- Evidence distinguishes full depth from the already completed ordered-overlay milestone.

The detailed implementation history, platform notes, and regression risks live in `GODOT_UNITY_COMPOSITOR_PARITY_PLAN.md`. If this document and that plan conflict, this document defines the goal contract and `GODOT_UNITY_COMPOSITOR_PARITY_PLAN.md` supplies the technical detail.

## Done Criteria

This goal is complete only when all of the following are true:

- Unity full-depth compositor implementation is committed.
- Godot full-depth compositor implementation is committed.
- Unity has automated CI or runtime validation that explicitly tests depth interleaving.
- Godot has automated CI or runtime validation that explicitly tests depth interleaving.
- Host geometry visibly occludes IMM where host geometry is closer.
- IMM visibly remains in front where IMM content is closer than host geometry.
- Final testing evidence includes Unity and Godot capture images.
- Both capture images have been opened and visually inspected against the expected depth layout.
- Each status output reports `composition_mode=full_depth`.
- Each status output reports `depth_composition=success`.
- Each status output reports `depth_interleaving=success`.
- Ordered-overlay evidence is not reused as proof of full depth compositing.
- Unity DirectX and existing non-Vulkan regression guards still pass or any change is explicitly scoped and justified.
- Platform impacts for XR/multiview, Metal, Android Vulkan/OpenXR, and Android GLES are validated or explicitly documented as unsupported/deferred for this full-depth milestone.
- The accepted implementation does not depend on CPU readback, screenshot compositing, per-frame CPU texture upload, forced MSAA disablement, or an unprofiled full-frame copy path.
- Performance risk has been addressed with measurement or concrete implementation-specific bandwidth/synchronization analysis.

If any item is missing, failed, visually wrong, or only indirectly inferred from logs or pixel counts, this goal is not complete.

## Not Completion Evidence

The following do not complete this goal:

- Ordered-overlay captures.
- A green workflow with visually wrong images.
- Captures that are blank, single-color, flipped, host-only, or IMM-only.
- A status JSON that says success while the image contradicts it.
- A no-MSAA diagnostic reported as normal Vulkan evidence.
- A host-depth diagnostic for one engine reported as cross-engine full-depth completion.
- Local-only evidence without matching committed validation coverage.
