# Gallery Viewer loading comparison

Date: 2026-09-07.

## Result

The upper workload's bounded background-loading median improved by about 7%,
from 4.88 to 4.55 seconds. Variation is large: one reusable run took 14.69 seconds.
The medium workload was approximately neutral. These results do not establish
a consistent user-visible speed-up, and do not reproduce the browser app's
earlier 48% result in Gallery Viewer.

## Correctness and measurement scope

1. Gallery Viewer's existing built bundle and IMM fixture ran against the local
   built library in visible Chrome, using temporary profiles at 1440 x 900.
   The normal `IMMAsset.update()` path drives Gallery's render loop. The owned
   comparison substitutes `playback.advance()` inside that same update method;
   other setup, including evaluator construction, is shared.
2. The test holds authored time at one second with audio disabled. The initial
   time-zero trial rendered a black opening frame despite submitting geometry.
   Its timings are excluded here. Captures at one second show visible content.
3. Every reported run passes a nonblack-pixel check excluding edge controls.
   Central scene pixels have identical SHA-256 hashes across both modes and
   rounds, as do resource counts, packet payloads, and authored time. This checks
   that the final rendered scene and loaded resource work match.
4. The upper comparison loads 600 of 44,499 deferred resources after the normal
   initial load of 1,534 resources. It does not measure full-file completion.
   The medium comparison completes all 357 deferred resources.
5. Five upper and three medium rounds alternate mode order. Background duration
   runs from the library asset becoming available to completion of the selected
   resource slice. Browser long tasks and animation frames are also recorded.
   The visible upper content is the opening title scene, not a later heavy view.

## Measurements

Values are medians across rounds, except the explicit completion range. Update
cost is the median of each run's mean `IMMAsset.update()` cost. Background long
tasks must fall wholly within the background interval; the table does not
include initial-load tasks crossing that boundary. Zero means none exceeded
Chrome's 50 ms long-task threshold, not that no work occurred.

| Workload / evaluation | Background median | Range | Update cost | Worst background long task |
|---|---:|---:|---:|---:|
| medium / owned | 1.00 s | 0.96–1.01 s | 0.56 ms | 58 ms |
| medium / reusable | 0.97 s | 0.97–1.03 s | 0.51 ms | 51 ms |
| upper / owned | 4.88 s | 4.61–6.34 s | 13.05 ms | 55 ms |
| upper / reusable | 4.55 s | 4.01–14.69 s | 11.65 ms | 51 ms |

The upper median mean frame interval remained about 16.7 ms in both modes.
There is no robust elimination of stalls: upper worst-background-task medians
were 55 and 51 ms, and the slow reusable outlier remains part of the results.
Observed peak heap medians were about 442 and 437 MB. These are sampled heap
levels, not total allocation volumes or precise peaks.

## Reproduction

From `code/projects/web/app`:

```sh
node tests/gallery-viewer-performance.mjs --loading-comparison --workload upper --rounds 5 --screenshots
node tests/gallery-viewer-performance.mjs --loading-comparison --workload medium --rounds 3 --screenshots
```

The default fixed authored time is one second. `--time-seconds` selects another
point; a black result is rejected. `--background-limit` controls the slice,
defaulting to 600 for upper and all resources otherwise. `--gallery` selects
a Gallery Viewer checkout; private corpus paths stay in the ignored mapping.
Raw reports retain per-frame durations, update costs, long tasks, telemetry,
and pixel checks. This test does not cover full upper-file loading, active
transport during loading, later chapters, or GPU-heavy viewpoints.
