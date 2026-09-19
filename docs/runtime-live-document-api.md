# Live document mutation contract (M0)

Status: specification for milestone M1 of `IMM_RUNTIME_ANIMATION_EDITOR_PLAN_V2.md`.
Nothing here is implemented yet; M1 implements it against this contract.

## 1. What is being mutated

The live document is **the runtime document the renderer already consumes**:

- `ImmPlayer::Document` (libImmPlayer) owns `ImmImporter::Sequence mSequence`.
- The CPU pipeline is `Document::UpdateStateCPU(...)` → `LayerRenderer*::LoadInCPU(log, layer)`.
- The GPU pipeline is `Document::UpdateStateGPU(...)` → `LayerRenderer*::LoadInGPU(renderer, sound, log, layer)`.
- Layer renderers are owned outside the document and passed in on every update, so edits go
  through the document, never by touching renderers directly.
- `LayerRendererPaintStatic` holds **one GPU chunk buffer per layer** (`piBuffer mChunkData`)
  plus a per-pass visible-layer array (`mVisibleLayerInfos`), and already exposes a
  per-drawing unload (`UnloadInGPU(..., unsigned int drawingID)`).

No new document representation is introduced. The IMM byte stream stops being part of the
interactive path and remains the persistence and cross-boundary format.

## 2. Object model and identity

| Object | Identity | Notes |
|---|---|---|
| Document | `docId`, existing player id | one live document per loaded/created document |
| Layer | stable `layerId` | survives edits; unique within the document |
| Drawing | stable `drawingId` | belongs to exactly one paint layer |
| Element (per drawing) | index within the drawing | brush section + point array |
| Animation key | (layerId, property, timeTicks) | replace-in-place on equal time |
| Spawn area | stable `spawnAreaId` | volume, transform, locomotion mask |

Ids are allocated by the live document, never reused within a session, and remain valid
across save/load so a saved file reloads with the same ids.

## 3. Mutation surface (C ABI)

New exports on `ImmUnityPlugin`, prefixed `ImmAuthoring_` to match the managed API they
serve. All return `int`: `0` success, negative error code (§7).

Document lifecycle:

| Function | Purpose |
|---|---|
| `ImmAuthoring_CreateEmpty(int* docIdOut)` | new empty live document in the player |
| `ImmAuthoring_Attach(int docId)` | start editing an already loaded document |
| `ImmAuthoring_Commit(int docId, uint64_t* revisionOut)` | publish pending edits to the renderer |
| `ImmAuthoring_GetRevision(int docId)` | current committed revision |
| `ImmAuthoring_SaveToFile/Memory(...)` | write the live document through the IMM writer (M3) |

Layers:

| Function | Purpose |
|---|---|
| `ImmAuthoring_LayerCreate(int docId, int type, int parentId, const Properties*, int siblingIndex, int* layerIdOut)` | paint, group, spawn area |
| `ImmAuthoring_LayerDestroy(int docId, int layerId)` | removes the layer and its drawings |
| `ImmAuthoring_LayerReparent(int docId, int layerId, int newParentId, int siblingIndex)` | reorder/reparent |
| `ImmAuthoring_LayerSetProperties(int docId, int layerId, const Properties*)` | name, visibility, opacity, transform, pivot, timing |

Drawings and geometry:

| Function | Purpose |
|---|---|
| `ImmAuthoring_DrawingCreate(int docId, int layerId, int* drawingIdOut)` | empty drawing |
| `ImmAuthoring_DrawingDestroy(int docId, int layerId, int drawingId)` | |
| `ImmAuthoring_DrawingSetGeometry(int docId, int layerId, int drawingId, const Geometry*)` | replace an element's brush section and points in one call |
| `ImmAuthoring_DrawingSetFrame(int docId, int layerId, int frameIndex, int drawingId)` | timeline mapping |

Geometry transfer is **borrowed, not copied**: the caller's arrays must stay valid until
`Commit` returns. One call per drawing, points in one batch — no per-point ABI calls in the
interactive path.

Implemented so far (M1a-M1c): `ImmAuthoring_Attach`, `ImmAuthoring_IsAttached`,
`ImmAuthoring_Commit`, `ImmAuthoring_DrawingSetGeometry`, `ImmAuthoring_DrawingAdd` and
`ImmAuthoring_FrameSet`. Appending a drawing keeps every existing index valid, so frame
mappings and GPU ids are untouched. Geometry replacement works on static paint drawings
(`DrawingStatic::ReplaceGeometry`); pretessellated drawings refuse, so the Android/GLES path
that uses them reports failure rather than silently ignoring the edit. The points are
quantised with the exporter's own helpers (`bits8` alpha, `bits15` width against
`biggestStroke`) so a live edit and an export of the same points describe the same pixels.

**Drawing destruction is not implemented, and two designs have now failed.** Erasing from
`mDrawings` was rejected on paper: it shifts the indices that both the frame buffer and the
renderer's GPU pool are addressed by. Emptying the drawing instead (clear its vertices,
indices and chunks, keep its index) was tried and **hung `appImmViewer`** in the same frame
as the other edits. The cause is not yet pinned down — a drawing with zero geometry chunks
going through the layer renderer's load path is the prime suspect, rather than the timing
that the deferred refresh already handles. The next attempt should keep the drawing's GPU
resources valid (a tombstone the renderer can still load, or an explicit per-drawing GPU
release) instead of clearing geometry underneath it.

**Mechanism findings, and a hypothesis that was wrong.** The working refresh is the
layer-wide pair (`UnloadInCPU` + `LoadInCPU`) plus the deferred, per-layer GPU re-upload:
that is what measures 0.061 ms + 0.004 ms on the 4,000-drawing document. Two alternatives
were tried and rejected with evidence:

- Replacing the layer's GPU buffers immediately (inside the same frame) **hung**
  `appImmViewer`. Deferring the destruction by the number of frames the renderer keeps in
  flight fixed it, and that is what the current code does.
- Re-initialising one drawing's existing draw-info slot in place, to avoid the layer-wide
  `LoadInCPU` (which allocates a fresh slot per drawing), **crashed** the viewer with an
  access violation. `iSLayerDrawInfoStatic::Init` is therefore not a safe re-init on a live
  slot. The entry point stays in the renderer interface, unused, so the next attempt starts
  from the crash rather than from the guess — the per-drawing route needs the slot's own
  release path (whatever `UnloadInCPU` does per drawing), not `Init` alone.

So the earlier claim that the layer-wide load caused the M1b hang was wrong: that hang was
the immediate GPU destruction, and the layer-wide CPU load is what still works.

**The per-drawing release path, read rather than guessed (round 10).**
`LayerRendererPaintStatic::UnloadInCPU` releases one drawing's slot with exactly two calls:

```cpp
const Drawing * dr = lp->GetDrawing(j);
const int id = dr->GetGpuId();
if (id == -1) return;
iSLayerDrawInfoStatic * me = (iSLayerDrawInfoStatic *)mLayerInfo.GetAddress(id);
me->End();
mLayerInfo.Free(id);
```

Two things follow. First, the safe per-drawing refresh is *release then re-allocate one
slot* — `End()` + `Free(id)`, then `mLayerInfo.Alloc(&isNew, &id, true)`, placement-new,
`Init(dr)`, and the drawing's gpu id updated — not `Init(dr)` on a live slot, which is what
crashed. Second, `UnloadInCPU` uses `return` rather than `continue` when it meets a drawing
with gpu id -1, so it abandons every later drawing in the layer. That is harmless for a
document loaded in full (every drawing has an id) but wrong the moment an editor adds or
empties a drawing, which is precisely the case this work introduces.

Animation and spawn areas:

| Function | Purpose |
|---|---|
| `ImmAuthoring_KeySet(int docId, int layerId, int property, int64_t timeTicks, const Value*, int interpolation)` | insert or replace |
| `ImmAuthoring_KeyRemove(int docId, int layerId, int property, int64_t timeTicks)` | |
| `ImmAuthoring_SpawnAreaSet(int docId, int layerId, const Volume*, const Transform*)` | volume and locomotion on a spawn-area layer |
| `ImmAuthoring_SetInitialSpawnArea(int docId, int layerId)` | document default viewpoint |

Query functions mirror the mutation set for round-trip tests and for the managed layer to
read back what the renderer holds. They are deliberately not listed here in full; the rule
is that anything mutable is readable.

## 4. Dirty model

Every mutation accumulates dirty marks; `Commit` applies them in one pass on the render
thread. The contract is what each dirty unit costs:

| Dirty unit | Regenerated on commit | Cost scales with |
|---|---|---|
| `Geometry(layerId)` | CPU geometry for that layer → GPU chunk buffer for that layer | that layer's drawings |
| `Properties(layerId)` | nothing geometric; visibility/opacity/transform taken from the live layer at draw time | O(1) |
| `Timeline` | frame-index and timeline caches | frame count |
| `SpawnAreas` | spawn-area list and pose caches | spawn-area count |
| `Bounds(layerId)` | layer and document bounding boxes | that layer's drawings |

Rules:

1. A geometry edit never rebuilds another layer, and never re-parses or re-encodes anything.
2. A property-only edit performs no geometry work at all (§2 budget: under 2 ms).
3. The document-wide rebuild (today's behaviour) is reachable only through explicit
   operations: load, import, save.
4. Per-layer GPU re-packing is the baseline, because the renderer holds one chunk buffer per
   layer. Per-drawing slices inside that buffer are an optimisation to be justified by
   measurement, not assumed.
5. A GPU refresh is deferred by three frames after the CPU rebuild. Destroying a layer's
   buffers in the same frame that still references them hangs the renderer — measured, not
   assumed: an immediate per-layer refresh hung `appImmViewer` until the refresh was delayed.
   Three frames is the number of frames the renderer keeps in flight.

## 5. Threading and commit semantics

- Mutations are queued and applied on the render thread, between the CPU and GPU update
  passes, extending the existing `Document::Command` mechanism rather than adding a second
  queue.
- The renderer observes only committed state. A commit is atomic: either every dirty unit in
  that commit is applied, or none is and the previous state stays intact.
- Revisions increase monotonically per document. A commit returns the new revision; callers
  can compare revisions to detect supersession in the same way the current preview
  coordinator discards stale results.
- Pending, uncommitted edits are visible to the editing API (read-back returns the edited
  state) and invisible to the renderer.
- The editing API is not re-entrant from render callbacks.

## 6. Ownership and lifetimes

- The live document owns all geometry it accepts: at `Commit` it either takes ownership of
  the caller's buffer or copies it. Copy-on-commit is the default; a caller may hand over
  ownership explicitly to avoid the copy, and the ABI documents which.
- Destroying a layer or drawing releases its CPU and GPU resources at the next commit, not
  immediately, so the renderer never sees a hole.
- Handles (ids) stay valid until the object is destroyed; destroy is idempotent-safe (a
  second destroy returns `NotFound`).

## 7. Errors

| Code | Meaning |
|---|---|
| `0` | success |
| `-1` | not attached / not found (document, layer, drawing) |
| `-2` | invalid argument (empty name, non-finite transform, bad counts) |
| `-3` | unsupported (point brush sections, unknown layer type, picture/model geometry) |
| `-4` | state (edit attempted from a render callback, document unloading) |
| `-5` | out of memory |
| `-6` | rejected by validation (geometry too large, id collision) |

Errors leave the document unchanged. Validation is per-unit: no whole-document walk on the
interactive path.

## 8. Explicit non-goals for M1

- No undo/redo (application-level, using managed snapshots).
- No multi-document editing session.
- No changes to the IMM file format.
- No picture, model or sound mutation; paint layers, groups and spawn areas only.
- No per-point ABI calls: geometry arrives one drawing at a time.

## 9. Measurement hooks

`code/appImmUnity/tests/exporter_benchmark.py` gains edit cases so M1 and M2 are accepted
against the plan's budget on the existing corpus (Small 10 / Medium 400 / Large 4,000
drawings):

| Case | Measures |
|---|---|
| `edit-property` | visibility/opacity/transform change on one layer |
| `edit-geometry-small` | replace one drawing's points (one stroke) |
| `edit-geometry-large` | replace one drawing's points (64 points, batch transfer) |
| `edit-add-drawing` | add a drawing with one stroke to an existing layer |
| `edit-destroy-drawing` | remove one drawing |
| `edit-timeline` | remap one frame |
| `edit-document` | current baseline: full compile-and-reload, kept as the comparison |

Each case reports median and p95 wall time from the commit call to the next completed
render-thread update, so the number includes queueing and regeneration, not just the API
call. The `edit-document` row is what the live path has to beat: 535 ms for Large today.

## 10. Regression safety

The player's existing behaviour is not allowed to change. The live path is additive, and the
following are acceptance conditions for every milestone, not aspirations:

1. **Opt-in only.** A document that was not explicitly attached for editing runs today's
   code path unchanged: same load, same update, same draw. Nothing in the editing work may
   alter the behaviour of `Document::UpdateStateCPU/GPU`, the layer renderers, or the
   importers for documents that are simply playing. If a change cannot be made additive,
   it is gated behind the attach flag.
2. **Frozen surfaces.** The IMM file format, the existing plugin exports and their
   signatures, and the existing managed public API do not change. New ABI is additive
   (`ImmAuthoring_*`); `tests/tools/verify_unity_plugin_exports.py` fails CI if a declared
   entry point stops being exported.
3. **Format and load invariance.** Exporting the same content must still produce the same
   bytes, and loading existing files must still produce the same document: layer, drawing,
   stroke, point, key and bounds counts, plus a content hash over point data, compared
   against recorded baselines (`tests/baselines/`, `verify_imm_baseline.py`).
4. **Render invariance.** The CI render-metric comparisons against the committed baselines
   stay green, and `appImmViewer` validation captures are compared before and after each
   milestone on the same corpus.
5. **One command for the whole set.** `tests/tools/run_player_regressions.py` runs the
   player-facing regression set (bridge smoke, content baseline, export verifier, C#
   compile check, stroke-reader adapter) and exits non-zero on any failure. Every milestone
   records its output before and after the change.
6. **Fallback kept.** Per-layer GPU re-packing is the baseline; if per-drawing buffer
   updates are attempted later, the document-rebuild path stays available and selectable,
   so a platform that regresses can be switched back without a code change.

## 11. Milestone acceptance for M1

M1 is accepted when, in addition to the latency budget:

- Every regression check in §10 passes unchanged.
- The `edit-document` baseline is still reproduced exactly when the live path is not
  attached, proving the old path is intact.
- The new edit cases are measured and recorded for Small, Medium and Large.
