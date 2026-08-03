# Validation Failure Inventory

Reference runs: `30813880906` and `30816894610` (`mode=full`)

Latest revision: `6659c4c9d37267f9b5891fb1ce826466d5822179`

This inventory records the evidence that motivated the validator cleanup. A red workflow result is not itself a render verdict.

| Lane | Image produced | Manual visual verdict | Automatic failure | Classification | Action |
| --- | --- | --- | --- | --- | --- |
| Unity Windows DirectX composition | Yes | Render baseline is correct; only cyan depth leakage is wrong | Render status `success`; rear cyan matched `439` pixels and exceeds the occlusion limit | `composition_failed` (genuine); manifest incorrectly said `visual` | Keep red and classify the manifest as `compositing` |
| Unity macOS Metal render | Yes | Render baseline is correct | MAD `0.143`; correlation `0.444` missed the `0.450` threshold, while the previous reviewed run was only `0.451` | False threshold failure caused by measured run-to-run variation | Lower the correlation floor to `0.400`; production-contract tests still reject a shifted camera and default scene |
| Unity macOS Metal full depth | Yes | Render baseline is correct; cyan depth leakage is wrong | Rear cyan remains visible through the character | `composition_failed` (genuine) after the render threshold is corrected | Keep red and classify the manifest as `compositing` |
| Unity macOS Metal ordered overlay | Yes | IMM content and ordered probes are correct | Ordered-overlay render metrics and composition status both passed | Passed subtest inside a red full-depth job | Preserve the passing ordered-overlay evidence instead of flattening the whole job to a visual failure |
| Unity macOS Metal diagnostic logs | N/A | Visual evidence independently proves rendering | `Loaded in CPU` and `Loaded in GPU` are absent | Optional diagnostics; not a failure | Implemented: absence does not fail, while explicit load errors still fail fast |
| Unity Windows Vulkan synthetic stereo | No | No visual verdict is possible | Unity rejected Lavapipe and reported `actual=Direct3D11` | `runtime_failed`, not `render_failed` | Require real Vulkan; report runner capability failure distinctly; do not accept fallback evidence |
| Unity Android Vulkan | Run `30816894610` produced a correct normal render and a composition capture; both synthetic eye images are black and byte-identical | Normal Vulkan rendering passes; synthetic stereo fails; composition still has cyan leakage | Swappy no longer crashes. Both exact native eye write-target pointers are `0x0` because the validation changes the offscreen size and queries each newly created RenderTexture before Unity first binds it. The Firebase CLI also returns `1` after the test because Cloud Tool Results API is disabled, despite successfully downloading all captures. | `render_failed` for the black stereo eyes, with genuine composition and secondary Firebase reporting failures; the old `infrastructure_failed` primary label was masking stronger visual evidence | Prime each internal eye RenderTexture before dispatch, keep independent eye comparisons, and make complete visual evidence outrank a post-test Firebase reporting error |
| Godot Windows Vulkan | No | No visual verdict is possible | Lavapipe runtime started but produced no required capture | `runtime_failed` | Keep separate from rendering correctness |
| Godot macOS Metal | Yes | Render-only capture is correct; composition capture lacks the required magenta and yellow probes | Render MAD `0.073`, correlation `0.862`, strict render status passed; front/rear-visible probe counts are zero | `composition_failed` (genuine) | Keep red; do not loosen the render baseline or hide the missing composition geometry |
| Android Godot Vulkan | No valid final capture | No visual verdict is possible | Device/native failure before capture | `runtime_failed` | Keep separate from rendering correctness |
| Aggregate evidence jobs | N/A | N/A | Run `30816894610` reports all 16 supported rows with evidence and correctly preserves Unity preflight and Metal status, but still emits one generic `Composition` section and drops the synthetic right-eye image when it is byte-identical to the left | Remaining report presentation defects | Suppress generic reports found under `captures`, deduplicate only equal files with the same semantic filename, and retain both named eye images even when identical |

The known cyan occlusion defect is intentionally not waived by this cleanup. The target state is a smaller set of red entries whose labels describe the actual failure mode.
