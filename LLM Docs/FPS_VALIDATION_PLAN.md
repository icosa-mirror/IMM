# Validation Matrix FPS Gating Plan

## Objective

Catch gross runtime performance regressions in the existing validation matrix,
including a slowdown such as the Unity Windows D3D12 sample falling to 10-12 fps
while visual correctness tests still pass. Publish attributable sustained frame
measurements and fail representative-hardware lanes when calibrated limits are
missed.

The first delivery is a working performance check on the normal Unity sample,
with a matched local D3D12/Vulkan measurement. Build only the shared schema,
verification and reporting needed for that delivery, then extend it across the
existing matrix. This plan does not remove platforms, rendering backends or
existing correctness gates. It does not require consolidating the sample scenes.

Regression gates compare the same workload and hardware configuration over time.
A separate controlled local comparison can compare backends on the same machine;
results from unlike products, devices or CI runners must not be ranked together.

## Current State and Coverage Gap

1. `tests/matrix_status.json` is the source of truth for supported rows. Determine
   coverage from that file rather than hard-coding a row count in this plan.
2. Existing visual, composition and package checks establish their tested
   correctness contracts, but do not establish acceptable sustained performance.
3. The standalone viewer has an instantaneous window-title FPS counter, and the
   web player has a sustained performance test. Unity and Godot lanes do not yet
   publish a common authoritative performance artifact.
4. Some Godot smoke commands use `--fixed-fps 30`; synthetic timing is unsuitable
   for performance evidence.
5. A hidden Unity player can run updates while its camera renders no frames. Native
   GPU loading can also require render-thread work before readiness. An Update
   counter or unscaled delta alone therefore cannot establish rendering FPS.
6. Hosted software-renderer and simulator measurements cannot establish real-GPU
   performance. Representative Windows GPU coverage must be available before
   claiming CI protects against the observed D3D12 regression. A local hardware
   baseline is useful but does not fill that CI coverage gap.

## Coverage and Gating Policy

1. Every supported matrix row explicitly declares one performance mode:
   `not_implemented`, `report_only`, `gated` or `not_applicable`.
2. `not_implemented` means a renderable row still lacks measurement coverage. It
   remains visibly incomplete in reports; it is never silently treated as not
   applicable. During staged rollout it does not fail unrelated existing lanes.
   Once a row is enabled in the required scope, missing evidence is a failure.
3. `not_applicable` requires a reason, such as package preflight or application
   build without runtime execution. Deferred or unsupported rows keep their
   existing matrix classification and are not required to produce measurements.
4. `report_only` requires valid, attributable evidence from enabled lanes but does
   not enforce speed thresholds. Missing, malformed or mismatched evidence is a
   collection/validation failure, not a low-FPS failure. A slow valid sample is
   reported without failing the lane.
5. `gated` requires representative runtime hardware and calibrated per-row limits.
   Missing or invalid evidence and threshold misses fail the required lane.
6. Simulators, software/Lavapipe renderers and GPU-less hosted runners remain
   report only. Their results must identify the actual device and renderer; they
   cannot substitute for hardware coverage or silently promote to hard gating.
7. Preserve every existing supported platform and correctness gate. Extend
   measurement incrementally to Windows, macOS, iOS, Android and web, including
   the renderer variants already required by the matrix.
8. Use existing runtime jobs and artifact collection. Do not create a second
   complete matrix or duplicate long builds merely for FPS collection.

## Workload Identity

A matrix row identifies a product/platform/mode family. Each measurement must also
identify its exact workload and rendering path:

1. Actual graphics API, distinguishing D3D11 from D3D12; engine version and render
   pipeline, distinguishing Built-in from URP; MSAA and other material settings.
2. Actual GPU/device model, driver version when available, OS and build identity.
3. Fixture filename and content hash, pinned camera pose with explicit values or
   a versioned pose definition, and animation time or a deterministic playback
   interval. Use the same animation progression in matched comparisons.
4. Requested and actual render-target resolution, view count, and relevant render
   scale. A full-resolution window with a 1x1 native target is invalid evidence.
5. Presentation policy, observed refresh cap, headless/visible mode, warm-up and
   sampling durations, and the measurement method.

Targets belong to the controlled workload and hardware profile within the row.
Do not compare results across changed profiles without recalibration. The normal
sample route and specialized offscreen probes must have different workload
identities; passing the latter cannot establish performance of the former.

## Measurement Rules

1. Load the fixture and advance the normal render loop until native loading,
   resource uploads and initial viewpoint setup are complete. Where a hidden
   validation route requires explicit render requests, issue them during loading;
   do not wait for GPU readiness while withholding all render-thread work.
2. Verify visible IMM content and the actual attachment dimensions immediately
   before or after sampling. Keep synchronous capture/readback outside the sample.
3. Begin the declared warm-up interval after readiness, then measure a separate
   sustained interval, initially around ten seconds where practical.
4. Count actual rendered workload frames, corroborated by IMM submissions for the
   intended camera/view configuration. Count each workload frame once even when
   it has several cameras, eyes or native submissions. Record engine updates and
   IMM-rendered frames separately so a busy update loop or blank rendering cannot
   produce a passing result.
5. Compute mean FPS as rendered workload frames divided by observed elapsed time.
   Record real frame intervals for mean, median, p95, p99 and maximum frame time.
   Define and record the frame boundary used by each producer. Do not substitute
   fixed delta, synthetic cadence or an instantaneous display counter.
6. Use a monotonic high-resolution clock. Reject inadequate duration/sample count,
   zero-duration intervals, clock anomalies, non-finite values, missing IMM work,
   mismatched configuration and unexpected loss of rendered frames.
7. Measure through the product's normal bounded frame scheduling/backpressure.
   Command-recording throughput with an indefinitely growing GPU queue is not
   rendering FPS. If a special offscreen route lacks normal backpressure, bound
   frames in flight and account for outstanding GPU work at sample boundaries;
   do not add a CPU/GPU wait to every frame just to measure performance.
8. Exclude startup, loading, shader warm-up, capture encoding, artifact writing and
   shutdown. Do not suppress normal product work solely to obtain a passing FPS.
9. Keep visual capture and sampling sequential. Preserve measurement artifacts
   on both collection errors and threshold failures.
10. Apply the same validity rules to report-only and gated measurements. Only
    threshold enforcement differs.

## Presentation Caps and Thresholds

1. Disable v-sync and application FPS caps for desktop headroom measurements where
   supported. Record what actually took effect, not just the requested setting.
2. Preserve mandatory mobile/XR presentation pacing. A capped 60 Hz run can still
   enforce a calibrated loose minimum that detects a fall to 10-12 fps. It cannot
   measure headroom above the cap or support claims about uncapped throughput.
3. Classify capped measurements explicitly and choose thresholds with sufficient
   margin below the observed cap. Do not require impossible uncapping as a
   prerequisite for useful regression detection.
4. Sustained mean FPS is the primary speed gate. A calibrated, generous p95
   frame-time ceiling also catches severe stalls hidden by a mean.
5. Start with loose limits based on several runs of the actual hardware profile.
   Do not derive a target from one unusually fast sample or copy one target across
   unlike rows. Record calibration evidence and date.
6. Default to one authoritative sample. Do not automatically choose the best of
   two runs. If a lane needs one confirmation sample for a threshold miss, retain
   and report both under a declared policy. A pass on confirmation is intermittent
   evidence, not an unconditional clean performance pass; set its failure policy
   during calibration rather than concealing the first failure.
7. Infrastructure retries remain separate from performance confirmation. Preserve
   successful independent lanes and their report inputs until the full validation
   pipeline succeeds. Never retry source failures as measurement noise.

## Evidence Contract

Use a small common producer artifact, `performance-measurements.json`, with a
versioned schema. For example:

```json
{
  "schema": "imm-performance-measurements-v1",
  "matrix": {
    "product": "unity",
    "platform": "windows",
    "mode": "non-vr",
    "renderer": "directx"
  },
  "workload": {
    "id": "sample-normal-urp-v1",
    "fixture": "sample1.imm",
    "fixture_sha256": "<content hash>",
    "camera_pose": "sample-authored-initial-v1",
    "playback_interval_seconds": [3.0, 13.0],
    "graphics_api": "Direct3D12",
    "render_pipeline": "URP",
    "msaa_samples": 8,
    "actual_resolution": { "width": 1280, "height": 720 },
    "view_count": 1
  },
  "runtime": {
    "build_commit": "<source commit>",
    "engine_version": "<version>",
    "gpu": "<actual GPU>",
    "driver": "<driver version or explicit unavailable reason>",
    "device_profile": "<calibrated hardware profile>",
    "visible": true
  },
  "presentation": {
    "vsync_enabled": false,
    "refresh_cap_hz": null,
    "measurement_kind": "uncapped"
  },
  "sampling": {
    "method": "normal-rendered-workload-frames",
    "warmup_seconds": 5.0,
    "requested_seconds": 10.0,
    "observed_seconds": 10.0,
    "engine_frame_count": 600,
    "imm_rendered_frame_count": 600
  },
  "metrics": {
    "mean_fps": 60.0,
    "mean_frame_ms": 16.67,
    "median_frame_ms": 16.6,
    "p95_frame_ms": 18.0,
    "p99_frame_ms": 20.0,
    "maximum_frame_ms": 25.0
  }
}
```

1. Carry the requested configuration alongside actual values where they can differ.
   Additional settings material to a workload are part of its versioned identity.
2. Producers write measurements and validity diagnostics, never their own target
   or pass/fail verdict. Keep frame interval samples in a companion artifact when
   needed to reproduce statistics; avoid logging each frame to the console.
3. A shared verifier reads the declared mode, hardware/workload profile and limits
   from the matrix configuration. It validates identity, real rendering, duration,
   presentation and statistics before applying thresholds.
4. Write a separate `performance-verdict.json` containing the measurement artifact
   hash, selected configuration/target, validity errors, threshold result and any
   confirmation history. Do not mutate either artifact after writing it.
5. Keep performance artifacts separate from visual metrics. Associate them through
   existing manifests and workload identity. Reports distinguish low FPS, invalid
   collection, missing coverage, infrastructure failure and visual failure.
6. Optional diagnostics may include CPU time around IMM preparation/rendering,
   GPU duration where available, and draw/submission counts. Label recording time
   separately from GPU execution time. FPS remains the gate; a universal GPU
   profiler or timestamp-query implementation is not a prerequisite.

## Runtime Integration

1. **Unity:** instrument the normal saved sample in a standalone player first.
   Correlate frame timing with the intended camera's IMM submissions and visible
   content. Editor profiles remain diagnostic. Specialized explicit-render-request
   smoke measurements are supplementary and must not replace normal sample
   acceptance. Write desktop artifacts to the existing lane directory and Android
   artifacts to application storage for Firebase collection.
2. **Standalone:** reuse the per-frame timing source, accumulating a sustained
   sample with pinned content/viewpoint. Add validation configuration for output,
   workload and durations. Share sampling logic across existing desktop/mobile
   renderers where their loops permit it; keep the title counter diagnostic.
3. **Godot:** measure normal real-time rendering without `--fixed-fps`, correlate
   with IMM/compositor activity, and write to the lane output or Android `user://`.
   Include intermediate rendering/composite cost in whole-product frame timing.
4. **Web:** adapt the existing performance test to the common evidence schema.
   Pin viewport, device pixel ratio, browser, playback interval and camera. Record
   the actual graphics backend; classify software rendering as report only.
5. **Mobile/XR:** retain platform pacing, record stable device/runtime identity and
   actual eye/target dimensions, and collect artifacts through existing routes.
   Hardware-dependent XR acceptance remains explicit when automation is absent.
   iOS Simulator results remain report only.
6. Sampling helpers belong with test infrastructure and can configure a temporary
   validation copy. Do not add CI-only scenes or runtime setup requirements to the
   user's sample project. Measure the saved user setup as delivered.

## CI and Report Integration

1. Extend `verify_matrix_status.py` to enforce explicit modes, reasons for
   inapplicability and valid workload/hardware target configuration. A missing
   declaration on a supported row is a schema error, not an opt-out.
2. Add `verify_performance_evidence.py` with shared validity and threshold semantics.
   Run it after collection and before finalizing the lane manifest.
3. Extend existing manifests, `verify_matrix_evidence.py` and
   `write_visual_evidence_report.py`; do not build a parallel reporting framework.
4. Display each row's coverage mode, actual API/pipeline/hardware, workload,
   sustained FPS, p95, cap, target, sample duration/count and verdict. Clearly show
   not implemented and not tested/out of scope as different states.
5. Fail required enabled lanes on missing/invalid evidence. Fail gated lanes on
   threshold misses. Report valid slow report-only measurements without failing.
   Rows outside a partial workflow's requested scope remain not tested.
6. Upload evidence on failure and retain intermediate inputs until all required
   lanes and evidence reports succeed, so isolated retries can reuse them.
7. Representative Windows hardware must exercise the normal Unity D3D12 path for
   a hard gate. If no suitable CI runner is available, report that coverage as
   incomplete and retain the local baseline; do not claim software CI fills it.

## Rollout Phases

### Phase 1: Normal Unity Sample and Matched Local Baseline

1. Implement the minimum common schema, sampler, verifier and report row needed
   for one complete normal Unity Windows D3D12 measurement.
2. Record a real-GPU baseline using the saved URP sample, with actual IMM rendering
   and matching visual evidence outside the timed interval.
3. Run the same Unity workload on Vulkan on the same machine, controlling content,
   animation, camera, resolution, MSAA and presentation. Record artifact/build
   identities. This comparison answers a local backend question; it does not rank
   unrelated matrix rows or establish native-player/Godot overhead.
4. Add native-renderer CPU timing where readily available for attribution.
5. Demonstrate that a controlled slowdown or known slow build fails the check
   while visual validation can still pass. Verify a no-render update loop cannot
   produce passing performance evidence.
6. Collect representative Windows CI samples and enable a loose initial D3D12
   gate when calibrated hardware is available. Keep all other coverage explicitly
   not implemented or report only as appropriate.

Exit condition: a real workload produces an attributable result and actionable
failure, the matched Vulkan baseline is recorded, and Windows hardware CI coverage
is either operational or explicitly identified as an unresolved coverage gap.
Synthetic report tests alone do not complete this phase.

### Phase 2: Existing Desktop and Web Runtime Lanes

1. Reuse the working contract for standalone, Godot, remaining Unity desktop paths
   and the existing web test. Keep product/compositor costs in whole-frame timing.
2. Collect report-only calibration history on each actual hardware profile, then
   enable appropriate gates without waiting for every other platform.
3. Keep hosted software-renderer and simulator measurements report only.

Exit condition: enabled desktop/web lanes publish valid evidence, representative
profiles enforce calibrated limits, and uncovered rows remain visible.

### Phase 3: Mobile and Available XR Hardware

1. Extend sampling and existing Firebase collection to Android runtime lanes,
   including the renderer variants already supported by the matrix.
2. Cover available iOS device execution and keep simulator measurements separate.
3. Calibrate device/thermal variance and capped presentation. Add XR measurements
   on available hardware without changing required runtime pacing.
4. Maintain an explicit outstanding list for hardware-dependent coverage; builds
   and synthetic stereo tests do not count as headset performance acceptance.

Exit condition: all supported renderable rows have valid measurements on their
available execution infrastructure, representative hardware is gated, remaining
hardware gaps are explicit, and build/preflight rows are correctly not applicable.

## Focused Verification

1. Test the verifier's thresholds and boundaries, wrong API/pipeline/workload or
   hardware identity, missing/duplicate evidence, non-finite statistics, inadequate
   samples, absent IMM rendering and wrong target dimensions.
2. Verify all four coverage modes and partial-scope behavior. Report-only ignores
   speed thresholds but still rejects invalid or missing enabled evidence.
3. Verify frame statistics against known intervals, artifact hash association and
   visible reporting of both samples when confirmation is enabled.
4. Extend existing workflow contracts to require collection, verification and
   artifact/report wiring only for enabled runtime lanes, with no early cleanup.
5. Use the phase-1 real slowdown and no-render controls as integration acceptance.
   Avoid building a large synthetic fixture suite before that path works.

## Risks and Completion Criteria

1. Noise, caps and thermal variation require calibrated hardware profiles and loose
   thresholds. Preserve observed results rather than selecting a favorable retry.
2. A small fixture can miss content-dependent regressions. Start with the existing
   sample that exposed this failure; add heavier workloads when evidence warrants
   them rather than delaying the first useful gate.
3. Android Vulkan can expose a 1x1 native display image while the camera surface is
   full resolution. This is an integration portability issue: invalidate that
   workload's measurement instead of treating its apparent FPS as representative.
4. Timing overhead must remain small. Avoid per-frame logging, capture, artificial
   waits and mandatory universal profiling machinery.
5. Completion means every supported row has an explicit classification; enabled
   runtime lanes produce valid evidence; representative hardware has calibrated
   gates; and reports identify missing coverage and low FPS distinctly.
6. At least one real rendering slowdown must be caught while visual checks pass.
   CI protection against the D3D12 failure specifically requires representative
   Windows GPU execution of the normal sample path.
7. Completing this plan does not prove headset pose/per-eye correctness or replace
   manual XR acceptance. It also does not claim all Vulkan hosts have equal cost.
8. Estimate further rollout from the completed phase-1 integration and observed CI
   timings. Do not spend several days finishing schemas/reports before measuring
   the workload that motivated this plan.
