# Validation Matrix FPS Gating Plan

## Objective

Add sustained frame-rate evidence to every renderable supported cell in the main
validation matrix. Each applicable cell will be classified as either `gated` or
`report_only`, publish machine-readable measurements, display those measurements in
the validation report, and, for `gated` cells, fail validation when the target is
missed or required performance evidence is absent.

The initial purpose is to catch gross performance regressions. This is not
intended to establish a precise cross-platform benchmark or compare unlike
renderers, devices, and runners.

## Current State

1. `tests/matrix_status.json` contains 21 supported rows. Seventeen are renderable.
   Two are preflight rows (`unity` / `all` / `preflight` and `godot` / `windows` /
   `preflight`) and two are application-build-only rows (`unity` / `ios` /
   `application-build` and `godot` / `ios` / `application-build`). None of those
   four produce rendered frames and none have meaningful FPS measurements.
2. `tests/tools/write_visual_evidence_report.py` already maps collected artifacts
   to matrix rows, reports evidence coverage, and can fail when evidence required
   by the selected validation scope is missing.
3. `tests/tools/verify_matrix_evidence.py` already enforces visual, preflight, and
   VR evidence requirements for applicable matrix rows.
4. The standalone viewer already calculates an FPS value for its window title,
   but does not publish a stable machine-readable performance sample.
5. The web performance test already records a ten-second sample containing mean,
   median, p95, p99, maximum frame time, slow-frame counts, and derived FPS.
6. Unity and Godot validation lanes do not currently emit a common sustained-FPS
   artifact.
7. Some Godot smoke commands use `--fixed-fps 30`. Those runs use a synthetic
   timing mode and cannot serve as real performance measurements.

## Policy

1. Performance targets are declared per matrix row rather than as one global FPS
   value. Rows differ in runner hardware, device model, resolution, presentation
   caps, engine overhead, and renderer behavior.
2. Applicability is explicit. Every row either carries a `performance` object or is
   unambiguously not applicable. Supported renderable rows that execute on
   representative runtime hardware must declare a hard target. Preflight rows,
   application-build-only rows, deferred rows, and unsupported rows are marked not
   applicable and must never be counted as missing FPS evidence.
3. Rows whose lanes run on non-representative infrastructure publish report-only
   measurements rather than gated ones. This includes the iOS Simulator rows, any
   lane backed by a software or Lavapipe Vulkan device, and hosted runners without a
   real GPU. Their artifacts are collected and displayed but never hard-fail. They
   are promoted to hard gating only when the lane runs on representative hardware.
4. Every targeted row uses fixed content, resolution, camera state, rendering
   configuration, warm-up duration, and sampling duration. The pinned camera pose
   is recorded in the matrix row and carried into the evidence so a measurement is
   reproducible.
5. The presentation cap (v-sync or refresh-rate lock) is disabled for the measured
   window. Where a platform cannot disable it, the active cap is recorded in the
   evidence and the recorded mean FPS is treated as a lower bound rather than a
   benchmark. A cap that masks regressions is not acceptable as a hard gate.
6. The primary gate is sustained mean FPS. A deliberately generous p95 frame-time
   ceiling catches severe stalls that a mean alone could hide.
7. Missing, malformed, mismatched, or failed performance evidence is a validation
   failure for a row that declares a hard target.
8. Initial thresholds are deliberately loose and intended only to detect gross
   regressions. Tightening them requires measurement history from the actual CI
   runners or devices.
9. FPS results are comparable only with previous results from the same matrix row
   and controlled configuration. The report must not rank unlike cells.

## Matrix Schema

Add an explicit `performance` object to each row in `tests/matrix_status.json`:

```json
"performance": {
  "applicable": true,
  "mode": "gated",
  "minimum_mean_fps": 25.0,
  "maximum_p95_frame_ms": 50.0,
  "warmup_seconds": 5.0,
  "sample_seconds": 10.0,
  "resolution": { "width": 1280, "height": 720 },
  "camera_pose": "validation-default"
}
```

`mode` is either `"gated"` (threshold misses fail the row) or `"report_only"`
(measurements are recorded and displayed but never fail the row). Rows that do not
declare a `performance` object are not applicable, which covers preflight,
application-build-only, deferred, and unsupported rows. The exact initial target is
selected independently for each gated row after collecting representative samples.

Schema validation lives in `tests/tools/verify_matrix_status.py` (with its existing
`tests/tools/test_verify_matrix_status.py` suite) and must enforce the following
rules:

1. A renderable row expected to gate sets `applicable: true`, `mode: "gated"`,
   positive `minimum_mean_fps`, `warmup_seconds`, and `sample_seconds`; when present,
   `maximum_p95_frame_ms` must be positive.
2. A renderable row on non-representative infrastructure sets `applicable: true`,
   `mode: "report_only"`. Its thresholds are optional and never fail the row.
3. Preflight, application-build-only, deferred, and unsupported rows must not set
   `applicable: true`, and must not be treated as missing FPS evidence.
4. `resolution` is an object with positive integer `width` and `height`.
5. A row's performance result must identify the same product, platform, mode, and
   renderer as the matrix row.

## Performance Evidence Contract

Each applicable lane writes a producer-owned `performance-measurements.json` file
containing only raw measurements:

```json
{
  "schema": "imm-performance-measurements-v1",
  "matrix": {
    "product": "standalone",
    "platform": "windows",
    "mode": "non-vr",
    "renderer": "directx"
  },
  "fixture": "sample1.imm",
  "camera_pose": "validation-default",
  "resolution": {
    "width": 1280,
    "height": 720
  },
  "presentation": {
    "vsync_enabled": false,
    "refresh_cap_hz": null
  },
  "sampling": {
    "warmup_seconds": 5.0,
    "requested_seconds": 10.0,
    "observed_seconds": 10.04,
    "frame_count": 602
  },
  "metrics": {
    "mean_fps": 59.96,
    "mean_frame_ms": 16.68,
    "median_frame_ms": 16.67,
    "p95_frame_ms": 17.21,
    "p99_frame_ms": 19.84,
    "maximum_frame_ms": 27.42
  }
}
```

The producer records raw measurements only. It must never write `target`, `passed`,
or `errors` into its own artifact.

A shared verifier reads the target from `tests/matrix_status.json`, verifies the
evidence identity, camera pose, presentation state, and sampling contract, then
writes a separate verifier-owned `performance-verdict.json`:

```json
{
  "schema": "imm-performance-verdict-v1",
  "matrix": {
    "product": "standalone",
    "platform": "windows",
    "mode": "non-vr",
    "renderer": "directx"
  },
  "evidence": "performance-measurements.json",
  "evidence_sha256": "3f2c...",
  "target": {
    "mode": "gated",
    "minimum_mean_fps": 25.0,
    "maximum_p95_frame_ms": 50.0
  },
  "passed": true,
  "errors": []
}
```

Neither artifact is mutated after being written. Because the verdict records the
evidence hash, a row's pass/fail decision is reproducible and can be re-checked
without re-running the lane, and individual runtime implementations cannot apply
different threshold semantics.

## Measurement Rules

1. Load the existing validation fixture, apply the row's pinned camera pose, and
   wait for document loading, resource upload, and any shader warm-up required by
   the lane.
2. Disable v-sync and any refresh-rate lock for the measured window. If the platform
   cannot disable it, record the active cap in the `presentation` block and treat
   the mean FPS as a lower bound.
3. Begin a separate warm-up interval only after the lane has reached the same ready
   state used for visual capture.
4. Sample actual presented or rendered frame intervals for at least the duration
   declared by the row.
5. Use a monotonic high-resolution clock.
6. Do not include startup, fixture loading, capture encoding, file writing, or
   shutdown time in the FPS sample.
7. Do not run capture readback during the measured interval unless readback is an
   intentional part of the product's normal frame path.
8. Do not use fixed-delta or synthetic-frame timing for performance evidence.
9. Record individual frame intervals internally so percentiles are calculated from
   the sample rather than inferred from aggregate FPS.
10. Detect insufficient samples, clock anomalies, zero-duration samples, NaN or
    infinite values, presentation-state mismatches, camera-pose mismatches, and
    renderer/device mismatches as evidence errors.
11. Preserve the measurement artifact even when the threshold fails so the report
    explains the failure.
12. Apply identical measurement rules to `report_only` and `gated` rows; only the
    verdict weight differs.

## Runtime Instrumentation

### Standalone Viewer

1. Reuse the viewer's existing per-frame timing source, but accumulate frame
   intervals over a controlled warm-up and measurement window.
2. Add validation-only command-line or settings fields for the measurements output
   path, warm-up duration, sample duration, pinned camera pose, and expected matrix
   identity.
3. Force the pinned camera pose and disable v-sync for the measured window, then
   write the `performance-measurements.json` artifact before normal validation
   shutdown.
4. Ensure DirectX, Vulkan, OpenGL, Android GLES, Android Vulkan, and macOS Metal
   use the same measurement implementation where their application loops allow it.
5. Keep the window-title FPS counter diagnostic-only; it is an instantaneous
   display value and is not the validation result.

### Web Player

1. Adapt `code/projects/web/app/tests/web-player-performance.mjs` to emit
   `imm-performance-measurements-v1` in addition to its detailed diagnostic report.
2. Keep the existing ten-second `requestAnimationFrame` sampling and percentile
   collection.
3. Fix the viewport, device pixel ratio, browser version, fixture, selected heavy
   playback point, canvas size, and pinned camera pose used by CI.
4. Make the FPS gate part of the main WebGL validation evidence rather than an
   unrelated optional benchmark.
5. Record whether the run was headless and the browser/GPU renderer description for
   diagnosis, but do not change the matrix identity based on those fields. When the
   hosted browser exposes only a software GL backend, mark the row `report_only`
   rather than gating it.

### Unity

1. Add a validation smoke component that pins the camera pose, disables v-sync, and
   samples unscaled frame intervals after the existing smoke fixture reports ready.
2. Use the standalone player for authoritative runtime measurements. Editor Play
   Mode measurements may remain diagnostic because Editor overhead is not stable
   enough for the gate. The Unity iOS Metal Simulator row is `report_only`.
3. Emit `performance-measurements.json` to the lane's existing artifact directory or
   an Android-accessible application data path.
4. Add the measurements output path, warm-up, sample duration, camera pose, and
   matrix identity to the existing smoke-player command-line configuration.
5. Pull the artifact from Firebase Test Lab for Unity Android Vulkan alongside the
   current captures and status evidence.
6. Ensure visual capture and performance sampling are sequential so synchronous
   screenshot work does not contaminate the measured interval.

### Godot

1. Add performance sampling to the validation scene or a dedicated validation
   helper that pins the camera pose, disables v-sync, and records real frame
   intervals after the IMM fixture reports ready. The Godot iOS Metal Simulator row
   is `report_only`.
2. Write `performance-measurements.json` to an explicit CI output path on desktop
   and to `user://` on Android for Firebase collection.
3. Run the measured portion without `--fixed-fps`; retain fixed FPS only for
   deterministic non-performance checks that require it.
4. Keep visual capture and performance sampling sequential.
5. Pull the Android Godot Vulkan artifact from Firebase Test Lab alongside the
   existing render and composition captures.

## Shared Verification and Report Integration

1. Add `tests/tools/verify_performance_evidence.py` to:
   1. Load the selected matrix rows and their `performance` objects.
   2. Discover `performance-measurements.json` artifacts by manifest identity.
   3. Validate the measurements schema, matrix identity, camera pose, presentation
      state, and evidence hash, then emit `performance-verdict.json`.
   4. Verify the configured warm-up, sampling duration, resolution, and fixture.
   5. Apply the row's mean-FPS and p95 frame-time targets for `gated` rows, and
      record a non-failing verdict for `report_only` rows.
   6. Fail on missing or duplicate authoritative evidence in the required scope.
   7. Produce a machine-readable summary for the final report.
2. Extend CI manifests and artifact summaries to classify
   `performance-measurements.json` and `performance-verdict.json` as performance
   evidence without confusing them with visual render metrics.
3. Do not add FPS fields to the current visual metrics schema. Visual correctness
   and runtime performance have different completeness and failure contracts.
4. Extend `tests/tools/verify_matrix_evidence.py` so a targeted renderable `gated`
   row is incomplete unless it has a passing verdict. Rows marked `report_only` or
   not applicable must not create false missing-evidence failures.
5. Extend `tests/tools/write_visual_evidence_report.py` with a Performance Matrix
   table containing:
   1. Matrix cell.
   2. Mean FPS.
   3. p95 frame time.
   4. Configured target and mode (`gated` or `report_only`).
   5. Sample duration and frame count.
   6. Pass, fail, report-only, not-applicable, or not-tested status.
6. Include performance failures in each lane's detailed report section and in the
   top-level failure summary.
7. Make the final report command return nonzero when performance evidence required
   by `--required-evidence-scope` is missing or failing.
8. Preserve the current behavior in quick or partial workflows: rows outside the
   requested evidence scope appear as not tested rather than failed.

## CI Wiring

1. Add performance collection to the existing runtime job for each applicable row;
   do not create a second complete matrix of duplicate application runs.
2. For `gated` rows, allow a bounded retry on a threshold miss (for example, take
   the best of two attempts) before failing, to absorb ordinary runner noise. Record
   the attempt count and the chosen sample in the evidence metadata.
3. Run the performance verifier after the runtime artifact has been collected and
   before the lane manifest is finalized.
4. Use a `performance` failure class, or another explicitly named class, so a low
   FPS failure is not reported as visual corruption or infrastructure failure.
5. Upload the evidence even when verification fails by using the same always-run
   manifest and artifact pattern as the existing visual lanes.
6. Download performance evidence into the existing Validation Evidence job and
   include it in `CIValidationEvidence`.
7. `report_only` rows collect and display evidence but never fail the lane or the
   report command.
8. Update workflow contract tests so every applicable matrix row is statically
   verified to produce and check performance evidence, and so build-only and
   preflight rows are statically verified not to require it.

## Target Calibration

1. Initially run measurement in `report_only` mode for several scheduled or full
   validation runs.
2. Record results separately for every matrix row and actual runner/device model.
3. Inspect the median, normal lower bound, p95 frame-time distribution, thermal
   behavior on Android, and any presentation cap.
4. Select a minimum mean FPS sufficiently below the normal lower bound that routine
   variance passes while a substantial regression fails.
5. Select a p95 frame-time ceiling that permits ordinary scheduling noise but
   rejects sustained severe stalls.
6. Do not derive targets from one unusually fast run.
7. Document the calibration evidence and date in the matrix row or an adjacent
   checked-in calibration file.
8. Only promote a row to `gated` when its lane runs on representative hardware.
   Rows backed by the iOS Simulator, a software or Lavapipe Vulkan device, or a
   hosted GPU-less runner stay `report_only`.
9. Enable hard failure one representative row group at a time: desktop standalone,
   engine desktop, Android devices, then web.

## Tests

1. Unit-test the performance evidence verifier for passing values, low mean FPS,
   excessive p95 time, missing evidence, malformed JSON, wrong matrix identity,
   wrong resolution, wrong camera pose, wrong presentation state, inadequate
   duration, duplicate evidence, and non-finite values.
2. Test boundary behavior at exactly the configured minimum and maximum.
3. Test that preflight, application-build-only, deferred, unsupported, and
   out-of-scope rows do not create false missing-evidence failures.
4. Test that `report_only` rows never fail regardless of measured value, while
   still producing a verdict and report row.
5. Test that the producer artifact is never mutated and that the verdict records a
   matching evidence hash.
6. Test report rendering for passed, failed, report-only, malformed, not-applicable,
   and not-tested cells.
7. Test that a performance failure remains visible when visual evidence passes.
8. Test that a visual failure remains correctly classified when performance also
   fails.
9. Unit-test `verify_matrix_status.py` for the new `performance` schema rules,
   including invalid `mode`, missing required thresholds, and non-object resolution.
10. Extend workflow verification to require the expected performance producer,
    verifier, artifact include, and report input for each applicable lane, and to
    reject performance requirements on preflight or application-build-only rows.
11. Run the existing CI tool self-tests to ensure visual evidence behavior is
    unchanged.

## Rollout Phases

### Phase 1: Contract and Report

1. Add the matrix `performance` schema to `verify_matrix_status.py` and the shared
   measurements and verdict schemas.
2. Implement the verifier and its unit tests.
3. Add the Performance Matrix and detailed sections to the final report.
4. Default every row to no `performance` object (not applicable) so existing lanes
   remain unaffected during development.

Exit condition: synthetic artifacts can exercise every performance report and
failure state without running an engine or viewer.

### Phase 2: Standalone and Web

1. Add standalone machine-readable sampling on desktop renderers.
2. Adapt the existing web performance test to the common schema.
3. Wire the artifacts through manifests and the final report.
4. Collect report-only calibration results.

Exit condition: all representative desktop standalone and WebGL rows publish valid,
visible performance evidence, with non-representative rows correctly marked
`report_only`.

### Phase 3: Unity and Godot Desktop

1. Add Unity player sampling and artifact output.
2. Add Godot real-time sampling without fixed FPS during measurement.
3. Wire Windows and macOS engine lanes into the common verifier.
4. Collect report-only calibration results.

Exit condition: all representative desktop Unity and Godot runtime rows publish
valid, visible performance evidence; iOS Simulator rows remain `report_only`.

### Phase 4: Android Device Lanes

1. Add standalone Android artifact output and Firebase collection.
2. Add Unity Android Vulkan artifact output and Firebase collection.
3. Add Godot Android Vulkan artifact output and Firebase collection.
4. Confirm the configured Firebase device model is recorded and stable.
5. Collect enough runs to account for thermal and service variance.

Exit condition: every supported Android runtime row publishes valid, attributable
performance evidence.

### Phase 5: Enforcement

1. Set calibrated per-row targets.
2. Enable hard failure for missing or failing evidence in the required validation
   scope.
3. Confirm intentional low-FPS fixtures fail while unmodified validation runs
   remain stable.
4. Document how to update a target when runner hardware, device model, resolution,
   fixture, or rendering configuration changes.

Exit condition: all 17 renderable supported rows publish valid, visible
performance evidence; every representative-hardware row is `gated`, every
non-representative row is `report_only`, the two preflight rows and two
application-build-only rows are explicitly not applicable, and the final validation
report identifies low FPS as a distinct actionable failure.

## Estimated Effort

1. Shared matrix schema, verifier, report integration, and tests: approximately one
   to two working days.
2. Standalone and web integration (17 rows span six standalone renderers plus
   WebGL, each needing camera pinning, v-sync control, and artifact output):
   approximately two working days.
3. Unity, Godot, and Firebase Android integration and stabilization: approximately
   three to five working days.
4. Total implementation estimate: approximately six to nine working days, excluding
   the elapsed CI time needed to collect enough calibration runs and to stabilize
   the retry policy on noisy lanes.

## Risks and Mitigations

1. Runner variance can produce false failures. Use controlled settings, warm-up,
   sustained sampling, loose initial thresholds, per-row targets, and a bounded
   best-of-N retry before failing.
2. V-sync or display caps can hide improvements but still expose severe
   regressions. Disable the cap for the measured window; where that is impossible,
   record the cap and treat the mean as a lower bound rather than a comparative
   benchmark.
3. Screenshot capture can distort timing. Measure before capture or after capture,
   never during synchronous readback and encoding.
4. Fixed-step modes can report artificial cadence. Reject synthetic timing modes
   as authoritative performance evidence.
5. Android thermal throttling can introduce noise. Keep samples short but
   sustained, warm up consistently, use stable Firebase models, and calibrate from
   multiple runs.
6. Startup or shader compilation can dominate the result. Gate steady-state FPS
   separately and retain startup timing as optional diagnostic evidence.
7. A low-complexity fixture may miss content-dependent regressions. Begin with the
   existing validation fixture for broad coverage, then add separate heavier
   performance fixtures only when a real regression class requires them.
8. Simulators, software or Lavapipe Vulkan, and GPU-less hosted runners do not
   measure representative rendering performance. Keep those rows `report_only` and
   never gate or rank them.
9. Some Android Vulkan devices expose Unity's display render buffer as a 1x1 image
   through `IUnityGraphicsVulkan::AccessRenderBufferTexture` even when the camera
   surface is full resolution. Treat standalone Android Vulkan FPS on such devices
   as potentially misleading and confirm the render-buffer path before gating that
   row.

## Completion Criteria

1. Every supported renderable row is explicitly classified `gated` (representative
   hardware) or `report_only` (non-representative infrastructure), and every
   `gated` row declares an approved performance target.
2. Every applicable lane emits valid `imm-performance-measurements-v1` evidence
   from a real-time sustained sample, and the verifier emits a matching
   `imm-performance-verdict-v1` with the evidence hash.
3. The final validation report shows FPS, p95 frame time, target, mode, sample size,
   and verdict for every applicable matrix cell.
4. Missing evidence and threshold misses fail required validation scopes for
   `gated` rows; `report_only` rows never fail.
5. Preflight, application-build-only, deferred, unsupported, and unrequested cells
   remain correctly classified without false failures.
6. Performance failures are distinct from visual, compositing, runtime, and
   infrastructure failures.
7. Automated tests cover schema validation, threshold boundaries, camera and
   presentation mismatches, evidence discovery, report output, and workflow wiring.
8. At least one controlled regression test demonstrates that a gross frame-rate
   regression is caught on a `gated` row while the produced visual evidence can
   still pass.
