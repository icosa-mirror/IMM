# Wasm rebalance: delivery experiment and stall trace

Date: 2026-09-07.

## Decision

Single requests without added delivery pacing remain the production default in
both the app and library. Batching and pacing remain explicit benchmark options.
The experiment does not establish a useful elapsed-time improvement from either
change. This rejects enabling this delivery policy by default; it does not reject
the broader shared-Wasm-runtime strategy.

The importer fix that initializes omitted view directions remains independently
validated by exact eager/single-staged/batched geometry comparisons.

## Equal-work experiment

1. Eighteen unprofiled trials cover two workloads, three modes, and three rounds.
   Each round rotates the mode order: A/B/C, B/C/A, C/A/B. All use visible Chrome
   152.0.7977.76 with temporary contexts and a 1280 by 720 viewport.
2. Initial loading is unchanged. Playback is held at its initial position during
   background delivery. Each medium trial completes the same 357 deferred items
   and 235,537,416 packet bytes; each upper trial completes the same 600 items and
   144,056,690 packet bytes. All trials remain staged, without errors or fallback.
3. Other agents and background tasks were active on this PC. Their load was not
   controlled or recorded. These are exploratory results, despite the matching
   browser, workload, and rotated order. Small differences are not acceptance
   evidence. Profiling was run separately from these trials.
4. Request time includes response validation on the single-request path. Explicit
   yield time is measured separately. These counters are explanatory, not
   disjoint components that can all be added together.

| Workload / mode | Completion median (s) | Range (s) | Message median | Worst-task median (ms) |
|---|---:|---:|---:|---:|
| Medium / Single requests | 1.017 | 0.983–1.151 | 357 | 0 |
| Medium / Single + pacing | 1.182 | 0.982–1.192 | 357 | 50 |
| Medium / Batch + pacing | 1.004 | 0.952–1.309 | 67 | 56 |
| Upper / Single requests | 38.679 | 38.331–56.231 | 600 | 3647 |
| Upper / Single + pacing | 48.098 | 44.058–53.827 | 600 | 4265 |
| Upper / Batch + pacing | 39.651 | 38.087–41.401 | 283 | 2134 |

Batching cuts message count substantially, but median completion is essentially
unchanged. Its shorter upper-workload worst stalls are worth further testing,
but their cause and repeatability are unresolved on the busy machine. Pacing
alone does not establish a benefit. Reducing request wait can simply move time
into delivery yields: upper single+pacing records about 21.9 seconds of median
yield waits. Native decode plus packet-build time is roughly 4.5–4.8 seconds for
the same upper workload, while end-to-end background delivery takes tens of seconds.

The measurement window starts when background delivery begins and ends after
applying the last selected item. Frame and long-task metrics include tasks that
overlap its boundary; they do not establish steady-state playback performance.

## Full-workload trace

A separate run captured the original load, settle, chapter-seek, and playback
harness, using the experimental batch+pacing path. This trace is diagnostic;
profiling overhead and unrelated machine load affect its durations. Raw trace
and reports remain ignored machine-local artifacts.

| Instrumented span | Captured spans | Accumulated wall time | Longest span |
|---|---:|---:|---:|
| Scene evaluation | 591 | 24.45 s | 801.6 ms |
| Per-frame Three.js adapter | 591 | 6.04 s | 367.0 ms |
| Rendering | 678 | 3.31 s | 274.0 ms |
| Background delta adapter | 256 | 2.53 s | 528.7 ms |
| Packet validation | 119 | 0.015 s | 1.0 ms |

Counts describe captured trace spans, not decoded-resource counts. An endpoint
audit found no unpaired user-timing endpoints.

1. CPU sampling attributes about 15.96 seconds to garbage-collector activity.
   This overlaps wall-time spans and must not be added to them as independent
   work. Samples also point to scene evaluation, transform composition, quaternion
   normalization, animation-key filtering, and Three.js matrix operations.
2. A 1.674-second main-thread task contains about 1.566 seconds of GC samples.
   A 2.476-second task contains about 1.165 seconds of GC samples. The largest
   3.227-second task is attributed only to browser/native `(program)` activity;
   its specific cause remains unresolved.
3. The upper document contains 5,402 layers and 56,430 animation keys. Current
   evaluation calls `keysFor()` six times per layer, allocating 32,412 filtered
   arrays per evaluation, in addition to state maps and transform intermediates.
   The 591 instrumented evaluations alone imply about 19.2 million such temporary
   arrays. This is a deterministic allocation count inferred from source and
   verified metadata, not a heap allocation profile.
4. This evidence identifies scene-state evaluation and allocation as a material
   target. It does not establish that all GC comes from evaluation, explain the
   unclassified longest task, or prove that a C++ implementation would be faster.

## Next work

1. Profile allocation by call site and test a compact, reusable evaluation
   representation against the current evaluator. Preserve timeline, visibility,
   transform, seek, and chapter semantics with identical-input comparisons.
2. Measure evaluation and allocation independently of rendering, then repeat the
   browser workload. Require a large, repeatable improvement before replacing
   the default path. Avoid accepting changes on message counts or attributed
   transfer-wait reductions alone.
3. Use the resulting evidence to decide the order of the immutable-resource work
   and renderer-neutral timeline migration. Resource packet batching can remain
   available for a later measured workload where it is useful.

## Validation and reproduction

1. All 53 app tests pass, including delivery ordering, cancellation, invalid
   responses, mode selection, message accounting, and the single-request default.
   Production app/library builds and visible Chrome smoke testing pass.
2. Run `node tests/web-player-delivery-performance.mjs --paths <local-mapping.json>
   --output <report.json> --rounds 3` from the app directory. The local mapping
   maps private filenames to full paths; public reports use workload labels.
3. `--trace` adds diagnostic traces to that harness. For the original full
   workload, set `IMM_WEB_TRACE_OUTPUT` to a local output prefix when running
   `tests/web-player-performance.mjs`. Keep traced runs separate from timing trials.
