# Wasm runtime rebalance Phase 0–2 audit

Audit date: 2026-08-30.

## Phase 0: freeze and characterize

1. **Contract — pass.** `decoder/CONTRACT.md` records the schema-v5 C ABI,
   worker/document layout, geometry invariants, packet schema, alignment,
   endianness, ownership, and lifetime.
2. **Version and malformed data — pass.** Main-thread schema/geometry tests and
   the standalone packet parser tests reject schema mismatches, truncated or
   misaligned ranges, inconsistent identities/counts, duplicate brushes, and
   invalid indices.
3. **Performance corpus — pass.** Raw before/after evidence covers the committed
   6 KB small fixture, committed 5.8 MB compatibility fixture, a private 20 MB
   medium animated and paint-heavy fixture, and a private 440 MB practical
   upper-bound fixture. Reports include load, worker, upload, long-task, frame,
   Wasm/JavaScript memory, buffer/GPU estimates, and per-point/triangle metrics.
4. **Geometry goldens — pass.** Tests cover all five brushes, duplicate
   endpoints, 16/32-bit index selection, large batches, draw-in progress,
   directional visibility, masks, flipped winding, and browser-rendered flipped
   transforms.
5. **Timeline fixtures — pass.** Tests cover visibility, offset, opacity,
   transform/pivot, draw-in time, loop override, every interpolation mode,
   play/stop/loop/make-default actions, waits, chapter changes, hierarchy/world
   propagation, one-shot and looping drawing selection, and deterministic seek.

Phase 0 exit condition is met: correctness and performance baselines are
deterministic and recorded.

## Phase 1: renderer-neutral native core

1. **Coupled logic identified — pass.** Brush cross-sections, tangent/basis
   construction, vertex placement, and triangle topology were isolated from
   importer/player classes.
2. **Renderer-neutral extraction — pass.** `paintGeometry.h/.cpp` depends only on
   importer point/brush types and core math; it owns no GPU, audio, window, VR,
   browser, or OS objects.
3. **Native adoption first — pass.** Native `Element` tangent/basis methods and
   `DrawingPretessellated` section construction delegate to the shared module.
   The Wasm packet builder consumes the same functions.
4. **Native unit tests — pass.** The native test covers all section counts,
   duplicate-endpoint tangent behavior, vertex placement, triangle topology,
   and flipped winding. Its executable passes.

Phase 1 exit condition is met: the refactor preserves the native formulas and
the shared module has no renderer dependency.

## Phase 2: paint tessellation in the Wasm worker

1. **Coarse WebGL-ready API — pass.** One drawing-build call emits a versioned,
   contiguous packet containing final float attributes and 16/32-bit triangle
   indices.
2. **Staged loading — pass.** Drawing decode/build remains keyed by requested
   layer and drawing IDs. Load-order, initial-window, repeated-load, and browser
   integration tests pass.
3. **Transfer and release — pass.** The worker copies each non-transferable Wasm
   packet once, exposes typed views over that buffer, transfers it once, and
   releases the Wasm packet in `finally`. Scene release also clears it.
4. **Oracle/fallback validation — pass.** The JavaScript tessellator was retained
   for side-by-side validation through all corpus and real-drawing parity runs.
5. **Geometry parity — pass.** Real `sample1.imm` drawings matched the JavaScript
   oracle for metadata, all attributes, and indices; deterministic native and
   JavaScript goldens cover exact topology and bounded float differences. Browser
   visual tests pass for winding, depth, coverage, and ordering.
6. **Production oracle removal — pass.** After the performance, memory, and
   parity gates passed, the JavaScript tessellator import and fallback were
   removed from the production worker and build artifacts. Its source remains
   test-only.

Phase 2 exit condition is met: the native player and web packet builder consume
the shared brush algorithms, while normal and representative large loads show
no material regression.

## Final verification

1. Native shared-geometry executable: pass.
2. Wasm production-worker smoke: pass with 1,171 strokes, 58,405 points,
   798,922 paint triangles, and three encoded sounds.
3. Bun unit suite: 39 passed, 0 failed.
4. TypeScript and production Vite build: pass.
5. Complete Chrome browser suite on port 4179: pass, including standalone,
   embedded, staged/repeated loading, desktop/mobile, timeline, audio, pictures,
   coverage, flipped paint, depth, and disposal checks.
6. `git diff --check`: pass; only pre-existing line-ending warnings remain.
