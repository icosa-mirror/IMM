# Wasm runtime rebalance Phase 3 progress

Implementation and validation date: 2026-09-07.

Latest progress: reusable scene evaluation is now enabled in the Three.js
browser app and library `IMMAsset.update()` used by Gallery Viewer. The browser
app upper-workload equal-work comparison improved from 36.4 to
19.0 seconds median, with parity against the original evaluator and about one
third less sampled allocation. Gallery integration checks pass; short playback samples remain approximately
60 fps with either evaluator, without a measured frame-rate gain.
See `WASM_REBALANCE_EVALUATION_RESULTS.md` for scope, validation, and limitations.

The subsequent three-way experiment did not establish a completion-time benefit.
Production loading now defaults to single requests without added pacing; batching
remains opt-in for experiments. The stall trace identifies scene evaluation and
GC as material costs. See `WASM_REBALANCE_DELIVERY_EXPERIMENT.md` for the current
decision, measurements, limitations, and next work. Historical results below
record the earlier experimental batching implementation.

## Bounded background delivery

1. The worker accepts up to eight staged resource requests per message, preserving
   native load order. It returns a prefix once elapsed decode/build time reaches
   4 ms or accumulated packet payload reaches 2 MiB. The consumer resumes from
   the first unreturned item. Individual resources remain indivisible and can
   exceed either threshold.
2. The app and embeddable library share one background delivery iterator. It
   keeps one batch in flight, validates each delta before application, and yields
   through a timer between 4 ms delivery slices, including adapter time. Hidden
   tabs can continue without depending on animation frames. Cancellation is
   checked after awaits and after each consumer callback.
3. Existing single-resource and initial-load paths remain available. Shared
   message transfer latency is attributed once per batch so summed telemetry
   does not multiply the same wait by the resource count.
4. This slice does not introduce new packet schemas, resource eviction, or a
   hard residency budget. The limits are provisional and require corpus tuning.

## Validation

1. All 53 app tests pass, including delivery-mode/default coverage and five original tests covering partial responses,
   ordering, cancellation, invalid responses, and task yielding.
2. TypeScript checking, production app build with cache-version verification,
   worker syntax checking, and visible Chrome smoke testing pass. Chrome used
   the harness's temporary automation profile.
3. The compiled-Wasm smoke test passes on `sample1.imm`: 1,171 strokes,
   58,405 points, 798,922 paint triangles, and three encoded sounds. It compares
   batched drawing geometry and metadata exactly against both eager and single
   staged requests, compares batched picture/sound/spawn payloads, and rejects
   invalid batches. Both CTest checks pass after rebuilding the decoder.
4. A stricter eager-versus-staged comparison exposed uninitialized direction
   buffers for always-visible strokes. A probe against the unchanged HEAD worker
   reproduced differences confined to directions of always-visible vertices.
   The file omits those fields, and the shared importer previously left them
   untouched. It now initializes them to zero. The exact geometry regression
   test failed with the previous decoder and passes with the rebuilt decoder.
   Directional strokes retain their decoded authored directions.
5. Five medium and three practical-upper-bound corpus runs completed in staged
   mode with no errors or fallback. Results and comparison limits are below.
   The compatibility fixture has no deferred work with the normal initial window,
   so its browser smoke result alone cannot establish background responsiveness.

## Private corpus measurements

Five medium and three practical-upper-bound runs used the same source files and
sizes, visible Chrome with a temporary profile, hardware, viewport, 5-second
warm-up, and 10-second sample settings as Phase 2.1. Chrome changed from
152.0.7977.65 to 152.0.7977.76. These are historical comparisons, not interleaved
paired trials; background completion also differs in the upper workload.
Raw reports and source paths remain ignored machine-local artifacts.

All values below are medians; before values come from Phase 2.1's adapter-fix runs.

| Measurement | Medium before | Medium now | Upper before | Upper now |
|---|---:|---:|---:|---:|
| First meaningful frame (ms) | 674.3 | 666.9 | 3,353.1 | 3,155.9 |
| Background completed items | 357 | 357 | 610 | 564 |
| Aggregate transfer wait (ms) | 135.2 | 77.3 | 69,055.6 | 32,441.9 |
| Mean frame time (ms) | 16.66 | 16.66 | 129.54 | 83.36 |
| 95th-percentile frame (ms) | 16.9 | 16.9 | 250.0 | 200.0 |
| Worst long task (ms) | 60 | 57 | 3,334 | 11,232 |
| Peak Wasm heap (bytes) | 393,216,000 | 393,216,000 | 1,037,565,952 | 1,037,565,952 |
| JavaScript heap (bytes) | 442,802,195 | 441,507,564 | 507,671,152 | 413,106,698 |

1. Medium work remains 545 requested, 188 initially loaded, and all 357 deferred
   items completed in every run. Responsiveness remains near a 60 Hz interval.
2. Upper work remains 46,033 requested, 1,534 initially loaded, and 44,499
   deferred. Background completion ranges from 538 to 637 items. First meaningful
   frame ranges from 3,080.9 to 3,306.3 ms; worst long tasks range from 4,621 to
   12,539 ms. Geometry transfer remains 93,060,804 bytes.
3. Lower aggregate transfer wait and mean frame time do not establish a net
   improvement: upper background completion is lower and worst long tasks are
   longer in these runs. Browser version and non-paired execution are additional
   confounders. The Phase 3 responsiveness gate remains unmet.
4. Next measurement should compare the existing single-request path and bounded
   delivery under the same browser in interleaved trials, with batch message
   counts and separate validation, delivery-yield, adapter, and long-task timing.
   Use comparable work completion when assessing throughput and memory.

## Resource lifetime audit (source inspection)

1. Paint geometry and materials are reused while a layer's selected drawing
   stays unchanged. Switching drawings disposes the outgoing geometry/materials
   and creates new Three.js objects from the retained packet arrays. Returning
   to a previous drawing therefore requires a new GPU upload. This applies to
   animated drawing changes as well as seeks and chapter changes. See
   `ImmThreeView.#activateDrawing` and `#disposePaintResources`.
2. Picture textures/materials and model geometry/materials are created once per
   view and retained until view disposal. Their opacity updates change uniforms.
3. Packets contain stable drawing resource IDs and generation 1, but the worker
   drops that identity when returning the canonical drawing. The adapter tracks
   layer/drawing selection rather than packet generations.
4. Staged sound completion recreates the audio manager, which decodes the loaded
   sounds again. This occurs in both the app and library and should be included
   in the resource reuse measurements.
5. The canonical document retains decoded arrays for all completed drawings.
   The current active-drawing-only GPU policy limits resident paint resources
   but does not bound retained CPU arrays. Reusing inactive GPU resources must
   have an explicit measured budget; retaining all drawings would trade upload
   churn for potentially substantial additional GPU memory.

These findings identify operations to instrument; they are not measured upload
counts or a completed residency policy.

## Remaining work

1. Follow up on remaining transform allocations and browser/native stalls in
   `WASM_REBALANCE_EVALUATION_RESULTS.md`. The reusable-frame investigation is
   implemented and validated; batching remains experimental.
2. Identify the remaining browser/native long task and separate scene evaluation
   allocation from other sources of garbage collection.
3. Continue resource lifetime and residency measurements before introducing a
   cache or moving additional responsibilities into Wasm.

Phase 3 remains in progress.
