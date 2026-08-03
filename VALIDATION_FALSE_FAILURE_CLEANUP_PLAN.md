# Validation False-Failure Cleanup Plan

## Objective

Make the validation report show failures only when the tested rendering contract is genuinely broken. Visual evidence remains authoritative. Text and log checks may fail early, but must not reject a visually proven render merely because a redundant diagnostic message was absent.

The cyan depth/occlusion defect is a genuine failure and must remain visible until corrected.

## Execution order

### 1. Finish the hosted synthetic-stereo Vulkan lane

This is the immediate priority. Do not begin broad validation-threshold changes until this lane produces inspectable left/right Vulkan evidence.

1. Determine why Unity 2022.3 rejects the hosted Mesa Lavapipe ICD and falls back to Direct3D 11.
2. Test the smallest viable hosted Vulkan configuration:
   - verify the ICD independently before launching Unity;
   - capture Unity's Vulkan initialization diagnostics;
   - ensure the player build contains Vulkan shader variants;
   - disable automatic graphics-API fallback so Vulkan initialization failure is explicit;
   - use a supported software Vulkan implementation or a different hosted runner image if Lavapipe cannot satisfy Unity.
3. Keep the test honest:
   - require `actual=Vulkan`;
   - require eye 0 and eye 1 to use the same IMM camera ID;
   - require distinct adjacent eye event IDs;
   - require distinct non-zero native render-target pointers;
   - require a side-by-side image with independently validated left and right halves;
   - never accept Direct3D fallback as synthetic-stereo Vulkan evidence.
4. Inspect the resulting image manually before declaring the lane valid.
5. Record the limitation in the report: this exercises IMM's Vulkan eye routing and composition, not the Quest/OpenXR compositor.

Exit criterion: the hosted job produces recognizable IMM content in both eye images through the Vulkan path, both halves satisfy an approved visual baseline, and the combined evidence appears in the validation report.

### 2. Inventory every reported failure by failure class

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

### 3. Fix log-only false failures

The current macOS Metal job is red because `Loaded in CPU` and `Loaded in GPU` are missing from the selected Unity player log even though the captured IMM scene proves that CPU loading and GPU upload occurred.

1. Find the actual destination of native IMM logs on each platform.
2. If the markers are reliably available in another artifact, validate that artifact instead.
3. Otherwise replace them with a reliable structured load-completion marker emitted by the integration layer.
4. Treat load messages as fast-fail diagnostics. A redundant missing message must not override valid visual evidence.
5. Retain hard failures for explicit load errors, missing documents, missing captures, renderer initialization failure, or absence of recognizable IMM content.

Exit criterion: Metal does not fail solely because redundant native log strings are absent, while genuine document-load failures still fail before visual comparison.

### 4. Repair stale or mismatched visual baselines

The current Windows DirectX render is visually plausible but is compared with a baseline whose spatial distribution no longer matches the current Unity camera and scene presentation.

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

### 5. Rework composition probes around their actual visible geometry

The current DirectX probe rejects visually intact magenta and yellow squares because its projected analysis rectangles include substantial pixels outside the visible square. Fixed dominant-color shares are therefore measuring projection/coverage mismatch as though it were composition failure.

1. Derive the expected probe mask from the rendered probe geometry rather than a loose projected bounding rectangle.
2. Ignore antialiased boundary pixels and allow a small renderer-dependent edge tolerance.
3. Validate front-visible and rear-visible probes independently.
4. Use depth-aware expected masks for occluded probes rather than a single whole-rectangle color share.
5. Add negative fixtures proving that missing, swapped, fully hidden, and incorrectly foregrounded probes fail.

Exit criterion: intact probes pass on DirectX, Metal, Vulkan, and OpenGL; deliberately incorrect depth ordering fails on every applicable renderer.

### 6. Make the cyan depth defect an explicit, reliable contract

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

### 7. Separate product failures from infrastructure and evidence failures

Do not label a renderer visually broken when the failure occurred before rendering.

Use distinct report states:

- `render_failed`: a produced image violates the approved visual contract;
- `composition_failed`: depth or ordering violates the composition contract;
- `runtime_failed`: the requested graphics API or player could not start;
- `infrastructure_failed`: runner, Firebase, driver, download, or tool failure;
- `evidence_incomplete`: required artifacts were not collected;
- `passed`: all required visual and supporting contracts passed.

Aggregate reports must preserve these distinctions instead of flattening all of them into a red visual failure.

### 8. Add regression tests for the validators themselves

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

### 9. Verify the cleaned report on an exact revision

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
- Until hosted GPU capacity or an x86_64 Android plugin exists, the hosted synthetic lane must report `runtime_failed` rather than a visual failure. Its strict two-eye Vulkan contract remains unchanged and cannot pass on a Direct3D fallback.
