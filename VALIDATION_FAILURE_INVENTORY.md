# Validation Failure Inventory

Reference run: `30811686247` (`mode=full`)

Revision: `300f8fdc457241c5ca32f3afeb88ac99208ce5b3`

This inventory records the evidence that motivated the validator cleanup. A red workflow result is not itself a render verdict.

| Lane | Image produced | Manual visual verdict | Automatic failure | Classification | Action |
| --- | --- | --- | --- | --- | --- |
| Unity Windows DirectX composition | Yes | Render baseline is correct; only cyan depth leakage is wrong | Render status `success`; rear cyan matched `439` pixels and exceeds the occlusion limit | `composition_failed` (genuine); manifest incorrectly said `visual` | Keep red and classify the manifest as `compositing` |
| Unity macOS Metal full depth | Yes | Render baseline is correct; cyan depth leakage is wrong | Strict render comparison passed; rear cyan matched `1759` pixels, share `0.002` | `composition_failed` (genuine); manifest incorrectly said `visual` | Keep red and classify the manifest as `compositing` |
| Unity macOS Metal ordered overlay | Yes | IMM content and ordered probes are correct | Ordered-overlay render metrics and composition status both passed | Passed subtest inside a red full-depth job | Preserve the passing ordered-overlay evidence instead of flattening the whole job to a visual failure |
| Unity macOS Metal diagnostic logs | N/A | Visual evidence independently proves rendering | `Loaded in CPU` and `Loaded in GPU` are absent | Optional diagnostics; not a failure | Implemented: absence does not fail, while explicit load errors still fail fast |
| Unity Windows Vulkan synthetic stereo | No | No visual verdict is possible | Unity rejected Lavapipe and reported `actual=Direct3D11` | `runtime_failed`, not `render_failed` | Require real Vulkan; report runner capability failure distinctly; do not accept fallback evidence |
| Unity Android Vulkan | Yes, including pulled render/composition files and physical-device video | Render-only and external render images are correct; composition has cyan leakage across the character | Render and external-render strict metrics passed; rear cyan share `0.000348` exceeds maximum `0.000150` | `composition_failed` (genuine) | Keep red; preserve the passing render evidence and show only the cyan composition defect as the reason |
| Godot Windows Vulkan | No | No visual verdict is possible | Lavapipe runtime started but produced no required capture | `runtime_failed` | Keep separate from rendering correctness |
| Godot macOS Metal | Yes | Render-only capture is correct; composition capture lacks the required magenta and yellow probes | Render MAD `0.073`, correlation `0.862`, strict render status passed; front/rear-visible probe counts are zero | `composition_failed` (genuine) | Keep red; do not loosen the render baseline or hide the missing composition geometry |
| Android Godot Vulkan | No valid final capture | No visual verdict is possible | Device/native failure before capture | `runtime_failed` | Keep separate from rendering correctness |
| Aggregate evidence jobs | N/A | N/A | Existing report says failed child manifests are “missing evidence” | False aggregation diagnosis | Index failed manifests as present evidence and report their `runtime` or `compositing` class while keeping the aggregate red |

The known cyan occlusion defect is intentionally not waived by this cleanup. The target state is a smaller set of red entries whose labels describe the actual failure mode.
