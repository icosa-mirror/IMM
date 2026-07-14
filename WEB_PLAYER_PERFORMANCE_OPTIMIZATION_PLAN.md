# Web Player Performance Optimization Plan

## Objective

Improve standalone and embedded web-player frame rate without weakening native
playback, camera, audio, transparency, or authored-animation parity. Prioritize
removing redundant work before changing rendering quality or adding caches and
complex residency systems.

This plan covers runtime frame performance. Load time, HTTP range loading, and
bounded asset residency remain separate Phase 5 concerns except where background
loading causes visible frame stalls.

## Triage principles

1. Measure representative real-world scenes, including a small baseline and at
   least one multi-thousand-layer local IMM. Private test files must remain local
   and must not be committed or uploaded.
2. Separate CPU-bound, GPU-bound, upload-stall, and memory-pressure failures.
   Do not infer the bottleneck from FPS alone.
3. Preserve native visual and playback semantics by default. Quality reductions
   must be explicit user-selectable controls, not silent parity regressions.
4. Optimize according to actual per-frame iteration counts. Prefer eliminating
   repeated whole-document work over adding speculative caching.
5. Record before/after results for the same file, viewpoint, playback timestamp,
   canvas size, device pixel ratio, browser, and hardware.

## Required measurements

Capture these values after initial buffering has completed and again while
background assets are arriving:

- average frame time and FPS over at least ten seconds;
- main-thread scripting, rendering, and idle time from a browser performance trace;
- GPU frame time sampled periodically rather than queried every frame;
- canvas pixel dimensions and device pixel ratio;
- draw calls, rendered triangles, resident geometries, and resident textures;
- number of document layers, evaluated layers, animation keys, active paint
  layers, active drawings, meshes, and playing positional sounds;
- garbage-collection frequency and JavaScript heap trend;
- frame-time spikes during drawing activation and incremental GPU upload;
- WebXR measurements separately from mono desktop because XR renders multiple
  views at headset-controlled resolution and cadence.

Interpretation:

- GPU time close to total frame time indicates a GPU/fill/geometry bottleneck.
- Low GPU time with high total frame time indicates main-thread evaluation,
  allocation, DOM, audio, or scene-graph overhead.
- Isolated long frames during background loading indicate decode transfer,
  geometry construction, shader compilation, or GPU upload stalls.
- Rising heap followed by frequent long collections indicates excessive
  per-frame allocation or unbounded residency.

## Priority 0 — establish evidence and remove instrumentation distortion

Priority: immediate. Risk: very low.

### P0.1 Add controlled performance capture

- Add an opt-in performance mode or diagnostic command that records a bounded
  sample rather than continually expanding telemetry.
- Report CPU frame time, periodically sampled GPU time, layer count, draw calls,
  triangles, pixel ratio, and canvas dimensions together.
- Add counters for document evaluations per frame and layers evaluated per frame.
- Add timing around playback evaluation, view application, audio reconciliation,
  authored-viewpoint resolution, DOM updates, and `renderer.render()`.
- Keep the normal player free of console spam and continuous trace collection.

Exit condition: a trace can identify whether a failing scene is CPU-bound,
GPU-bound, upload-bound, or memory-bound.

### P0.2 Make GPU timing periodic or on demand

Current issue: the standalone player creates, begins, ends, polls, and destroys
`EXT_disjoint_timer_query_webgl2` queries continuously. This is debug-only work
inside the production frame loop and can perturb driver scheduling.

- Sample approximately once per second, during an explicit diagnostic capture,
  or behind a debug query parameter.
- Retain the last valid result for diagnostics.
- Do not synchronously wait for a query result.

Exit condition: ordinary playback performs no continuous GPU diagnostic queries.

## Priority 1 — eliminate redundant per-frame CPU work

Priority: highest expected CPU benefit. Risk: low to moderate because the work
must preserve exact playback snapshots.

### P1.1 Evaluate the document once per frame

Current issue: the standalone loop can perform up to four complete evaluations
of the document in one frame:

1. `ImmPlaybackController.advance()` evaluates and returns a snapshot.
2. Authored viewpoint resolution calls `playback.evaluate()` again.
3. `ImmThreeView.setTimeTicks()` evaluates again.
4. Audio synchronization calls `playback.evaluate()` again.

For thousands of layers this duplicates traversal, key lookup, transform
composition, map creation, and allocation.

Implementation direction:

- Treat the snapshot returned by `advance()` as the authoritative frame state.
- Add a view method that applies an existing `ImmPlaybackSnapshot` without
  reevaluating the document.
- Pass the same snapshot to authored-viewpoint resolution and audio update.
- Ensure seek, restart, chapter selection, pause, wait/continue, staged layer
  refresh, and embedded-host updates also create exactly one authoritative
  snapshot for each state change.
- Add tests proving shared-snapshot output matches independent evaluation at
  animation keys, drawing changes, stops, loops, chapters, and viewpoint actions.

Exit condition: the normal standalone path performs one full document evaluation
per rendered frame, with identical retained playback results.

Implementation checkpoint (2026-07-14):

- Implemented one authoritative snapshot per standalone animation frame. The
  view, authored viewpoint resolver, and audio engine now consume the snapshot
  returned by `ImmPlaybackController.advance()` instead of reevaluating the
  complete document independently.
- Added a repeatable visible-Chrome benchmark harness with fixed 1280×720 output,
  device scale 1, bounded warm-up and sampling, authored-chapter scanning, and
  an explicit eager benchmark mode so later heavy chapters are fully resident.
- Clean before/after measurements used the same heavy authored chapter and tick.
  A 5,402-layer local stress document with 3,321 resident meshes, 86 draw calls,
  and approximately 998,000 rendered triangles improved from 20.24 FPS
  (49.40 ms mean frame time) to 59.44 FPS (16.82 ms). Three smaller documents
  remained display-refresh-capped at approximately 60 FPS before and after.
- The benchmark-only eager mode is opt-in and does not alter normal native-order
  staged loading. Private IMM files and identifying paths remain outside the
  repository.

### P1.2 Stop rewriting playback DOM every frame

Current issue: timeline value, chapter selection, button text, wait-button state,
and formatted playback time are written every frame even when unchanged.

- Update event-driven values only when their state changes.
- Update the visible time and timeline at a bounded rate such as 5–10 Hz.
- Preserve immediate updates after direct user input, chapter navigation,
  stop/continue, and load/reset.

Exit condition: DOM playback controls are not mutated on most animation frames.

### P1.3 Remove redundant per-frame validation and matrix updates

- Recompute host compatibility warnings only when the camera projection or host
  depth contract changes, not on every timestamp update.
- Remove the explicit root `updateMatrixWorld()` when Three.js will perform the
  same update during `renderer.render()`; retain targeted matrix updates only
  where viewer-locked content requires current matrices before render.
- Verify standalone, embedded, and WebXR camera-locked pictures after the change.

Exit condition: compatibility validation and duplicate root matrix traversal no
longer appear in the steady-state frame path.

### P1.4 Avoid trivial per-frame asynchronous allocations

- Do not call an `async` transport-state setter every frame when the playback
  state has not changed.
- Move audio context play/suspend transitions to playback-state, visibility, and
  user-interaction events.
- Continue updating active positional audio parameters at the cadence required
  for smooth spatial playback.

Exit condition: unchanged transport state creates no promise or context-state
work in the frame loop.

## Priority 2 — reduce the cost of the remaining single evaluation

Priority: high for large layer/key counts. Risk: moderate; requires careful
animation-parity tests.

### P2.1 Pre-index animation keys by property

Current issue: evaluation repeatedly filters each layer's complete key array for
visibility, offset, opacity, transform, draw-in, loop, and action properties.
This allocates temporary arrays and multiplies key scanning by property count.

- Build immutable per-property key arrays once when the document is created.
- Preserve authored stable ordering for equal timestamps.
- Use binary search or retained cursors only after measurement shows linear scans
  remain significant. Cursors must be invalidated correctly on seeks and loops.

Exit condition: steady-state evaluation creates no temporary key-filter arrays.

### P2.2 Reduce snapshot allocation

- Reuse safe frame-state containers or use double-buffered snapshots rather than
  allocating new maps and transform arrays for every layer every frame.
- Keep public snapshots immutable from consumers' perspective.
- Do not reuse objects across asynchronous code unless ownership is explicit.
- Measure garbage collection before deciding how far to take this work.

Exit condition: allocation and garbage-collection time are no longer material in
large-scene traces.

### P2.3 Distinguish static and animated layers

- Precompute which layers or ancestor chains can change with time.
- Apply transforms, visibility, opacity, and material uniforms only when their
  evaluated values change.
- Continue updating draw-in, animated brush effects, viewer-locked pictures, and
  active drawing selection where authored behavior requires it.
- Avoid a broad cache until traces quantify the number of static versus animated
  layers in real files.

Exit condition: static scenes do not rewrite every Object3D transform and material
uniform every frame.

### P2.4 Make authored viewpoint resolution event-driven

- Pre-index `MakeDefault` actions by relevant timeline/chapter.
- Re-resolve the authored viewpoint when loading, seeking, changing chapter,
  crossing a viewpoint action, or resetting playback.
- Do not scan all snapshot layers for viewpoint actions on every frame when no
  relevant boundary was crossed.

Exit condition: viewpoint selection has negligible steady-state cost while
chapter/viewpoint semantics remain identical to native.

## Priority 3 — GPU and resolution controls

Priority: conditional on GPU-bound evidence. Risk: low when exposed as explicit
quality controls; high if silently changing native rendering.

### P3.1 Add an explicit render-scale control — implemented

The standalone player now exposes the native-style Normal/High choice. Normal
is the default and uses a 1x render scale, matching the native default pixel
density. High uses device pixel ratio capped at 2x, preserving the web player's
previous behavior. In WebXR, Normal uses the runtime-recommended framebuffer
size and High requests 1.5x per dimension, following the native player's eye-
buffer scaling approach while retaining antialiasing. The CSS display size is
unchanged and the preference is stored locally.

- If two choices prove insufficient in real use, consider adding an Auto mode
  based on measured sustained frame time, with conservative hysteresis and a
  visible selected value.

Exit condition: users can trade resolution for frame rate without altering IMM
content, timing, or camera transforms.

### P3.2 Preserve multisample transparency parity

Do not disable antialiasing or alpha-to-coverage as a routine optimization. The
current native-parity transparency strategy depends on multisample coverage.
Any alternative path must be separately compared against native fades, opaque
edges, ordering, tube surfaces, and mobile browser support.

### P3.3 Measure fill rate and overdraw

- Compare GPU time at fixed CSS size using render scales 1 and 2.
- Compare representative opaque-heavy and fade/coverage-heavy timestamps.
- Record triangle count and screen coverage; triangle count alone does not
  identify paint overdraw.
- Consider coarse frustum/layer culling only if Three.js bounds and authored
  visibility are insufficient in measured scenes.

Exit condition: any geometry, shader, or culling work is justified by a measured
GPU bottleneck rather than assumed from draw-call count.

## Priority 4 — background upload and drawing-transition stalls

Priority: after steady-state P1/P2 work. Risk: moderate.

- Time staged delta application, geometry expansion, buffer upload, texture
  creation, shader compilation, and old-resource disposal separately.
- Apply a main-thread upload budget so background loading cannot monopolize one
  animation frame.
- Precompile known paint material variants before playback where this reduces a
  measured first-use shader stall.
- Avoid recreating geometry/materials when the active drawing has not changed.
- Investigate short look-ahead residency for imminent drawing changes before
  adopting Phase 5's full bounded-LRU design.

Exit condition: background loading and ordinary drawing transitions remain
within the target frame budget on representative files.

## Debug and diagnostic cost assessment

| Facility | Current cadence | Expected cost | Action |
|---|---:|---:|---|
| Visible JSON summary | Load and staged updates | Low during steady playback | Retain |
| `window.__immDiagnostics()` | Only when called | None when idle | Retain |
| FPS counters and `performance.now()` | Every frame | Very low | Retain |
| GPU timer queries | Every frame | Low to potentially material driver overhead | Make periodic/on-demand |
| Host compatibility validation | Every frame | Small allocations and repeated checks | Make event-driven |
| Playback-control DOM writes | Every frame | Potential layout/style and string churn | Throttle/event-drive |
| Audio drift measurement | Per active sound per frame | Potentially material with many sounds | Measure; retain parity unless proven excessive |

## Recommended implementation order

1. Add bounded instrumentation and capture a baseline on the current failing
   scene at fixed resolution.
2. Make GPU timing periodic and throttle/event-drive playback DOM updates.
3. Share one authoritative playback snapshot across view, viewpoint, and audio.
4. Remove per-frame compatibility validation and redundant matrix traversal.
5. Capture the same baseline again and determine whether CPU evaluation remains
   the dominant cost.
6. Pre-index animation keys, then address snapshot allocation and static-layer
   updates only if traces justify them.
7. If GPU-bound, add render scale and characterize fill/overdraw before changing
   shaders, geometry, or transparency.
8. Profile and budget incremental uploads if frame spikes remain during staged
   loading.

## Verification matrix

Every optimization must cover, in proportion to its scope:

- small committed sample and at least one large private local IMM;
- paused, playing, waiting, continue, restart, seek, and chapter navigation;
- authored viewpoint changes, including chapter-controlled transforms;
- animated paint drawing swaps and draw-in effects;
- opaque strokes and authored fades/order-independent coverage;
- flat and positional audio, including pause/seek synchronization;
- standalone mono, embedded Three.js host, and WebXR where affected;
- repeated file replacement and failed-load recovery;
- desktop Chrome plus at least one representative mobile/WebXR device before a
  production performance claim.

## Initial performance targets

Targets should be refined after baseline capture, but the first practical gates
are:

- no more than one full document evaluation per rendered frame;
- no continuous debug GPU queries in normal playback;
- no steady-state DOM mutations when displayed playback values are unchanged;
- stable 60 FPS at native refresh on the committed sample at render scale 1;
- sustained 30 FPS minimum on representative large desktop content at render
  scale 1, with a documented path toward 60 FPS where GPU capacity allows;
- no recurring frame over 50 ms caused by background staged upload after initial
  buffering;
- no visual, camera, playback, chapter, wait, or audio regression relative to
  the current native-parity behavior.

## Deferred until evidence requires it

- WebGPU renderer rewrite;
- Emscripten pthreads or `SharedArrayBuffer` deployment requirements;
- texture-backed canonical stroke expansion;
- aggressive material/mesh merging that complicates drawing swaps;
- adaptive asset eviction and HTTP-range caching beyond the existing Phase 5
  plan;
- silent reductions in MSAA, transparency quality, or authored update cadence.
