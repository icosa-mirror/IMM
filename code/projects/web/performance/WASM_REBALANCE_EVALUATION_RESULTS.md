# Reusable scene evaluation results

Date: 2026-09-07.

## Change and decision

The Three.js browser app and library `IMMAsset.update()` now use indexed
animation-key lists and reusable evaluation containers. It creates one `ImmFrameEvaluator`
per document and consumes each borrowed frame immediately. This is a TypeScript
runtime change; no additional timeline semantics moved into Wasm in this step.

The existing `evaluateImmDocument()` and `ImmPlaybackController.advance()` APIs
still return independent snapshots. `IMMAsset.update()` now returns borrowed
snapshot state valid until its next update, enabling reuse in Gallery Viewer
without changing its integration. The library also exports `ImmFrameEvaluator`. Its frame is valid until
its next evaluation; `rebuild()` is required after hierarchy or animation-key
structure changes. Staged drawing, picture, and sound replacement does not
require rebuilding the index.

The implementation shares evaluation and interpolation code between owned and
borrowed paths. It removes six per-layer filtered-key arrays, reuses local/state/
context containers, and resolves parent links once. Transform arithmetic remains
unchanged and still allocates temporary values. A frozen pre-change evaluator is
retained only in test/benchmark fixtures, outside shipped application bundles.

## Correctness

1. Unit comparisons cover interpolation modes, visibility/timeline offsets,
   waiting, backward and forward seeks, transport actions, frame ownership,
   metadata rebuilds, and staged drawing replacement.
2. Browser comparisons against the frozen evaluator match all serialized frame
   state at 36 medium and 46 upper-workload times, including chapter boundaries.
3. All 58 app tests, production app/library builds, the Chrome smoke test, and
   the broader Chrome suite pass. The broader suite exercises controls, chapter
   changes, audio, deterministic scene captures, and embedding.
4. The broader browser test's fixed 300 ms fixture delay was replaced with an
   explicit request gate: clipboard interaction now completes before the default
   fixture is released, removing a test race.

## Evaluation-only measurements

Visible Chrome 152.0.7977.76 ran five alternating samples of 100 evaluations per
path and workload. Both paths used identical metadata and time sequences, with
warm-up and explicit GC between samples. These tests exclude rendering/transfer.

| Workload | Original evaluator | Reusable evaluator | Reduction |
|---|---:|---:|---:|
| Medium, median per 100 evaluations | 20.1 ms | 16.4 ms | 18% |
| Upper, median per 100 evaluations | 628.3 ms | 454.4 ms | 28% |

A separate three-path confirmation measured medians of 536.6 ms for the frozen
reference, 502.3 ms for the refactored owned-snapshot API, and 392.3 ms for reusable
frames. Thus the browser comparison does not rely on a deliberately slower owned
baseline.

Allocation sampling included objects collected by minor and major GC, with a
16 KiB sampling interval. Estimated allocations per 100 upper evaluations were
959.8 MB for the frozen reference, 974.6 MB for the owned API, and 645.6 MB for
reusable frames: roughly one third less allocation. These are sampled allocation
volumes, not retained heap sizes or exact byte counts. Transform composition,
interpolation, and array operations remain substantial allocation sources.

## Equal-work browser comparison

Three alternating rounds compared owned and reusable evaluation with single
resource requests held constant. Each medium trial loaded the same 357 deferred
items; each upper trial loaded the same 600. Packet payloads match across modes:
235,537,416 bytes for medium and 144,056,690 bytes for upper. Every run remained
staged without browser errors or fallback.

Playback stayed at its initial position. Both source files specify
`animateOnStart: false`; the harness now also explicitly pauses transport and
asserts zero time and paused state, so future inputs cannot invalidate this
condition. These measurements isolate background loading and per-frame state
work; they are not a claim about all chapter/seek/GPU-heavy workloads.

| Workload / path | Completion median | Completion range | Worst-task median | Observed peak JS heap median |
|---|---:|---:|---:|---:|
| Medium / owned | 1.11 s | 0.98–1.13 s | 90 ms | 438.8 MB |
| Medium / reusable | 1.07 s | 0.99–1.41 s | 0 ms | 434.0 MB |
| Upper / owned | 36.39 s | 33.07–38.00 s | 7,988 ms | 743.1 MB |
| Upper / reusable | 18.99 s | 16.35–21.47 s | 1,417 ms | 392.9 MB |

Upper median completion improved by approximately 48%, and every reusable run
completed faster than every owned run. Upper mean frame time fell from a median
97.0 to 47.6 ms; the 95th percentile fell from 216.9 to 166.7 ms. The medium
workload was approximately neutral in the browser comparison.

Other agents and tasks were active on this PC. Absolute timings and stall/heap
extremes are noisy, and the sample is small. The consistent separation in upper
completion times, exact work accounting, evaluator parity, and allocation
reduction support enabling this change in the Three.js browser app. They do not
establish a universal speed-up or eliminate the remaining long stalls.

## Reproduction and remaining work

1. `tests/web-player-evaluation-performance.mjs` performs real-metadata parity and
   evaluator-only measurements. `tests/web-player-evaluation-allocation.mjs`
   compares the frozen, owned, and reusable paths and records allocation samples.
2. `tests/web-player-delivery-performance.mjs --evaluation-comparison --rounds 3
   --output <local-report.json>` performs the equal-work browser comparison.
   Private source lookup remains in ignored machine-local files.
3. Investigate remaining transform/interpolation allocations and native/browser
   long tasks before adding more caching or moving timeline semantics into Wasm.
   Resource identity, upload reuse, disposal, and residency work remain open.

## Gallery Viewer integration

Gallery Viewer calls `IMMAsset.update()` each render frame and immediately uses
its authored camera result; it does not retain the evaluation snapshot. The
library's normal update path now owns one reusable evaluator. No additional API
or Gallery Viewer source change is required. Authored camera transforms remain
copies, and transport/audio handling remains in the same update method.

The Gallery Viewer smoke test passed before and after rebuilding the library,
including geometry, playback advancement, chapter selection, and explicit
viewpoints. All 58 IMM app unit tests and library package validation pass.

`tests/gallery-viewer-performance.mjs` serves Gallery Viewer's built bundle and
existing IMM fixture against the local built IMM library. It checks borrowed
frame identity and parity at seeks/chapter boundaries after staged loading has
completed, then measures Gallery's actual content updater and frame intervals.
The comparison substitutes the existing owned evaluator inside the same library
update method. Three rounds alternate modes, each with 30 warm-up frames and
120 measured frames, advancing identical authored times at 60 ticks per second.
Audio is disabled for timing; the separate smoke test runs with audio enabled.

Visible Chrome uses a temporary profile and a 1440 x 900 viewport. Results below
are medians across the three rounds; update values are per-round means.

| Workload | Owned update | Reusable update | Frame interval, both modes |
|---|---:|---:|---:|
| sample1.imm | 0.270 ms | 0.252 ms | 16.7 ms |
| medium | 1.056 ms | 1.022 ms | 16.7 ms |

These short playback samples show no measurable frame-rate improvement: both
paths remain approximately 60 fps. Their small update-cost differences are not
evidence of a broad speed-up. They establish that Gallery uses the reusable
path with matching evaluation and rendered work. The earlier browser-app loading
results do not establish Gallery loading performance or performance on the upper
workload. Those require separate measurements; so do longer playback, later
chapters, and GPU-heavy views.

Run `node tests/gallery-viewer-performance.mjs --workload medium` from the IMM
web app directory. `--gallery <checkout>` selects a Gallery Viewer checkout; the
default is the sibling `gallery-viewer` directory. Omit `--workload` for
`sample1.imm`. Private workload paths remain in the ignored corpus mapping.
