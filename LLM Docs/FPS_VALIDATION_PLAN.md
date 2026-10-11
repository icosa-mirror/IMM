# Validation Matrix FPS Gating Plan

## Objective

Catch gross runtime performance regressions in the existing validation matrix,
including a slowdown such as the Unity Windows D3D12 sample falling to 10-12 fps
while visual correctness tests still pass. Publish attributable sustained frame
measurements and fail representative-hardware lanes when calibrated limits are
missed.

The priority is detecting degenerate cases, not optimizing already acceptable
performance. Use short sustained samples and loose calibrated floors to catch
multi-fold slowdowns, stalls and update loops that render no IMM content. Small
FPS differences do not justify optimization work or block acceptance. Start with
mean FPS and proof of rendering; add a tail-latency gate only when an observed
failure warrants it. Do not make detailed profiling a prerequisite.

The primary coverage axes are platform and host:

1. Platforms: Windows, macOS, iOS and Android.
2. Hosts: Unity, Godot and the native player (`standalone` in matrix identities).

The first milestone is useful end-to-end measurement of the normal supported
runtime paths across these platform/host combinations. Metal on macOS and iOS is
part of that milestone, alongside the normal Windows and Android configurations.
Graphics APIs, engine pipelines and drivers identify the tested configuration and
help diagnose failures; they do not determine which platforms or hosts matter.
A fast native player does not establish that the Unity or Godot integration is fast.

Build only the shared schema, verification and reporting needed to collect and
surface those real measurements. Extend the same implementation to existing web,
XR and additional renderer variants. This plan does not remove platforms, hosts,
rendering backends or correctness gates, and does not require scene consolidation.

Regression gates compare the same platform/host workload and hardware profile over
time. Controlled same-machine API or host comparisons are optional diagnostic
investigations, not rollout prerequisites. Do not rank unlike devices or CI runners.

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
   performance. Each supported platform/host combination needs representative
   hardware coverage before claiming CI protects its runtime performance. This
   includes physical iOS and Android devices and real GPUs on Windows and macOS.
   A local hardware baseline is useful but does not fill an absent CI hardware gate.

### Current task boundary

The immediate task is to check the existing CI targets on the four major
platforms for degenerate performance, across the three hosts, and then pause.
Use the available CI runners and device lanes. Identify simulator and software
renderer results accurately; obtaining additional physical devices is follow-up
coverage, not a prerequisite for this task's stopping point. This boundary does
not claim that simulator results establish physical-device performance.

### Implementation checkpoint (2026-10-11)

1. Matrix rows now declare performance coverage explicitly, and audit reports show
   the platform/host grid. The twelve primary platform/host runtime rows have report-only
   collection wired; complete CI measurements are pending. Additional runtime
   variants remain `not_implemented` until wired.
2. The native viewer has an opt-in raw frame collector enabled by
   `IMM_PERFORMANCE_OUTPUT`. It samples the normal mono render loop after two
   seconds of warm-up for five seconds, counting frames with IMM draw work.
   `tests/tools/verify_frame_sample.py` checks validity and an optional local FPS
   floor. This raw artifact does not yet provide the full evidence contract below.
3. The Windows native build and focused verifier tests passed. A local normal
   Vulkan sample measured approximately 357 FPS at 1280x720 on an RTX 4090.
   This is an initial measurement, not a calibrated CI target.
4. Unity and Godot probes live under `tests`, outside the user projects. Unity's
   hidden CI mode issues one full-size render request per engine tick and labels
   this separately from normal window FPS. Collection does not perform per-frame
   readback or add GPU waits. The runner retains raw samples, logs, input hashes
   and a separate verdict; missing or invalid enabled evidence fails the lane.
5. Exercising the delivered Unity scene exposed its missing saved `ImmCamera`
   component. The saved main and VR cameras now opt into URP, and the defaults
   command handles a fresh unnamed Editor scene. The corrected normal Windows
   sample produced valid approximately 60 FPS evidence with v-sync enabled.
6. iOS/Android collection and speed calibration remain unfinished. No completed
   cross-platform performance protection is claimed by this checkpoint.

### Android Godot investigation (2026-10-11)

Local physical-device follow-up on A059 (Android 16, Adreno 810): the same Godot
benchmark APK from run `38150209754` produced 58.49, 26.75 and 24.18 FPS in three
five-second samples at 2392x1080. All samples passed rendered-frame validity;
the variation is unresolved and the first result must not stand alone.

The native Vulkan APK from green run `38167228187` produced **10.15 FPS** at
2266x1080 with 8x MSAA. Its saved log records about 0.5 ms of scene command
recording but 20–57 ms waiting for submitted GPU work. This is an unresolved
physical-device performance problem, despite full CI validation passing.

The first candidate reuses the full-resolution MSAA presentation resolve image
instead of allocating/freeing it every frame. It retains the existing resolution,
MSAA, presentation waits and visual behavior. A single before/after Windows
Vulkan comparison on an RTX 4090 at 1280x720 improved from 383.10 to 613.05 FPS
(2.61 to 1.63 ms/frame). This is desktop evidence, not an Android improvement claim.
The Android APK builds; seven existing Vulkan tests and the native sample
readback/live-presentation smoke pass. A059 retesting is pending: the user has
reclaimed the phone, and no further device interaction is authorized until it is
made available again. Raw samples, logs, verdicts and the candidate APK are kept
locally under ignored `artifacts/a059-phone/`.

The corrected same-run binary validation in run `38150209754` measured
**51.44 FPS** on the physical Mali-G715 Android device at 2400x1080, compared
with 10.3 FPS before persistent render-pass reuse. This resolves the observed
degenerate Android Godot case for that workload; it is a short CI sample, not a
claim about sustained thermal performance.

CI startup reliability was separately traced to cached `Library/burst.pid`
records: Unity can kill a reused PID on a new runner while cleaning up an old
Burst compiler. Four restored-cache startup trials passed with PID cleanup in
run `38163769973`. Production saves and restores now allowlist only package,
imported-asset and shader-cache data, using new keys that exclude old whole-Library
archives. Compiler PIDs and other unlisted session/build state are regenerated.
Required gates remain unchanged. See
[the investigation and validation record](../docs/ci-unity-startup-kills.md).
Full validation run [38167228187](https://github.com/icosa-mirror/IMM/actions/runs/38167228187)
passed all 45 jobs at source `f4229d82` with the allowlist policy. This is a
successful full run, not a measured failure rate. Warm-cache build times must
be measured before expanding the allowlist.

1. Run `38135593727`, source `b2c29023`, measured 10.3 FPS for the delivered
   sample on Firebase's akita Android 34 / Mali-G715 at 2400x1080. Measurement
   validity passed; acceptable performance has not. No FPS floor is enforced yet.
2. Existing renderer timings show 62-75 ms of CPU recording, almost all inside
   draw API calls, and approximately 15 ms of submit/completion time for 38 draws.
   Captured handles show stable color/depth images but a recreated render pass
   each frame. The pipeline cache requires matching render-pass handles.
3. Candidate: use the existing persistent external-image wrappers for Godot's
   intermediate targets. Keep the direct host-color/depth path unchanged. Preserve
   normal-Z depth clearing on cache hits. At most this can recover the measured
   62-75 ms CPU cost; it cannot eliminate the GPU/composition cost.
4. Budget: one focused local Vulkan check and one matched Firebase comparison.
   Accept only if the same fixture, resolution, device profile and source-built
   binaries improve sustained FPS materially and existing composition checks pass.
   Stop this candidate if recording time remains high or correctness regresses;
   do not hide the slowdown by lowering resolution or relaxing correctness gates.
5. Run `38144175925` still measured 10.36 FPS on Android, but its uncached-path
   logs exposed stale inputs: the device job exported checked-in native libraries
   rather than the same-run build artifact. This is not a valid comparison of the
   candidate. The job now downloads the same-commit Android native artifact and
   checks both exported APKs against its exact library bytes, recording hashes.
   The valid source-built Windows llvmpipe measurement rose from 3.16 to 8.49 FPS;
   the corrected Android comparison is recorded above. Simulator and software-renderer
   results do not establish hardware performance on those platforms.

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
7. Inventory all twelve major platform/host combinations against the supported
   matrix. Each supported runtime combination needs its own performance evidence
   and target; explicitly identify unsupported, deferred or build-only cells.
   Preserve existing correctness gates and renderer variants. Web and XR coverage
   remain in scope in addition to the major platform/host grid.
8. Use existing runtime jobs and artifact collection. Do not create a second
   complete matrix or duplicate long builds merely for FPS collection.

## Workload Identity

A matrix row identifies a platform/host/mode family. Keep the native-player host
named `standalone` in existing machine-readable identities. Each measurement must
also identify its exact workload and rendering path:

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
    "platform": "macos",
    "mode": "non-vr",
    "renderer": "metal"
  },
  "workload": {
    "id": "sample-normal-urp-v1",
    "fixture": "sample1.imm",
    "fixture_sha256": "<content hash>",
    "camera_pose": "sample-authored-initial-v1",
    "playback_interval_seconds": [3.0, 13.0],
    "graphics_api": "Metal",
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

1. **Unity:** instrument the normal saved sample in a built Unity player.
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
   Native-player measurements cannot stand in for these Godot integration costs.
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
4. Lead the report with a Windows/macOS/iOS/Android by Unity/Godot/native coverage
   grid, showing each cell's evidence and verdict or its explicit coverage gap.
   Show supported API/mode variants under the relevant cell rather than collapsing
   them into one passing result. Detailed rows record actual API/pipeline/hardware,
   workload, FPS, p95, cap, target and sample duration/count. Keep not implemented,
   unsupported, build-only and not tested/out of scope visibly distinct.
5. Fail required enabled lanes on missing/invalid evidence. Fail gated lanes on
   threshold misses. Report valid slow report-only measurements without failing.
   Rows outside a partial workflow's requested scope remain not tested.
6. Upload evidence on failure and retain intermediate inputs until all required
   lanes and evidence reports succeed, so isolated retries can reuse them.
7. Gate each supported platform/host cell on its representative normal runtime
   path: real GPUs for Windows/macOS and physical devices for iOS/Android. macOS
   and iOS Metal are first-class requirements. If hardware is unavailable, expose
   the specific cell as a coverage gap and retain any local baseline. Software or
   simulator results, builds and another host's success do not fill that gap.

## Rollout Phases

Phases are incremental and may overlap: enable a useful calibrated gate as soon as
its cell is ready. Do not wait for every platform to finish before protecting one,
and do not complete a backend-comparison project before collecting platform data.

### Phase 1: Real Baselines Across Major Platforms and Hosts

1. Inventory Windows, macOS, iOS and Android crossed with Unity, Godot and native.
   Map each supported runtime cell to its normal configuration, available hardware
   and existing validation lane. Record unsupported/build-only cases and concrete
   hardware gaps without silently excluding them.
2. Implement minimal reusable sampling, verification and reporting for the three
   hosts, then collect actual rendered-frame measurements across the four major
   platforms. Reuse each host's implementation across platforms where possible.
3. Include macOS/iOS Metal and the normal supported Windows/Android paths. Pin
   workload and settings per cell; do not require the same numerical FPS target
   across different devices or host integrations.
4. Measure normal delivered samples end to end, including Unity renderer-feature
   and Godot compositor costs. Record visible correctness outside the sample.
5. Add native-renderer CPU timing where readily available for attribution, without
   making universal GPU profiling a prerequisite for collection.
6. Demonstrate on representative hardware that a controlled slowdown or known
   slow build fails while visual checks can still pass, and that a no-render
   update loop cannot produce passing performance evidence.

Exit condition: the major platform/host coverage grid shows real baseline evidence
for supported runtime cells and identifies every remaining hardware or collection
blocker. Missing physical-device evidence remains incomplete; simulator results do
not complete iOS coverage. A single Windows or native-player result does not
complete this milestone. Synthetic report tests alone are insufficient.

### Phase 2: CI Collection, Calibration and Enforcement

1. Wire these measurements into existing runtime jobs and artifact collection,
   including Firebase and available iOS device execution.
2. Collect history on actual hardware profiles, then enable loose per-cell gates
   incrementally. Calibrate thermal variance and presentation caps on mobile.
3. Keep simulator/software results report only and expose hardware gaps in the
   platform/host grid until representative CI execution is available.
4. Verify collection, failure classification, retained retry evidence and final
   reporting on the existing pipeline. Preserve normal runtime pacing.

Exit condition: supported major platform/host cells have valid representative CI
measurements and calibrated gates, or a clearly identified outstanding hardware
blocker. Do not describe blocked cells as protected by CI.

### Phase 3: Remaining Supported Configurations, Web and XR

1. Extend the same mechanism to additional graphics API/configuration variants
   already supported by the matrix. They remain separate attributable workloads
   under their platform/host cell; do not drop their correctness requirements.
2. Adapt the existing web performance test to the common artifact/report contract.
3. Add XR measurements on available hardware, recording eye/target dimensions and
   mandatory presentation pacing. Track hardware-dependent gaps explicitly.
4. Run matched API or host comparisons only when useful to diagnose measured
   results. Such investigations are separate from platform/host coverage acceptance.

Exit condition: all supported renderable matrix rows have valid measurements on
available execution infrastructure, representative hardware is gated, remaining
hardware gaps are explicit, and build/preflight rows are not applicable. Builds
and synthetic stereo tests do not count as headset performance acceptance.

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
   Verify the coverage report cannot substitute a passing host, backend or simulator
   result for a missing platform/host runtime result. Avoid a large synthetic
   fixture suite before real sampling works.

## Risks and Completion Criteria

1. Noise, caps and thermal variation require calibrated hardware profiles and loose
   thresholds. Preserve observed results rather than selecting a favorable retry.
2. A small fixture can miss content-dependent regressions. Start with normal
   existing samples for each host; add heavier workloads when evidence warrants
   them rather than delaying useful measurements across the major platforms.
3. Android Vulkan can expose a 1x1 native display image while the camera surface is
   full resolution. This is an integration portability issue: invalidate that
   workload's measurement instead of treating its apparent FPS as representative.
4. Timing overhead must remain small. Avoid per-frame logging, capture, artificial
   waits and mandatory universal profiling machinery.
5. Completion means every supported platform/host cell and required runtime
   variant has an explicit classification and valid performance evidence;
   representative hardware has calibrated gates; and reports identify missing
   coverage and low FPS distinctly. Unresolved hardware gaps remain outstanding
   work rather than evidence of completion.
6. At least one real rendering slowdown must be caught while visual checks pass.
   CI protection is claimed per measured platform/host/configuration, including
   macOS/iOS Metal; a good backend microbenchmark or another host's success is
   insufficient.
7. Completing this plan does not prove headset pose/per-eye correctness or replace
   manual XR acceptance. Shared rendering code does not establish equal host costs.
8. Estimate further rollout from real collection across the major platform/host
   grid and observed CI timings. Do not spend several days finishing schemas or
   backend comparisons before measuring the supported user experiences.
