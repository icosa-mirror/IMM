# WASM and Web Runtime Responsibility Rebalance Plan

**Status:** Phases 0–2.1 complete; Phase 3 partial. Remaining migrations deferred pending evidence of substantial Gallery Viewer gains.
**Scope:** `code/projects/web` and renderer-independent portions of the native IMM runtime
**Goal:** Deliver substantial user-visible performance gains in Gallery Viewer. Shared ownership is a secondary benefit, not sufficient justification for further migration.

## Reassessment: prioritize substantial gains

The user prioritizes big gains over improvements of a few percent. The original
phase sequence is therefore not a mandatory implementation checklist. Keep the
completed tessellation and reusable-evaluation changes; do not pursue the rest
merely to finish an architectural migration.

1. **Primary target:** Gallery Viewer's actual load, animated playback, seek and
   chapter-change experience. The Three.js browser app is a diagnostic comparison
   host. A faster internal function is insufficient if the limiting user-visible
   metric does not improve.
2. **Working investment gate:** target at least roughly 25% improvement in a
   limiting end-to-end metric, or removal of a reproducible disruptive stall.
   This is a planning threshold, not a claimed result or universal acceptance
   percentage. Use repeated workload-specific baselines to distinguish gains
   from noise; do not spend another implementation cycle chasing a few percent.
3. **First investigation: drawing activation and GPU uploads.** The adapter
   disposes active paint resources on drawing changes, then builds new Three.js
   objects from retained arrays. Measure activation count, CPU cost, upload bytes,
   drawing revisit frequency, GPU time where available, and missed frame budgets
   during populated animated playback and chapter revisits. Only prototype bounded
   reuse if this work accounts for enough of the limiting cost to meet the gate.
   Never retain every drawing without a measured memory budget.
4. **Second investigation: loading work and demand.** Compare resources decoded,
   transferred and uploaded with those required for the current chapter and near
   future playback. Measure time to a populated usable scene, chapter readiness,
   loading stalls and peak memory with audio enabled. Test demand-prioritized
   loading only if unnecessary work is material; account for later seek latency,
   animation continuity and cancellation. A title screen or a fixed small batch
   completing faster is not sufficient evidence.
5. **Conditional investigation: audio loading.** Staged sound completion currently
   recreates the library audio manager and prepares loaded sounds again. Measure
   repeated decode work in the same load before considering incremental reuse.
   Previous audio-disabled benchmarks cannot establish its cost.
6. **Stop rule:** estimate the maximum possible end-to-end saving from each
   measured component before implementing a replacement. If it is too small,
   drop that candidate. If GPU work dominates, investigate measured render cost
   instead of further CPU timeline migration. Do not silently reduce visual
   quality to manufacture a performance gain.
7. **Validation:** use populated, sustained Gallery playback and realistic loading
   and navigation, identical authored work, parity checks, and repeated paired
   runs. Test a slower supported device if the desktop remains refresh-rate
   limited. Preserve normal/small workload behavior and bound CPU/GPU memory.
   Keep machine-specific reports local; commit reusable tests and justified code.

Phase 3 is narrowed to measured resource churn and loading demand. Phase 4's
native timeline migration is deferred unless profiling identifies a large
remaining timeline bottleneck and a compact prototype demonstrates an end-to-end
benefit after bridge costs. Phase 5's shader sharing and Phase 6's broad ownership
cleanup are deferred as architectural work; necessary correctness and disposal
fixes remain in scope. The plan is complete for this revised objective when
worthwhile measured candidates have been implemented and validated, or rejected
with evidence, rather than when every originally proposed migration is built.

## Current evidence and next decision (2026-09-07)

1. Apply `code/projects/web/performance/INVESTIGATION_PROCEDURE.md` before further
   optimization. Each experiment needs an explicit metric, upper bound, budget,
   identified baseline and accept/reject decision. This supersedes the earlier
   open-ended sequence of candidate implementations.
2. Drawing activation and upload submission are closed as CPU optimization
   candidates for the measured desktop scene: about 0.52 ms/frame combined is
   too little to justify a cache. GPU execution was not measured.
3. Initial visibility filtering, compact metadata keys and download buffering
   remain unaccepted. Recent loading comparisons combined these changes and
   cannot attribute timing or sampled heap changes independently. Restore the
   accepted default path and retain the prototypes for reference, not shipment.
4. Do not spend another cycle optimizing title-screen readiness. The next
   diagnostic is later-chapter readiness during an active load. Previous scene
   tests preloaded selected resources and therefore hid the real queue delay.
5. First audit existing metadata and request order, then measure the actual
   native queue until the chosen populated scene is resident, with a bounded
   time/resource budget and audio enabled. Record completed target resources,
   missing resources, elapsed time, adapter/decoder costs and stalls. A timeout
   is a censored result, not an invented completion time or a reason to run the
   entire file indefinitely.
6. Only if queue delay is substantial, compare a harness-only priority reorder
   of the same resource work. Include planning cost and eventual resource parity.
   This tests the opportunity before designing a production scheduler. If it
   does not meet the investment gate, stop and report that result.

## Decision after the bounded navigation diagnostic

1. The actual queue diagnostic found 69/70 target-frame resources still missing
   after 31 seconds. Coarse attribution located 26.7 seconds in sound-arrival
   preparation, not scene refresh or native decoding. This qualifies incremental
   audio preparation before scheduler work.
2. Retaining the audio manager and decoding only new arrivals reduced the same
   373-resource queue segment from a median 26.86 to 4.49 seconds across three
   alternating pairs (about 83%). This is not completed chapter navigation.
3. Accept this focused fix after unit, browser, Gallery and medium-workload
   regression checks. See performance/STAGED_AUDIO_RESULTS.md for evidence and
   limitations. Unaccepted loading prototypes were archived and removed.
4. Next, remeasure navigation using the corrected audio baseline. Only then
   estimate whether queue reprioritization can meet the investment gate. Do not
   combine that experiment with audio changes or extrapolate its benefit from
   the old 31-second baseline.

## 1. Original proposal (historical baseline)

The current web player uses WebAssembly for IMM parsing and asset extraction, then independently implements stroke tessellation, timeline evaluation, scene-state propagation, and rendering behavior in JavaScript/TypeScript and Three.js. This produces a clean browser integration boundary, but it also means that changes to native playback, tessellation, and shader behavior can require manual web ports.

The proposed architecture moves renderer-independent, high-risk semantic code into a shared C++ runtime compiled to Wasm while retaining Three.js/WebGL ownership of browser graphics resources and host-scene integration.

The target flow is:

```text
IMM bytes
  -> shared C++ importer/runtime/tessellator in Wasm worker
  -> versioned, flat render packets
  -> thin TypeScript/Three.js adapter
  -> WebGL 2 and WebXR
```

This is not a proposal to compile the complete native renderer into Wasm. GPU resource creation, draw submission, WebXR, Web Audio, browser I/O, and host application integration remain browser responsibilities.

## 2. Original boundary (before Phases 0–2.1)

The current pipeline is:

```text
IMM source
  -> C++ importer in Wasm
  -> C ABI getters
  -> JavaScript worker marshaling
  -> ImmDocument
  -> JavaScript stroke tessellation
  -> TypeScript timeline evaluation
  -> Three.js scene objects and custom GLSL
  -> WebGL 2
```

Current ownership is:

| Responsibility | Current owner | Primary location |
|---|---|---|
| IMM parsing and decompression | C++/Wasm | `code/projects/web/decoder`, `code/libImmImporter` |
| Scene and asset extraction | C++/Wasm plus worker marshaling | `imm_web_scene.cpp`, `imm-web-decoder-worker.mjs` |
| Canonical browser document | TypeScript | `app/src/format/imm-document.ts` |
| Paint stroke tessellation | JavaScript worker | `decoder/js/imm-web-geometry.mjs` |
| Timeline and animation evaluation | TypeScript | `app/src/runtime/imm-playback.ts` |
| Scene graph and GPU resources | Three.js | `app/src/render-three/imm-three-view.ts` |
| Paint, model, and picture shaders | Web GLSL embedded in TypeScript | `app/src/render-three/imm-three-view.ts` |
| Dithered alpha capability selection | Three.js/WebGL adapter | `app/src/render-three/imm-three-view.ts` |
| Audio scheduling and playback | Web Audio/TypeScript | `app/src/audio/imm-web-audio.ts` |
| File, HTTP, UI, and WebXR integration | Browser/TypeScript | `app/src` |

The existing CMake target compiles an explicit subset of native source files. A native change is inherited by the web build only when it occurs inside that source closure and does not require a new or changed web export.

## 3. Design objectives

1. Establish one authoritative implementation for renderer-independent IMM behavior.

2. Preserve the embeddable contract of one host-owned Three.js renderer, canvas, camera, depth buffer, render loop, and WebXR session.

3. Avoid fine-grained JavaScript-to-Wasm calls during frame evaluation or geometry construction.

4. Reduce peak memory by avoiding simultaneous retention of source bytes, native decoded structures, canonical JavaScript points, expanded JavaScript geometry, and GPU buffers where possible.

5. Make performance decisions from representative corpus measurements rather than assuming that C++/Wasm is inherently faster than JavaScript.

6. Keep the data boundary flat, versioned, testable, and independent of native renderer handles or Three.js classes.

7. Preserve staged loading, cancellation, deterministic disposal, and bounded residency.

8. Make unsupported native features explicit at the render-packet boundary rather than silently dropping them.

## 4. Proposed responsibility boundary

### 4.1 Responsibilities to move into shared C++/Wasm

1. Timeline evaluation rules, including visibility, opacity, looping, offsets, actions, interpolation, and drawing selection.

2. Parent-child state propagation and renderer-independent world-transform evaluation.

3. Keep-alive state evaluation where the result is independent of the graphics API.

4. Paint stroke tessellation, including tangent and basis construction, duplicated endpoint handling, brush cross-sections, flipped topology, batching, index selection, and progress attributes.

5. Renderer-independent draw-in and directional-visibility inputs.

6. Creation of flat render packets containing active resources, transforms, material parameters, vertex/index buffers, texture descriptors, and stable resource identifiers.

7. Shared validation functions that can evaluate a document or drawing deterministically without a GPU.

These responsibilities should be implemented in renderer-neutral native modules used by both the native player and the Wasm target. They should not be copied out of `libImmPlayer` into another web-only C++ implementation.

### 4.2 Responsibilities to keep in TypeScript and browser APIs

1. Fetch, file selection, HTTP range requests, caching, cancellation, and worker lifecycle.

2. Playback clock integration with `requestAnimationFrame`, page visibility, user interaction, and Web Audio scheduling.

3. Three.js `Object3D`, `BufferGeometry`, texture, material, camera, and resource-lifetime management.

4. WebGL capability detection and selection among programmable sample masks, hardware alpha-to-coverage, and alpha-hash fallback.

5. WebXR session and reference-space integration.

6. Host renderer compatibility checks, render ordering, layer masks, post-processing integration, and diagnostics.

7. UI, accessibility, browser error reporting, and application policy.

### 4.3 Responsibilities that should use shared sources but platform-specific execution

1. Dithered-alpha mathematics should come from a shared shader source or generated shader fragment, while each backend retains its own capability selection and pipeline state.

2. Blue-noise data should continue to have one authoritative source and generated platform representations.

3. Colour conversion, directional coverage, and draw-in equations should be shared or generated where shader-language differences permit it.

4. Shader conformance tests should feed identical inputs to native and web shader implementations and compare coverage masks or rendered reference images.

## 5. Render-packet contract

The Wasm boundary should change from many field-oriented getters to a small number of coarse operations. A proposed API shape is:

```c
ImmWebStatus imm_web_open_document(const void* bytes, uint32_t size, ImmWebDocumentHandle* out);
ImmWebStatus imm_web_decode_asset(ImmWebDocumentHandle document, uint32_t asset_id);
ImmWebStatus imm_web_build_drawing(ImmWebDocumentHandle document, uint32_t layer_id, uint32_t drawing_id);
ImmWebStatus imm_web_evaluate_frame(ImmWebDocumentHandle document, int64_t ticks, ImmWebFramePacket* out);
void imm_web_release_asset(ImmWebDocumentHandle document, uint32_t asset_id);
void imm_web_close_document(ImmWebDocumentHandle document);
```

The exact ABI requires a separate design review, but it should follow these rules:

1. Return offsets and lengths into contiguous packet storage rather than requiring one call per layer, stroke, or field.

2. Version every packet schema and reject unsupported versions explicitly.

3. Use fixed-width scalar types and document alignment, endianness, ownership, and lifetime.

4. Identify immutable resources separately from per-frame state so unchanged geometry is not copied every frame.

5. Use stable resource IDs and generation counters so the adapter can upload, reuse, replace, and dispose GPU resources deterministically.

6. Return dirty ranges or changed records for frame updates instead of republishing the entire scene.

7. Keep browser objects, native renderer handles, and pointers out of the public packet format.

8. Support staged metadata opening and on-demand drawing or asset decode from the start.

## 6. Performance analysis

Moving code to Wasm can improve CPU throughput and code sharing, but location alone does not guarantee better frame time. The relevant costs are computation, memory allocation, copies, bridge calls, worker messaging, upload bandwidth, GPU work, and peak retained memory.

### 6.1 Paint tessellation

Paint tessellation is the strongest initial candidate for migration because its work scales with point and vertex counts and currently performs substantial per-point vector math in JavaScript.

Potential benefits are:

1. Reuse of native topology and basis-generation code removes a high-risk parity surface.

2. C++/Wasm can provide predictable tight loops for large drawings.

3. Geometry can be produced directly in final packed layouts, avoiding intermediate JavaScript objects and repeated property access.

4. Canonical stroke buffers can be released after a drawing is packed, reducing retained memory.

5. The work remains in the existing decoder worker and therefore stays off the browser main thread.

Potential costs are:

1. Standard Wasm memory cannot normally be transferred to the main thread by detaching its backing buffer, so completed geometry generally still needs to be copied into transferable `ArrayBuffer` objects.

2. Building expanded geometry inside Wasm can temporarily increase the Wasm heap while decoded native structures are still resident.

3. A generic native layout may require repacking before Three.js upload if its stride, alignment, component types, or index partitions do not match WebGL needs.

4. Growing Wasm memory can invalidate JavaScript views and create additional memory pressure.

5. Small drawings may see no material speedup because allocation, marshaling, and transfer overhead dominate computation.

The migration should therefore produce WebGL-ready buffers in a single coarse operation, release source/intermediate storage promptly, and benchmark total decode-to-upload time rather than tessellation time alone.

### 6.2 Timeline and animation evaluation

Timeline evaluation is valuable primarily for semantic sharing. Its performance value depends on real layer and key counts.

Potential benefits are:

1. Native and web players use the same interpolation, looping, action, transform, and drawing-selection rules.

2. Large documents with many animated layers may benefit from contiguous native data and fewer temporary JavaScript objects.

3. A dirty-record packet can reduce TypeScript scene traversal and uniform updates when little changes between frames.

Potential costs are:

1. For ordinary scenes, TypeScript evaluation may already be a small fraction of frame time; moving it could have no visible benefit.

2. Calling Wasm once per layer, key, or property would likely lose time to boundary overhead.

3. Copying a complete frame state every animation frame could cost more than evaluating it directly in TypeScript.

4. Browser-side audio and render clocks still need synchronization, so moving the clock itself into Wasm would add coordination without removing browser work.

The acceptable design is one `evaluate_frame` call per document per update, returning compact changed records. Per-layer Wasm calls are not acceptable for the frame loop.

### 6.3 Shader and coverage behavior

Moving shader execution into Wasm is not possible; shaders execute on the GPU through WebGL. Compiling the native renderer to Wasm would still issue browser graphics calls and would not remove GPU-side platform differences.

Sharing shader source can nevertheless provide:

1. Lower maintenance cost for coverage, colour, visibility, and draw-in equations.

2. More direct native/web conformance tests.

3. Fewer silent mathematical differences between backends.

It can also introduce costs:

1. Cross-compiling one shader dialect to every native and browser target adds build tooling and debugging complexity.

2. WebGL 2 lacks some native shader features, so shared source still requires capability-specific variants.

3. Generated shaders can become harder to inspect in browser tooling unless source maps and readable generated outputs are retained.

The first step should share small algorithmic fragments and test vectors, not replace all shader sources with a new cross-compiler.

### 6.4 Memory and transfer behavior

Peak memory, rather than arithmetic speed, may be the limiting factor for large IMM files. Measure these categories separately:

1. Source bytes held by the browser and Wasm.

2. Native decoded scene structures.

3. Canonical stroke-point buffers.

4. Expanded paint geometry in Wasm.

5. Transferable geometry buffers in JavaScript.

6. Three.js CPU-side attributes.

7. GPU buffers and textures.

8. Prefetched inactive drawings and assets.

The implementation should avoid retaining all categories simultaneously. Staged loading, per-drawing packing, explicit release calls, and a residency budget remain necessary regardless of which language performs the work.

### 6.5 Main-thread responsiveness

Moving work into Wasm does not improve responsiveness if the Wasm module runs on the main thread. All decode, tessellation, and substantial frame evaluation should continue in a worker.

Frame evaluation in a worker introduces latency and synchronization considerations. The runtime should either evaluate slightly ahead using explicit requested ticks or keep lightweight frame evaluation on the main thread until measurement shows that worker evaluation is beneficial. Geometry construction should remain worker-only.

## 7. Benchmark and acceptance framework

Before and after every migration phase, measure the complete user-visible pipeline on representative documents rather than isolated microbenchmarks.

The Phase 2 results show that source file size is not a useful proxy for expected speedup. Benefit tracks the share of work spent expanding paint geometry, while documents with many layers or drawings can instead be dominated by lookup, packet, transfer, and adapter overhead. The current samples do not establish a file-size-to-speedup curve.

Classify every fixture by workload shape as well as source size. Record source bytes, layer count, drawing count, animation-key count, source-point count, rendered-triangle count, expanded geometry bytes, average and maximum drawing-packet size, picture/audio bytes, and packet count. One fixture may cover more than one class.

Required corpus classes are:

1. A small, low-work static document that exposes fixed bridge overhead.

2. `exampleImmFiles/sample1.imm` as the committed compatibility fixture.

3. An animation-dense document with many layers and keys.

4. A geometry-dense document with high point and expanded-vertex counts.

5. A structure-dense document with many layers and drawings relative to its expanded geometry.

6. A staged-loading document representative of practical upper-bound use, using local/private fixtures without committing private assets.

Required measurements are:

1. Metadata-open time.

2. Drawing decode time.

3. Geometry construction time.

4. Worker-to-main transfer time.

5. Three.js buffer creation and first-upload time.

6. Time to first meaningful frame.

7. Main-thread long tasks and worst frame time during loading.

8. Mean, median, 95th-percentile, and 99th-percentile frame time during playback.

9. Per-frame timeline evaluation and adapter-update time.

10. Peak Wasm heap, JavaScript heap, and estimated GPU allocation.

11. Geometry buffer bytes per source point and per rendered triangle.

12. Visual and semantic parity results.

13. Requested and effective load mode, including whether staged loading completed or fell back to eager loading and the fallback reason.

14. Requested, loaded, deferred, and background-completed drawing counts.

15. Drawing-build count, packet count and bytes, plus decode, native build, Wasm-to-JavaScript copy, transfer, and adapter cost per request.

16. Layer/resource lookup count and time where structural overhead is material.

Report medians and ranges rather than relying on a single run. Use at least five runs for small and normal fixtures and at least three independent runs for expensive upper-bound fixtures. Do not infer scaling trends from one fixture or one run.

Migration acceptance criteria are:

1. No regression in committed native/web geometry and visual parity tests.

2. No regression in staged loading, cancellation, disposal, or multiple-document behavior.

3. For small documents, median time to first meaningful frame does not regress by more than 5 ms unless the run ranges show the apparent change is noise; normal compatibility documents must not materially regress.

4. A measured reduction in either maintenance duplication, CPU time, or peak memory sufficient to justify the added ABI complexity.

5. No new per-layer or per-stroke JavaScript-to-Wasm calls in steady-state playback.

6. No main-thread task longer than the project-defined responsiveness budget caused by the migrated work.

7. Geometry-dense fixtures show separately reported geometry-build and total-worker improvements; source-byte reduction alone is not evidence of success.

8. Structure-dense fixtures stay within explicit lookup, packet, timeline, and adapter budgets.

9. Upper-bound staged tests confirm the effective load mode, time to first meaningful frame, background responsiveness, and peak memory. A silent eager fallback invalidates a staged-loading result unless fallback behavior is the subject of the test.

Numeric percentage gates beyond the small-document absolute guardrail should be set only after collecting repeated baselines on the existing supported desktop and mobile/WebXR device classes. Gates should be workload-specific rather than a single expected Wasm speedup.

## 8. Migration phases

### Phase 0: Freeze and characterize the current contract

1. Document the existing C ABI, worker marshaling layout, `ImmDocument` schema, geometry output, and ownership rules.

2. Add schema-version mismatch tests and malformed-packet tests.

3. Record baseline timing and memory measurements for the required corpus tiers.

4. Expand golden tests for all brush types, flipped transforms, duplicate endpoints, large-index batches, draw-in progress, and per-stroke masks.

5. Add timeline parity fixtures for every property, interpolation mode, loop/action combination, hierarchy propagation, and drawing-selection edge case.

Exit condition: the current implementation has deterministic correctness and performance baselines.

### Phase 1: Extract a renderer-neutral native semantic core

1. Identify native playback and tessellation logic currently coupled to renderer classes.

2. Extract only renderer-independent algorithms into shared native modules with no GPU, audio, window, VR, or OS ownership.

3. Make the native player consume those modules before exposing them to Wasm, proving that they are authoritative rather than web-specific copies.

4. Add native unit tests for the extracted APIs.

Exit condition: the native player behavior is unchanged and the shared modules have no renderer dependencies.

### Phase 2: Move paint tessellation into the Wasm worker

1. Add a coarse drawing-build API that outputs WebGL-ready packed buffers.

2. Preserve staged loading and build only requested drawings.

3. Transfer completed buffers to the main thread and promptly release Wasm intermediates.

4. Retain the JavaScript tessellator temporarily as a test oracle and fallback during validation.

5. Compare byte-for-byte geometry where representations match and semantic/visual results where they do not.

6. Remove the JavaScript tessellator only after performance, memory, and parity gates pass.

Exit condition: native and web use the same authoritative brush topology implementation, with no material normal-case load regression.

### Phase 2.1: Remove structural scaling overhead exposed by validation

Completed 2026-09-01. Five committed-fixture runs, five medium structure-heavy
runs, and three practical-upper-bound runs confirmed indexed request paths and
the requested/effective-mode contract. Removing no-op full-document adapter
refresh reduced upper-workload adapter time from 5.54 seconds to 108 ms; request
delivery/transfer latency remains dominant over native packet build and Wasm
copy. Phase 3 should begin with bounded batching and explicit main-thread pacing
for structure-dense background work.

1. Build immutable layer-ID and resource-ID indexes when a document is opened so requested drawing builds use constant-time lookup rather than repeated linear scans.

2. Add explicit requested/effective load-mode and fallback-reason telemetry.

3. Record per-request drawing decode, native build, Wasm copy, transfer, and adapter timings together with packet counts and bytes.

4. Repeat the structure-dense and upper-bound measurements before adding broader native responsibilities.

Exit condition: drawing requests do not scan the layer collection, staged and eager execution are distinguishable in results, and repeated measurements identify the remaining dominant costs.

### Phase 3: Introduce immutable resource packets

Current outcome: reusable TypeScript evaluation is enabled in both the browser
app and the library's normal `IMMAsset.update()` path. Returned asset snapshots
are borrowed until the next update. A sustained loaded animated-scene comparison
found lower update CPU cost, but both modes remained near 60 fps. This does not
establish a substantial user-visible playback improvement or complete Phase 3.
No native/Wasm timeline migration has been implemented. The following earlier
experiment notes are historical; the reassessment below governs further work.

Latest decision, 2026-09-07: eighteen equal-work trials do not establish an
elapsed-time benefit from batching or pacing. Single requests remain the default;
experimental controls remain available. Full-workload tracing identifies scene
evaluation and GC as material costs, with the longest browser/native task still
unclassified. Other agents were active, so timing results remain exploratory.
See `code/projects/web/performance/WASM_REBALANCE_DELIVERY_EXPERIMENT.md`.
The next step is an allocation profile and compact evaluation prototype with
semantic comparisons, before assuming either batching or native timeline
migration will help. The earlier progress notes below are historical.

Progress 2026-09-07: the first slice adds bounded background worker batches and
shared, paced main-thread delivery for the app and library. Batches contain at
most eight resources and stop after an item reaches 4 ms or 2 MiB of payload.
Validation and adapter delivery yield between 4 ms slices. These are soft limits
for indivisible resources, not hard frame-time or memory guarantees. Initial
loading retains its existing request path.

Unit, compiled-Wasm, app build, and visible Chrome smoke checks pass. Exact
batched-versus-single staged geometry and picture/sound/spawn payload comparisons
pass on `sample1.imm`. The eager/staged geometry mismatch was traced to
uninitialized view directions on always-visible strokes. The shared importer now
initializes those omitted fields to zero, and exact eager/single-staged/batched
geometry comparisons pass after rebuilding Wasm. See
`code/projects/web/performance/WASM_REBALANCE_PHASE3_PROGRESS.md`.

Five medium and three upper-bound runs completed with no errors or fallback.
Medium responsiveness remains near 60 Hz. Upper mean frame time fell from
129.54 to 83.36 ms, but median background completion fell from 610 to 564 and
worst-long-task median rose from 3,334 to 11,232 ms. Chrome changed patch version,
and the trials were not interleaved; this is not an acceptance result.

Next: compare single-request and bounded delivery on the same browser with
interleaved trials and equivalent work accounting; isolate the long tasks before
accepting batching. Continue the resource identity, generation, release, and
residency audit below. Phase 3's exit condition is not met.

1. Audit how often unchanged geometry, picture, sound, and material resources are built, copied, transferred, and uploaded across loading, seeking, chapter changes, and playback.

2. Separate immutable resource packets from mutable frame state and make packet reuse the primary optimization target.

3. Add stable resource IDs, generations, explicit release operations, and a measured residency budget.

4. Update the Three.js adapter to upload each immutable resource once and reuse it until invalidated.

5. Aggregate or batch packets only where the Phase 2.1 measurements show per-packet overhead is material.

6. Verify disposal and residency under repeated load, seek, chapter change, and document replacement.

Exit condition: steady-state playback does not rebuild, copy, transfer, or upload unchanged resources, and measured residency remains within budget.

### Phase 4: Move timeline semantics behind a batched evaluation API

1. Before migration, record real layer counts, animation-key counts, and timeline/adapter frame cost. Proceed only for workloads where this cost is material.

2. Make the extracted native runtime evaluate a complete document at requested ticks through one batched call per document, not calls per layer or key.

3. Return only compact changed state records after the initial snapshot.

4. Keep the browser responsible for clock selection and pass explicit ticks into the runtime.

5. Keep audio scheduling in TypeScript while consuming the same evaluated state and action events.

6. Run native and TypeScript evaluators side by side in tests until all fixtures agree.

7. Retire duplicate TypeScript semantic evaluation only if layer-heavy workloads improve and small and normal workloads remain neutral or better after transfer costs.

Exit condition: native and web share timeline semantics without increasing normal-scene frame cost.

### Phase 5: Share shader algorithms and parity inputs

1. Extract blue-noise coverage, alpha-mask, colour conversion, draw-in, and directional-visibility equations into small shared/generated fragments where practical.

2. Preserve explicit WebGL capability branches for sample masks, alpha-to-coverage, and alpha hash.

3. Add deterministic CPU test vectors for coverage-mask calculations.

4. Add close-up native/web image comparisons for alpha edges and depth interaction.

5. Keep readable generated shader artifacts for browser debugging.

Exit condition: mathematical changes to shared coverage behavior require one authoritative edit, while platform pipeline differences remain explicit.

### Phase 6: Simplify and enforce ownership

1. Remove superseded JavaScript implementations and compatibility paths.

2. Add CI checks that regenerate shared assets and shader fragments and fail on uncommitted differences.

3. Add a feature checklist requiring every new native semantic field to declare its packet representation, browser handling, and parity coverage.

4. Update web and native architecture documentation to point to the authoritative module for each behavior.

Exit condition: the responsibility boundary is enforceable through code organization, generated artifacts, and CI rather than developer memory.

## 9. Feature-change workflow after migration

Every feature that affects IMM playback should be classified before implementation:

1. Format-only changes belong in the importer and require packet/schema exposure only if they add observable data.

2. Renderer-independent semantic changes belong in the shared native runtime and require native plus Wasm tests.

3. Geometry/topology changes belong in the shared tessellator and require native/web golden geometry tests.

4. GPU-algorithm changes should update shared shader fragments or conformance vectors where possible, plus backend-specific pipeline code.

5. Browser integration changes remain in TypeScript and should not alter native semantics.

6. Backend-specific features must declare whether the web implementation is supported, approximated, or unsupported and expose that status through diagnostics.

## 10. Risks and mitigations

| Risk | Mitigation |
|---|---|
| The Wasm ABI becomes another large maintenance surface | Keep operations coarse, packets versioned, and structs renderer-neutral; generate bindings where practical. |
| Wasm copies offset computation gains | Benchmark end-to-end, output final packed layouts, and avoid returning redundant canonical and expanded forms. |
| Peak Wasm memory grows for large drawings | Build and release per drawing, use staged loading, add explicit release operations, and measure peak pages. |
| Per-frame worker latency causes visible lag | Keep explicit timestamps, use compact dirty packets, evaluate ahead where safe, or retain main-thread evaluation when measurement favors it. |
| Native extraction destabilizes existing players | Make the native player adopt extracted modules first and preserve existing integration tests. |
| Shared shaders hide platform differences | Share algorithms selectively while keeping backend capability and pipeline branches explicit. |
| Three.js integration becomes constrained by native assumptions | Keep packets declarative and prevent shared C++ code from owning renderer, camera, frame loop, or GPU resources. |
| Small scenes regress from bridge overhead | Use batched calls and retain a benchmark gate that covers small documents, not only large stress fixtures. |

## 11. Non-goals

1. Compiling the complete native renderer, sound engine, VR layer, windowing system, or OS abstraction into Wasm.

2. Allowing the Wasm runtime to own the WebGL context or issue draw calls behind Three.js's back.

3. Replacing Three.js before profiling demonstrates a concrete limitation.

4. Moving browser clocks, Web Audio nodes, fetch, cache policy, DOM state, or WebXR session ownership into Wasm.

5. Adding Wasm pthreads, shared memory, WebGPU, or a new shader cross-compiler without measured need.

6. Assuming that a Wasm implementation is faster merely because it is compiled from C++.

## 12. Original first implementation slice (completed)

The first implementation should move only paint tessellation into a renderer-neutral shared C++ module and expose one batched drawing-build operation from the existing decoder worker.

This slice has the best balance of:

1. High semantic duplication today.

2. Work that scales with real point counts.

3. Existing worker isolation.

4. Clear golden-test inputs and outputs.

5. No requirement to change Three.js scene ownership or WebGL capability handling.

Timeline migration should follow only after the geometry results establish the actual costs of Wasm allocation, copying, and packet marshaling on the supported device classes.

## Machine-local workload lookup (not for public project documentation)

Private corpus root: `F:/Imm`. Exact paths for the medium and practical-upper-bound
workloads are retained in ignored `artifacts/rebalance-phase3-corpus-paths.json`.
Both files were verified on 2026-09-07 against the previous report sizes:
20,115,461 and 440,673,604 bytes. Consult this mapping and prior session commands
before asking the user for these paths again.
