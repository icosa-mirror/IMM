# Validation Matrix FPS Gating Plan

## Objective

Add sustained frame-rate evidence to every renderable supported cell in the main
validation matrix. Each applicable cell will declare an explicit performance
target, publish machine-readable measurements, display those measurements in the
validation report, and fail validation when the target is missed or required
performance evidence is absent.

The initial purpose is to catch gross performance regressions. This is not
intended to establish a precise cross-platform benchmark or compare unlike
renderers, devices, and runners.

## Current State

1. `tests/matrix_status.json` contains 16 supported rows. Fourteen are renderable;
   the Unity and Godot preflight rows do not have meaningful FPS measurements.
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
2. Only supported, renderable rows declare performance targets. Preflight,
   deferred, and unsupported rows do not require FPS evidence.
3. Every targeted row uses fixed content, resolution, camera state, rendering
   configuration, warm-up duration, and sampling duration.
4. The primary gate is sustained mean FPS. A deliberately generous p95 frame-time
   ceiling catches severe stalls that a mean alone could hide.
5. Missing, malformed, mismatched, or failed performance evidence is a validation
   failure for a row that declares a performance target.
6. Initial thresholds are deliberately loose and intended only to detect gross
   regressions. Tightening them requires measurement history from the actual CI
   runners or devices.
7. FPS results are comparable only with previous results from the same matrix row
   and controlled configuration. The report must not rank unlike cells.

## Matrix Schema

Add an optional `performance` object to each applicable row in
`tests/matrix_status.json`:

```json
"performance": {
  "minimum_mean_fps": 25.0,
  "maximum_p95_frame_ms": 50.0,
  "warmup_seconds": 5.0,
  "sample_seconds": 10.0,
  "resolution": "1280x720"
}
```

The exact initial target is selected independently for each row after collecting
representative samples. Schema validation must enforce the following rules:

1. Supported renderable rows must have a `performance` object once FPS gating is
   enabled for that row.
2. `minimum_mean_fps`, `warmup_seconds`, and `sample_seconds` must be positive.
3. `maximum_p95_frame_ms`, when present, must be positive.
4. Preflight, deferred, and unsupported rows must not be treated as missing FPS
   evidence.
5. A row's performance result must identify the same product, platform, mode, and
   renderer as the matrix row.

## Performance Evidence Contract

Each applicable lane writes a `performance.json` file with a common schema:

```json
{
  "schema": "imm-performance-evidence-v1",
  "matrix": {
    "product": "standalone",
    "platform": "windows",
    "mode": "non-vr",
    "renderer": "directx"
  },
  "fixture": "sample1.imm",
  "resolution": {
    "width": 1280,
    "height": 720
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
  },
  "target": {
    "minimum_mean_fps": 25.0,
    "maximum_p95_frame_ms": 50.0
  },
  "passed": true,
  "errors": []
}
```

The producer records raw measurements. A shared verifier reads the target from
`tests/matrix_status.json`, verifies the evidence identity and sampling contract,
calculates the verdict, and writes the authoritative `target`, `passed`, and
`errors` fields. This prevents individual runtime implementations from applying
different threshold semantics.

## Measurement Rules

1. Load the existing validation fixture and wait for document loading, resource
   upload, and any shader warm-up required by the lane.
2. Begin a separate warm-up interval only after the lane has reached the same
   ready state used for visual capture.
3. Sample actual presented or rendered frame intervals for at least the duration
   declared by the row.
4. Use a monotonic high-resolution clock.
5. Do not include startup, fixture loading, capture encoding, file writing, or
   shutdown time in the FPS sample.
6. Do not run capture readback during the measured interval unless readback is an
   intentional part of the product's normal frame path.
7. Do not use fixed-delta or synthetic-frame timing for performance evidence.
8. Record individual frame intervals internally so percentiles are calculated
   from the sample rather than inferred from aggregate FPS.
9. Detect insufficient samples, clock anomalies, zero-duration samples, NaN or
   infinite values, and renderer/device mismatches as evidence errors.
10. Preserve the performance artifact even when the threshold fails so the report
    explains the failure.

## Runtime Instrumentation

### Standalone Viewer

1. Reuse the viewer's existing per-frame timing source, but accumulate frame
   intervals over a controlled warm-up and measurement window.
2. Add validation-only command-line or settings fields for the output path,
   warm-up duration, sample duration, and expected matrix identity.
3. Write the common JSON artifact before normal validation shutdown.
4. Ensure DirectX, Vulkan, OpenGL, Android GLES, Android Vulkan, and macOS Metal
   use the same measurement implementation where their application loops allow
   it.
5. Keep the window-title FPS counter diagnostic-only; it is an instantaneous
   display value and is not the validation result.

### Web Player

1. Adapt `code/projects/web/app/tests/web-player-performance.mjs` to emit the
   common evidence schema in addition to its detailed diagnostic report.
2. Keep the existing ten-second `requestAnimationFrame` sampling and percentile
   collection.
3. Fix the viewport, device pixel ratio, browser version, fixture, selected heavy
   playback point, and canvas size used by CI.
4. Make the FPS gate part of the main WebGL validation evidence rather than an
   unrelated optional benchmark.
5. Record whether the run was headless and the browser/GPU renderer description
   for diagnosis, but do not change the matrix identity based on those fields.

### Unity

1. Add a validation smoke component that samples unscaled frame intervals after
   the existing smoke fixture reports ready.
2. Use the standalone player for authoritative runtime measurements. Editor Play
   Mode measurements may remain diagnostic because Editor overhead is not stable
   enough for the gate.
3. Emit the common JSON artifact to the lane's existing artifact directory or an
   Android-accessible application data path.
4. Add the output path, warm-up, sample duration, and matrix identity to the
   existing smoke-player command-line configuration.
5. Pull the artifact from Firebase Test Lab for Unity Android Vulkan alongside
   the current captures and status evidence.
6. Ensure visual capture and performance sampling are sequential so synchronous
   screenshot work does not contaminate the measured interval.

### Godot

1. Add performance sampling to the validation scene or a dedicated validation
   helper that records real frame intervals after the IMM fixture reports ready.
2. Write the common JSON artifact to an explicit CI output path on desktop and to
   `user://` on Android for Firebase collection.
3. Run the measured portion without `--fixed-fps`; retain fixed FPS only for
   deterministic non-performance checks that require it.
4. Keep visual capture and performance sampling sequential.
5. Pull the Android Godot Vulkan artifact from Firebase Test Lab alongside the
   existing render and composition captures.

## Shared Verification and Report Integration

1. Add `tests/tools/verify_performance_evidence.py` to:
   1. Load the selected matrix rows.
   2. Discover `performance.json` artifacts by manifest identity.
   3. Validate the evidence schema and matrix identity.
   4. Verify the configured warm-up, sampling duration, resolution, and fixture.
   5. Apply the row's mean-FPS and p95 frame-time targets.
   6. Fail on missing or duplicate authoritative evidence.
   7. Produce a machine-readable summary for the final report.
2. Extend CI manifests and artifact summaries to classify `performance.json` as
   performance evidence without confusing it with visual render metrics.
3. Do not add FPS fields to the current visual metrics schema. Visual correctness
   and runtime performance have different completeness and failure contracts.
4. Extend `tests/tools/verify_matrix_evidence.py` so a targeted renderable row is
   incomplete unless it has passing performance evidence.
5. Extend `tests/tools/write_visual_evidence_report.py` with a Performance Matrix
   table containing:
   1. Matrix cell.
   2. Mean FPS.
   3. p95 frame time.
   4. Configured target.
   5. Sample duration and frame count.
   6. Pass, fail, missing, or not-applicable status.
6. Include performance failures in each lane's detailed report section and in the
   top-level failure summary.
7. Make the final report command return nonzero when performance evidence required
   by `--required-evidence-scope` is missing or failing.
8. Preserve the current behavior in quick or partial workflows: rows outside the
   requested evidence scope appear as not tested rather than failed.

## CI Wiring

1. Add performance collection to the existing runtime job for each applicable
   row; do not create a second complete matrix of duplicate application runs.
2. Run the performance verifier after the runtime artifact has been collected and
   before the lane manifest is finalized.
3. Use a `performance` failure class, or another explicitly named class, so a low
   FPS failure is not reported as visual corruption or infrastructure failure.
4. Upload the evidence even when verification fails by using the same always-run
   manifest and artifact pattern as the existing visual lanes.
5. Download performance evidence into the existing Validation Evidence job and
   include it in `CIValidationEvidence`.
6. Update workflow contract tests so every targeted matrix row is statically
   verified to produce and check performance evidence.

## Target Calibration

1. Initially run measurement in report-only mode for several scheduled or full
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
8. Enable hard failure one row group at a time: desktop standalone, engines,
   Android devices, then web.

## Tests

1. Unit-test the performance evidence verifier for passing values, low mean FPS,
   excessive p95 time, missing evidence, malformed JSON, wrong matrix identity,
   wrong resolution, inadequate duration, duplicate evidence, and non-finite
   values.
2. Test boundary behavior at exactly the configured minimum and maximum.
3. Test that preflight, deferred, unsupported, and out-of-scope rows do not create
   false missing-evidence failures.
4. Test report rendering for passed, failed, missing, malformed, and not-applicable
   cells.
5. Test that a performance failure remains visible when visual evidence passes.
6. Test that a visual failure remains correctly classified when performance also
   fails.
7. Extend workflow verification to require the expected performance producer,
   verifier, artifact include, and report input for each targeted lane.
8. Run the existing CI tool self-tests to ensure visual evidence behavior is
   unchanged.

## Rollout Phases

### Phase 1: Contract and Report

1. Add the matrix policy schema and shared performance evidence schema.
2. Implement the verifier and its unit tests.
3. Add the Performance Matrix and detailed sections to the final report.
4. Keep targets optional so existing lanes remain unaffected during development.

Exit condition: synthetic artifacts can exercise every performance report and
failure state without running an engine or viewer.

### Phase 2: Standalone and Web

1. Add standalone machine-readable sampling on desktop renderers.
2. Adapt the existing web performance test to the common schema.
3. Wire the artifacts through manifests and the final report.
4. Collect report-only calibration results.

Exit condition: all supported desktop standalone and WebGL rows publish valid,
visible performance evidence.

### Phase 3: Unity and Godot Desktop

1. Add Unity player sampling and artifact output.
2. Add Godot real-time sampling without fixed FPS during measurement.
3. Wire Windows and macOS engine lanes into the common verifier.
4. Collect report-only calibration results.

Exit condition: all supported desktop Unity and Godot runtime rows publish valid,
visible performance evidence.

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

Exit condition: all 14 renderable supported rows are performance-gated, the two
preflight rows are explicitly not applicable, and the final validation report
identifies low FPS as a distinct actionable failure.

## Estimated Effort

1. Shared schema, verifier, report integration, and tests: less than one working
   day.
2. Standalone and web integration: approximately one working day.
3. Unity, Godot, and Firebase Android integration and stabilization: approximately
   two to four working days.
4. Total implementation estimate: approximately three to five working days,
   excluding the elapsed CI time needed to collect enough calibration runs.

## Risks and Mitigations

1. Runner variance can produce false failures. Use controlled settings, warm-up,
   sustained sampling, loose initial thresholds, and per-row targets.
2. V-sync or display caps can hide improvements but still expose severe
   regressions. Treat the gate as a lower-bound regression check rather than a
   comparative benchmark.
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

## Completion Criteria

1. Every supported renderable matrix row declares an approved performance target.
2. Every targeted lane emits valid `imm-performance-evidence-v1` evidence from a
   real-time sustained sample.
3. The final validation report shows FPS, p95 frame time, target, sample size, and
   verdict for every applicable matrix cell.
4. Missing evidence and threshold misses fail required validation scopes.
5. Preflight, deferred, unsupported, and unrequested cells remain correctly
   classified without false failures.
6. Performance failures are distinct from visual, compositing, runtime, and
   infrastructure failures.
7. Automated tests cover schema validation, threshold boundaries, evidence
   discovery, report output, and workflow wiring.
8. At least one controlled regression test demonstrates that a gross frame-rate
   regression is caught while the produced visual evidence can still pass.
