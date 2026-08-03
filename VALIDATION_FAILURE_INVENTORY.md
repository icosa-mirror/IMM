# Validation Failure Inventory

Reference run: `30804478423`  
Revision: `82c538c038035ad373a36fb924b0ba23b2d50df2`

This inventory records the evidence that motivated the validator cleanup. A red workflow result is not itself a render verdict.

| Lane | Image produced | Manual visual verdict | Automatic failure | Classification | Action |
| --- | --- | --- | --- | --- | --- |
| Unity Windows DirectX composition | Yes | IMM render and probes are visible | Front magenta share `0.623`; rear yellow share `0.493`; render baseline MAD/correlation mismatch | False composition failure plus stale/mismatched baseline | Analyze the projected quad rather than its bounding rectangle; establish a Unity-specific frozen baseline from repeated reviewed runs |
| Unity macOS Metal full depth | Yes | IMM content is visible; cyan depth behavior remains under review | Missing `Loaded in CPU` and `Loaded in GPU` strings | False log-only failure | Keep visual and explicit error checks mandatory; record the two native strings as optional diagnostics |
| Unity macOS Metal ordered overlay | Yes | IMM content and ordered probes are visible | Lane inherited job failure from the redundant full-depth log contract | False aggregate failure | Same log-contract correction; preserve the ordered-overlay visual probe |
| Unity Windows Vulkan synthetic stereo | No | No visual verdict is possible | Unity rejected Lavapipe and reported `actual=Direct3D11` | `runtime_failed`, not `render_failed` | Require real Vulkan; report runner capability failure distinctly; do not accept fallback evidence |
| Unity Android Vulkan | Yes in Firebase evidence | Render image is recognizable; composition/depth remains the relevant product check | Composition/baseline checks | Genuine or unresolved composition failure | Keep red where the cyan occlusion contract fails; do not infer render failure from infrastructure or missing logs |
| Godot Windows Vulkan | Incomplete | No valid final visual verdict for the missing captures | Hosted job did not produce all required evidence | `evidence_incomplete` or `runtime_failed` | Do not label as a bad render until a capture exists |
| Godot macOS Metal | Yes | Capture requires separate visual review; probe colors were not found by the classifier | Probe classifier failure | Unresolved | Apply the same geometry-mask rules before changing thresholds |
| Android Godot Vulkan | No valid final capture | No visual verdict | Device/native crash | `runtime_failed` | Keep separate from rendering correctness |
| Aggregate evidence jobs | N/A | N/A | Required child evidence was absent or red | Evidence aggregation, not a renderer failure | Preserve child classifications instead of flattening them to visual failure |

The known cyan occlusion defect is intentionally not waived by this cleanup. The target state is a smaller set of red entries whose labels describe the actual failure mode.
