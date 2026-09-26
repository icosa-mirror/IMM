# Staged sound arrival: repeated audio decoding

When a sound arrived during Gallery background loading, `IMMAsset` disposed its
audio manager, created another context, and prepared every resident sound again.
The fix retains the manager and prepares the arriving sound. Existing decoded
audio, mute state, context and active sources survive the arrival.

## Evidence and scope

1. A bounded later-scene navigation diagnostic exposed a substantial queue delay.
   After about 31 seconds, 69 of 70 resources needed for that frame were missing.
   This was a censored run; it did not measure completed chapter navigation.
2. Coarse attribution found 26.7 seconds in ten sound-arrival callbacks, 0.14
   seconds in other adapter work, and 0.17 seconds in native decoding. Browser
   audio preparation repeatedly decoded sounds already resident. The earlier
   selected-scene preloading tests had hidden the actual queue behavior.
3. The isolated comparison changes only audio preparation and retains the
   accepted initial-resource order and batched background delivery. Each run
   processes the same first 373 background resources on the upper workload:
   59,907,467 packet bytes, identical order/content-size hash. Audio is enabled;
   authored playback is paused at the later scene to hold demand constant.
4. All timings below include approximately 1.2 seconds of identical diagnostic
   planning, which is not production navigation work. Runs use visible Chrome,
   a fresh automation profile, the shared benchmark turn and three alternating
   pairs. Library, worker, Wasm, host and input hashes are retained with raw runs.

| Identical background work | Before | Incremental audio |
|---|---:|---:|
| Elapsed median, including planning | 26.86 s | 4.49 s |
| Elapsed range | 25.75–27.45 s | 4.39–4.60 s |
| Audio decode calls | 55 | 10 |
| Newly arrived sound layers | 10 | 10 |
| Audio decode failures | 0 | 0 |

The median elapsed reduction is about **83% for this bounded queue segment**.
It is not an 83% reduction in complete-file loading or completed chapter latency.
The target chapter remains behind unrelated queued resources in both variants.
No queue reprioritization is included in this fix.

## Regression checks and decision

1. Complete medium-workload background loading remains roughly one second in
   three alternating pairs: baseline 0.941–0.992 s, candidate 0.951–1.000 s.
   All 357 deferred resources and final rendered pixels match. This workload
   does not reproduce the upper workload's staged-audio penalty.
2. Unit coverage checks retaining resident sounds and mute state, sharing an
   in-flight initial decode, disposal during decoding, failed arrivals, and
   continuing existing sources when another sound arrives. The final suite has
   63 passing tests; the prototype-only tests removed during cleanup are archived.
3. The browser integration suite, Gallery IMM audio/playback/navigation smoke,
   application build and library/package checks pass. Gallery smoke also passed
   after removal of the unrelated loading prototypes.
4. The audio manager retains one decoded buffer per resident sound, as before,
   and clears residency at disposal. Total browser/GPU memory and continuous
   audible playback during the upper queue comparison were not measured. Unit
   continuity checks and browser audio tests provide narrower correctness evidence.
5. Accept incremental audio preparation. Archive and remove unaccepted initial
   visibility, metadata-key packing and response-buffer prototypes; their
   combined comparisons did not establish independent benefits. Keep scheduling
   as a separate future investigation using this corrected audio baseline.

Raw paired reports are local `navigation-audio-pair-<round>-<variant>.json`
artifacts. Reproduction uses `gallery-viewer-performance.mjs --navigation-audit
--audio --workload upper --navigation-items 373 --navigation-budget-ms 45000`,
with `--library-dir` selecting an explicitly built baseline or candidate.
The diagnostic deliberately stops at its budget and records missing target
resources rather than treating that stop as background completion.
