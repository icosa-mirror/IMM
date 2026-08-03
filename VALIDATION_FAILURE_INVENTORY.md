# Validation Failure Inventory

Reference run: `30813880906` (`mode=full`)

Revision: `175bf082fd4f3c89b95081c2c1abe35d5b548950`

This inventory records the evidence that motivated the validator cleanup. A red workflow result is not itself a render verdict.

| Lane | Image produced | Manual visual verdict | Automatic failure | Classification | Action |
| --- | --- | --- | --- | --- | --- |
| Unity Windows DirectX composition | Yes | Render baseline is correct; only cyan depth leakage is wrong | Render status `success`; rear cyan matched `439` pixels and exceeds the occlusion limit | `composition_failed` (genuine); manifest incorrectly said `visual` | Keep red and classify the manifest as `compositing` |
| Unity macOS Metal render | Yes | Render baseline is correct | MAD `0.143`; correlation `0.444` missed the `0.450` threshold, while the previous reviewed run was only `0.451` | False threshold failure caused by measured run-to-run variation | Lower the correlation floor to `0.400`; production-contract tests still reject a shifted camera and default scene |
| Unity macOS Metal full depth | Yes | Render baseline is correct; cyan depth leakage is wrong | Rear cyan remains visible through the character | `composition_failed` (genuine) after the render threshold is corrected | Keep red and classify the manifest as `compositing` |
| Unity macOS Metal ordered overlay | Yes | IMM content and ordered probes are correct | Ordered-overlay render metrics and composition status both passed | Passed subtest inside a red full-depth job | Preserve the passing ordered-overlay evidence instead of flattening the whole job to a visual failure |
| Unity macOS Metal diagnostic logs | N/A | Visual evidence independently proves rendering | `Loaded in CPU` and `Loaded in GPU` are absent | Optional diagnostics; not a failure | Implemented: absence does not fail, while explicit load errors still fail fast |
| Unity Windows Vulkan synthetic stereo | No | No visual verdict is possible | Unity rejected Lavapipe and reported `actual=Direct3D11` | `runtime_failed`, not `render_failed` | Require real Vulkan; report runner capability failure distinctly; do not accept fallback evidence |
| Unity Android Vulkan | First Firebase process produced render, composition, and synthetic-stereo captures; physical video contains a correct IMM render | Synthetic stereo completed for both eyes in the log; composition still has cyan leakage | Firebase relaunched the Unity 6 activity and crashed in `SwappyVk_setAutoSwapInterval`; the final pull therefore omitted the three PNGs and the final Robo screenshot shows the default scene | Runtime/presentation crash after valid first-process evidence, plus a genuine composition defect | Disable optimized frame pacing only in the non-XR CI smoke build, validate the native eye-target pointers, and rerun; do not accept the log-only stereo result without both eye images |
| Godot Windows Vulkan | No | No visual verdict is possible | Lavapipe runtime started but produced no required capture | `runtime_failed` | Keep separate from rendering correctness |
| Godot macOS Metal | Yes | Render-only capture is correct; composition capture lacks the required magenta and yellow probes | Render MAD `0.073`, correlation `0.862`, strict render status passed; front/rear-visible probe counts are zero | `composition_failed` (genuine) | Keep red; do not loosen the render baseline or hide the missing composition geometry |
| Android Godot Vulkan | No valid final capture | No visual verdict is possible | Device/native failure before capture | `runtime_failed` | Keep separate from rendering correctness |
| Aggregate evidence jobs | N/A | N/A | Report still says the passed Unity preflight and Unity Metal/Windows Vulkan lanes are “missing evidence”; Unity Metal loses its failed manifest and generic nested sections reappear | False aggregation diagnosis | Preserve exact status manifests and strict metrics across nested reports, canonicalize macOS identity, remove the ambiguous engine-wide manifest, and suppress generic capture-mode sections |

The known cyan occlusion defect is intentionally not waived by this cleanup. The target state is a smaller set of red entries whose labels describe the actual failure mode.
