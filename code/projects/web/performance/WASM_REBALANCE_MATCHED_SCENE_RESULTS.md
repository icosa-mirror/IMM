# Matched loaded-scene performance

Date: 2026-09-07.

## Result

After warm-up, reusable evaluation reduced average scene-update CPU time by
about 19% in Gallery Viewer and 29% in the Three.js browser app. Both remained
near 60 fps on this machine. This is CPU headroom, not a demonstrated substantial
frame-rate increase. The earlier 48% figure measured a different loading phase
in a short, unmatched browser-app test and should not be used as the Gallery
playback gain.

## Representative workload and controls

1. The test uses a populated later chapter of the upper workload, beginning at
   821.4375396825396 seconds. Its initial visible state has 65 paint layers,
   including 32 animated layers. The opening title screen is excluded.
2. All 1,413 deferred resources used by the 30-second segment are loaded before
   timing. The planner samples the exact 60 Hz trajectory, including visible
   zero-opacity layers that the renderer still visits. Normal initial resources
   are also loaded. This is a fully resident segment, not the entire file.
3. Each mode warms up for one complete 1,800-frame pass, then measures two passes:
   3,600 frames, taking approximately 60 seconds. Both modes use identical fixed
   authored-time steps and camera poses. Unexpected stops or time divergence fail
   the test. Camera interaction is disabled during measurement.
4. Both hosts use Three.js 0.185.1, visible Chrome with a temporary profile, a
   1440 x 900 viewport, 45-degree vertical field of view, matching near/far planes
   and stencil settings, and disabled audio. The project now pins Three.js
   0.185.1 and @types/three 0.185.4, matching Gallery Viewer's installed versions;
   the built library declares a Three.js 0.185.x peer dependency.
5. The ordered resource identities and packet byte counts match across all four
   runs. So does the hash of the per-frame draw-call/triangle sequence: each mode
   renders 2,118,791,024 triangles over the measured frames (about 589,000 per
   frame). Starting captures show visible scene content; owned/reusable pixel
   hashes match within each host. Small cross-host raster differences remain.

## Sustained playback measurements

Scene-update time covers evaluation and applying the result, not the complete
CPU/GPU frame. This is one sustained trial per host/mode; the maximum frame
interval is an observed extreme, not a stable tail-latency estimate.

| Host / evaluation | Mean update | FPS | P95 frame interval | Worst frame interval |
|---|---:|---:|---:|---:|
| Gallery Viewer / owned | 8.88 ms | 59.85 | 16.9 ms | 49.9 ms |
| Gallery Viewer / reusable | 7.22 ms | 59.98 | 16.9 ms | 33.3 ms |
| Browser app / owned | 10.96 ms | 59.39 | 16.9 ms | 33.7 ms |
| Browser app / reusable | 7.81 ms | 60.00 | 16.9 ms | 17.0 ms |

## Validation and reproduction

The app and library builds, all 58 unit tests, Gallery's IMM smoke test, and the
full browser suite pass after the Three.js upgrade. The first full-browser run
failed the audio-drift limit (61 ms versus 50 ms); a repeat passed without
changing that limit or the audio implementation.

From `code/projects/web/app`:

```sh
node tests/gallery-viewer-performance.mjs --workload upper --loading-comparison --rounds 1 --scene-playback --measure-playback --time-seconds 821.4375396825396 --scene-duration 30 --screenshots --output <gallery-report.json>
node tests/web-player-delivery-performance.mjs --evaluation-comparison --match-gallery --workload upper --rounds 1 --scene-playback --measure-playback --time-seconds 821.4375396825396 --scene-duration 30 --screenshots --output <app-report.json>
```

The loading phase prepares the selected scene; only `playbackSample` contains
the sustained playback result. Reports retain loading data separately. Resource
paths remain in the ignored private corpus mapping.

The shorter title-scene loading tests remain distinct diagnostics. Their timing
variability, startup work, different earlier Three.js versions, audio settings,
and camera setup prevent treating the old loading percentage as a general
playback improvement. This scene test does not cover all chapters, devices,
audio-enabled playback, full-file residency, or interactive camera motion.
