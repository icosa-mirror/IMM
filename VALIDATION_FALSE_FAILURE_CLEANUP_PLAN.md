# Validation False-Failure Cleanup Plan

## Objective

Make the validation report show failures only when the tested rendering contract is genuinely broken. Visual evidence remains authoritative. Text and log checks may fail early, but must not reject a visually proven render merely because a redundant diagnostic message was absent.

The cyan depth/occlusion defect is a genuine failure and must remain visible until corrected.

## Execution order

### Current priority order

1. Gate the user-facing Godot Run-button and Unity Editor Play entry points with runtime logs and baseline-reviewed captures. The Godot gate must catch the `load_on_ready=true` regression that bypassed Vulkan warm-up.
2. Finish the two remaining report false-failure corrections from full run `30820773061`: suppress root-level generic composition reports and allow the reviewed Unity Metal render whose spatial MAD exceeded the old limit by `0.002`.
3. Finish Android/Firebase synthetic stereo. Target priming now produces two correct IMM eye captures with distinct non-zero targets and event IDs, but the captures are byte-identical; the lane must remain red until view disparity is visible.
4. Run one full suite and manually confirm that every remaining red entry describes a real render, composition, runtime, or infrastructure defect.
5. Use the stereo lane to detect the Quest defect where one eye contains IMM strokes while the other contains only the sky sphere.

The Windows Lavapipe lane remains useful as a runtime-contract test: Unity rejecting Vulkan and falling back to Direct3D must report `runtime_failed`. It is no longer the primary route to cloud stereo evidence.

The Godot Run-button regression is now locally reproduced and fixed. The Quest-oriented Vulkan renderer had applied pipelined external-image submission to Godot even though Godot samples its intermediate image in the same compositor callback. The corrected path waits on the non-dedicated queue and restores shader-read layout before handoff. Two additional startup hazards were found and guarded: bounding-box queries during partial loading and child-count queries on non-group layers. The validation launches `project.godot` without a scene/script override, requires a clean native log, and visually compares the resulting 1280x720 frame with an animation-tolerant entry-point contract.

### 1. Finish and verify the false-failure cleanup on `main`

Full runs `30811686247`, `30813880906`, `30816894610`, and `30820773061` have been inspected. Their images show that Unity DirectX, Unity Metal full-depth, Unity Android Vulkan composition, and Godot Metal have genuine composition failures. Run `30820773061` contains all 16 supported rows and preserves both semantic eye captures. It also exposed two remaining false report signals: a generic root-level `Composition` section and a visually correct Unity Metal render rejected only because spatial MAD was `0.152` against `0.150`.

1. Index every valid manifest, including failed and expected-failed manifests, while allowing only a passed manifest to satisfy a supported row.
2. Carry non-build status manifests and their strict visual metrics through each aggregation layer so the final report retains preflight and failed-lane evidence.
3. Canonicalize `macOS` identifiers before matching reports, manifests, and matrix rows.
4. Show the manifest result and failure class in the aggregate report.
5. Suppress generic nested capture-mode sections such as `Render`, `Full depth`, and `Ordered overlay` when the authoritative parent lane is already reported.
6. Classify the Unity DirectX and Metal cyan depth failures as `compositing`, not generic `visual` failures.
7. Classify Android Unity Vulkan from its actual evidence instead of hard-coding every failure as `compositing`: app/API crashes are `runtime`, Firebase command or collection failures are `infrastructure`, absent authoritative captures are `evidence`, failed render/stereo images are `rendering`, and only a failed depth image after the other contracts pass is `compositing`.
8. Run the full suite and manually inspect the regenerated report.

Exit criterion: every report result agrees with its underlying evidence, and every remaining red entry is actionable.

### 2. Finish the Android/Firebase synthetic-stereo Vulkan lane

The immediate implementation priority is hardware-backed Android Vulkan in Firebase, reusing the existing Unity Android Vulkan build and capture infrastructure. The synthetic test exercises IMM eye routing and composition without requiring an OpenXR headset or separate-eye presentation.

Run `30820773061` proves target priming works: both split eye images independently pass the approved render baseline, and the exact native write-target pointers are non-zero and distinct. The files are nevertheless byte-identical, so the stereo disparity contract correctly fails. The next iteration uses an exaggerated validation-only eye separation and records the uploaded matrix translations, while retaining the strict requirement that the resulting images differ. Log-only success is not accepted.

1. Add a deterministic Android synthetic-stereo mode that renders two adjacent offscreen eye targets in one frame.
2. Run that mode on a Firebase device that reports and uses Vulkan.
3. Capture a side-by-side image through the existing Firebase video/evidence path.
4. Keep the test honest:
   - require `actual=Vulkan`;
   - require eye 0 and eye 1 to use the same IMM camera ID;
   - require distinct adjacent eye event IDs;
   - require distinct non-zero native render-target pointers;
   - require a side-by-side image with independently validated left and right halves;
   - never accept an API fallback or a single duplicated eye as synthetic-stereo Vulkan evidence.
5. Add a negative fixture or classifier test in which one half contains only the Unity sky/default scene; it must fail even when the other half renders IMM correctly.
6. Inspect the resulting image manually before declaring the lane valid.
7. Record the limitation in the report: this exercises IMM's Android Vulkan eye routing and composition, not the Quest/OpenXR compositor itself.

Exit criterion: the Firebase job produces recognizable IMM content in both eye images through the Vulkan path, both halves satisfy an approved visual baseline, and the combined evidence appears in the validation report.

### 3. Inventory every reported failure by failure class

Status: completed for full run `30811686247`; results are recorded in `VALIDATION_FAILURE_INVENTORY.md`.

For one exact commit and workflow run, create a table containing:

- lane and capture name;
- whether the expected image was produced;
- manual visual verdict;
- automatic visual verdict and exact failed metric;
- composition-probe verdict;
- log-contract verdict;
- infrastructure/runtime verdict;
- whether the failure is genuine, a false failure, or unresolved.

Do not change thresholds until the associated image has been inspected.

### 4. Fix log-only false failures

Status: completed. The macOS Metal lane no longer fails because the optional `Loaded in CPU` and `Loaded in GPU` diagnostic strings are absent. Explicit load errors and the mandatory visual checks remain failures.

1. Find the actual destination of native IMM logs on each platform.
2. If the markers are reliably available in another artifact, validate that artifact instead.
3. Otherwise replace them with a reliable structured load-completion marker emitted by the integration layer.
4. Treat load messages as fast-fail diagnostics. A redundant missing message must not override valid visual evidence.
5. Retain hard failures for explicit load errors, missing documents, missing captures, renderer initialization failure, or absence of recognizable IMM content.

Exit criterion: Metal does not fail solely because redundant native log strings are absent, while genuine document-load failures still fail before visual comparison.

### 5. Repair stale or mismatched visual baselines

Status: the reviewed Unity render baseline now accepts the correct DirectX, Metal, and Android Vulkan render-only captures from run `30811686247`. No baseline change is justified by that run.

1. Confirm the intended camera pose, playback timestamp, resolution, color space, and scene state for every Unity capture.
2. Generate candidate baselines only from a reviewed, correct rendering of that exact fixture.
3. Compare candidates across at least two repeat runs to measure normal nondeterminism.
4. Commit a baseline contract that tolerates normal renderer variation but rejects:
   - black or blank frames;
   - the default Unity scene without IMM content;
   - badly displaced or distant cameras;
   - missing major scene regions;
   - incorrect orientation;
   - severe color or exposure corruption.
5. Keep reference provenance in the baseline metadata: fixture, camera, freeze time, Unity version, renderer, and source run.

Exit criterion: reviewed correct images pass repeatedly, and deliberately altered black, default-scene, displaced-camera, and missing-content fixtures fail.

### 6. Rework composition probes around their actual visible geometry

Run `30811686247` shows that the front-visible and rear-visible probe checks are no longer causing the Unity failures. The remaining Unity failures are cyan leakage in regions that should be occluded. Do not broaden probe thresholds unless a reviewed, visually correct capture is rejected.

1. Derive the expected probe mask from the rendered probe geometry rather than a loose projected bounding rectangle.
2. Ignore antialiased boundary pixels and allow a small renderer-dependent edge tolerance.
3. Validate front-visible and rear-visible probes independently.
4. Use depth-aware expected masks for occluded probes rather than a single whole-rectangle color share.
5. Add negative fixtures proving that missing, swapped, fully hidden, and incorrectly foregrounded probes fail.

Exit criterion: intact probes pass on DirectX, Metal, Vulkan, and OpenGL; deliberately incorrect depth ordering fails on every applicable renderer.

### 7. Make the cyan depth defect an explicit, reliable contract

The cyan probe currently exposes an apparent depth/occlusion problem, but at least one classifier reports success. Tightening this check is more important than making the report green.

1. Define exactly which cyan pixels should be visible and which scene/IMM geometry should occlude them.
2. Produce an expected occlusion mask from a reviewed correct reference.
3. Measure leakage only inside regions that must be occluded; do not penalize cyan pixels intentionally extending beyond the occluder.
4. Report leakage count, leakage area, and a diagnostic overlay showing the unexpected pixels.
5. Add controlled negative fixtures:
   - cyan forced in front;
   - depth testing disabled;
   - IMM depth absent;
   - Unity scene depth absent.
6. Require the existing defective capture to fail the new contract before accepting the test change.

Exit criterion: the known cyan defect fails with a clear diagnostic overlay, while a reviewed correct occlusion capture passes across repeated runs.

### 8. Separate product failures from infrastructure and evidence failures

Do not label a renderer visually broken when the failure occurred before rendering.

Use distinct report states:

- `render_failed`: a produced image violates the approved visual contract;
- `composition_failed`: depth or ordering violates the composition contract;
- `runtime_failed`: the requested graphics API or player could not start;
- `infrastructure_failed`: runner, Firebase, driver, download, or tool failure;
- `evidence_incomplete`: required artifacts were not collected;
- `passed`: all required visual and supporting contracts passed.

Aggregate reports must preserve these distinctions instead of flattening all of them into a red visual failure.

### 9. Add regression tests for the validators themselves

Each validator must be tested with known positive and negative fixtures.

Required cases:

- correct render passes;
- small normal renderer variation passes;
- black frame fails;
- default Unity scene fails;
- wrong camera pose fails;
- missing IMM content fails;
- correct composition passes;
- cyan depth leakage fails;
- absent optional diagnostic marker does not override visual success;
- explicit load or renderer error fails immediately;
- requested Vulkan falling back to another API fails as a runtime failure;
- missing capture reports `evidence_incomplete`, not `render_failed`.

### 10. Verify the cleaned report on an exact revision

1. Run the full cloud validation suite.
2. Download and manually inspect every rendered capture, including both synthetic eyes.
3. Check every red report entry against its underlying evidence.
4. Confirm that all remaining failures are reproducible, correctly classified, and actionable.
5. Do not loosen a contract merely to make the report green.

## Expected outcome

The report should become shorter and more trustworthy:

- correct Metal and DirectX renders no longer appear as visual failures because of stale thresholds or redundant log markers;
- infrastructure problems are clearly separated from rendering defects;
- the synthetic-stereo lane provides two-eye Vulkan evidence without requiring a physical headset;
- the cyan depth/occlusion issue remains red until the rendering itself is corrected.

## Implementation findings

- Run `30804478423` (`82c538c038035ad373a36fb924b0ba23b2d50df2`) proved that the standard GitHub Windows runner exposes no Vulkan GPU accepted by Unity. Mesa Lavapipe was installed and selected correctly, but Unity reported no Vulkan device and fell back to Direct3D 11.
- The repository's Android native dependency graph is ARM64-only, including the imported static libraries and `libjpeg-turbo`. An x86_64 Android emulator lane therefore requires a separate native-porting task and is not a workflow-only substitute.
- The Windows software-Vulkan synthetic lane must continue to report `runtime_failed` rather than a visual failure when Unity falls back to Direct3D.
- The primary cloud stereo strategy is now an ARM64 Android/Firebase hardware-Vulkan lane, avoiding both the unsupported Windows software-Vulkan device and the x86_64 native dependency gap.
- The known Swappy failure artifact from run `30813880906` is now covered by a classifier regression test and is reported as `runtime_failed`, rather than the lane's former hard-coded `compositing` label.
- Run `30816894610` shows why complete visual evidence must outrank a secondary Firebase CLI failure: the CLI returned `1` because Cloud Tool Results API is disabled after the test, but all captures were downloaded and contain an unambiguous black-eye rendering failure. The classifier now reports that result as `render_failed` and retains the Firebase error as supporting detail.
