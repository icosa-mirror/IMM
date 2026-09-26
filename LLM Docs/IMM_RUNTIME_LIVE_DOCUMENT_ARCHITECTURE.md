# IMM runtime live-document architecture and C ABI

Status: target contract for M1 and the foundation for later editor milestones.

This document is normative for implementation details. The companion
[IMM_RUNTIME_ANIMATION_EDITOR_ROADMAP.md](IMM_RUNTIME_ANIMATION_EDITOR_ROADMAP.md) owns the
motivation, latency budgets, milestone sequence, risks, and stop conditions. If the two
documents conflict on identity, batching, commit semantics, renderer lifetime, ABI, threading,
ownership, errors, or acceptance behaviour, this document wins.

This document separates the intended architecture from the current prototype. The prototype
has established that live geometry replacement is possible, but it does not yet implement the
transaction, identity, or renderer-lifetime guarantees below. Section 13 records that current
state so experimental code is not mistaken for the contract.

## 1. Goals and boundaries

The live document is the runtime document the player already consumes:

1. `ImmPlayer::Document` owns `ImmImporter::Sequence mSequence`.
2. The CPU pipeline is `Document::UpdateStateCPU(...)` to
   `LayerRenderer*::LoadInCPU(log, layer)`.
3. The GPU pipeline is `Document::UpdateStateGPU(...)` to
   `LayerRenderer*::LoadInGPU(renderer, sound, log, layer)`.
4. Layer renderers remain owned outside the document and are supplied on update. Authoring
   code changes the document; it does not mutate renderer internals directly.
5. The IMM byte stream remains the persistence and interchange format. It is not used to
   shuttle each interactive edit through an export-and-reload cycle.

The architecture has four non-negotiable properties:

1. A renderer sees one complete committed revision at a time.
2. Public object handles do not depend on vector positions, renderer slots, or GPU offsets.
3. Old rendering resources remain valid until the renderer confirms that they are no longer
   in flight.
4. Existing load and playback behaviour is unchanged unless authoring is explicitly attached.

The implementation may add edit batches, handle maps, and replacement renderer resources.
Those are control structures around the existing document model, not a second authoring file
format or a second long-lived document representation.

## 2. Identity and storage

| Object | Public identity | Internal storage |
|---|---|---|
| Document | existing player `docId` | player document slot |
| Edit batch | implicit while open; requested revision once sealed | pending command batch |
| Layer | 64-bit `layerId` handle | existing layer pointer plus handle-map entry |
| Drawing | 64-bit `drawingId` handle | drawing slot/index plus handle-map entry |
| Element | index within a drawing | drawing-owned element array |
| Animation key | `(layerId, property, timeTicks)` | property timeline entry |
| Spawn area | its layer's `layerId` | spawn-area layer implementation |

Identity rules:

1. Layer and drawing handles are stable for the lifetime of the loaded document.
2. Handles are allocated monotonically and are never reused in that document session, even
   when creation is rolled back or an object is destroyed.
3. A handle is not a drawing index, frame-buffer value, `mLayerInfo` slot, pointer, or GPU id.
   Handle maps resolve public identity to current internal storage.
4. A created handle may be referenced by later operations in the same edit batch. It becomes
   committed only when that batch succeeds. If the batch fails, the reserved handle remains
   invalid and is not reused.
5. Destroyed handles return `NotFound` after the destroying revision is presented.
6. M1 handles are session-only. They are not promised to survive save and reload: the current
   IMM format does not persist drawing identities, and layer IDs can be affected by creation
   order. Persistent IDs require a separately approved, backward-compatible format extension
   or sidecar and are outside M1.
7. On `Attach`, existing layers and drawings receive handles before the first batch opens.
   Existing layer handles may preserve `Layer::GetID()` values where they are unique; drawing
   handles are recorded independently even if their initial values match drawing indices.
   Newly allocated handles start above every imported handle.

The distinction between handles and storage indices makes deletion and compaction possible.
Frame buffers may continue to store drawing indices internally, but authoring calls use
`drawingId`; the commit preparation step resolves handles and rewrites affected frame indices.

## 3. Editing session and batch model

Each attached document has at most one open batch and a bounded number of sealed commits
awaiting completion. M1 does not support concurrent writers to one document.

The lifecycle is:

1. `Attach` enables authoring and creates an empty open batch.
2. Mutation calls validate their ABI inputs, copy all caller-owned payloads into the open
   batch, and return without touching the active document or renderer state.
3. Queries read the last presented revision by default. A separate pending-query flag may be
   added later; M1 does not synthesize readback by overlaying an uncommitted batch.
4. `Commit` seals the open batch, assigns a requested revision, queues one authoring command,
   and immediately opens the next empty batch.
5. The render thread prepares, validates, and publishes the sealed batch at its defined update
   boundary.
6. The caller polls commit status, or consumes the existing application-level completion
   mechanism, to learn whether the requested revision was presented or rejected.

`Commit` is asynchronous. A successful return means that the batch was accepted into the
queue, not that it is already visible. This avoids blocking the managed or render thread and
keeps the ABI usable when both are the same thread.

An empty batch returns `InvalidState`. If the commit queue is full, `Commit` returns
`QueueFull`, does not assign a revision, and leaves the current batch open. Operations in a new
batch may refer to handles reserved by an unresolved earlier batch; this creates an explicit
revision dependency and is why rejected revisions can cause `DependencyFailed` later.

The document tracks three revision values:

| Revision | Meaning |
|---|---|
| requested | assigned when a sealed batch is queued |
| prepared | document and renderer replacements were built successfully |
| presented | atomically visible to playback and rendering |

Revisions increase monotonically. A rejected revision is never reused. Later batches that
depend on handles created by a rejected revision are rejected with `DependencyFailed`.
Commit status remains queryable until the document is detached or unloaded.

## 4. Atomic commit preparation

A commit is prepared without modifying the active revision:

1. Resolve every referenced handle against the active revision plus objects created earlier
   in the same batch.
2. Perform whole-batch semantic validation, including hierarchy cycles, frame references,
   layer types, finite values, geometry limits, and allocation-size limits.
3. Build replacement model objects for every changed ownership unit. For M1 that unit is an
   affected layer plus document-level tables such as spawn areas. Unchanged objects are shared
   or referenced read-only.
4. Recalculate dirty bounds, timeline tables, and other derived CPU state into the replacement
   revision.
5. Build and upload replacement renderer resources while the old revision remains active.
6. If every step succeeds, swap the active model references and active renderer-resource
   references together at the presentation boundary.
7. If any step fails, discard the replacements and leave the active revision untouched.

This is copy-on-write at the affected-layer level, not a full-document clone. It is also why
calling `UnloadInCPU` on the active layer followed later by `LoadInGPU` cannot be the final
commit mechanism: that exposes a partially transitioned revision.

For the initial implementation, an authoring commit may rebuild every drawing in each dirty
layer. Per-drawing preparation is an optimisation and must preserve the same revision-swap
semantics.

## 5. Renderer resource lifetime

Each renderable layer revision owns a resource bundle containing all CPU draw information and
GPU buffers needed to draw that revision. A replacement bundle is prepared alongside the old
bundle rather than in its storage.

Publication and retirement follow this sequence:

1. Build replacement CPU draw information in unused slots or a separate layer bundle.
2. Create and upload replacement GPU buffers.
3. At a renderer update boundary, switch the layer's active bundle from old to new.
4. Submit the frame that first uses the new bundle.
5. Give the old bundle to a renderer-owned retirement queue tagged with the relevant
   submission serial or fence.
6. Destroy the old CPU and GPU resources only after the renderer reports that the last frame
   which could reference them has completed.

The document must not hard-code a delay such as three frames. If a backend does not expose
fences, its renderer implementation may translate retirement into its configured maximum
frames-in-flight, but that policy belongs to the renderer and must be correct for that backend.

A failed replacement upload leaves the old bundle active. It fails the commit rather than
logging an error and presenting a document whose renderer resources are incomplete.

## 6. Mutation surface (C ABI)

New exports on `ImmUnityPlugin` use the `ImmAuthoring_` prefix. Every function returns an
`ImmAuthoringResult`; values less than zero are errors.

### 6.1 ABI structure rules

1. Every non-trivial input/output structure starts with `uint32_t structSize` and
   `uint32_t structVersion`.
2. The plugin accepts known prefixes of older structures and rejects unsupported versions.
3. ABI structures contain fixed-width integers and pointers only; no C++ containers, `bool`,
   references, or compiler-dependent enums cross the boundary.
4. Mutation calls copy pointer-backed arrays before returning. Caller memory therefore needs
   to remain valid only for the duration of the call, not until `Commit`.
5. Counts and byte-size multiplications are checked for overflow before allocation.
6. Strings are UTF-8 byte spans with explicit lengths and need not be null terminated.

### 6.2 Session and commit functions

| Function | Purpose |
|---|---|
| `ImmAuthoring_Attach(int32_t docId)` | enable authoring for a loaded document |
| `ImmAuthoring_Detach(int32_t docId)` | reject new edits and release empty pending state |
| `ImmAuthoring_IsAttached(int32_t docId, int32_t* attachedOut)` | query attachment |
| `ImmAuthoring_DiscardPending(int32_t docId)` | discard the current unsealed batch |
| `ImmAuthoring_Commit(int32_t docId, uint64_t* requestedRevisionOut)` | seal and queue the current batch |
| `ImmAuthoring_GetRevisions(int32_t docId, ImmAuthoringRevisions* out)` | get requested, prepared, and presented revisions |
| `ImmAuthoring_GetCommitStatus(int32_t docId, uint64_t revision, ImmAuthoringCommitStatus* out)` | get queued/preparing/presented/rejected state and failure code |

Detaching is rejected while sealed commits remain unresolved unless an explicit cancel mode is
added later. Unloading a document cancels unresolved commits, waits for renderer-owned resource
retirement as the normal unload path already requires, and invalidates all handles.

### 6.3 Layers

| Function | Purpose |
|---|---|
| `ImmAuthoring_LayerCreate(...)` | reserve a layer handle and create content in the open batch; the current vertical slice supports groups |
| `ImmAuthoring_LayerDestroy(docId, layerId)` | destroy a layer subtree after reference validation; the current slice accepts non-timeline, group-only subtrees |
| `ImmAuthoring_LayerReparent(...)` | reorder or reparent while preventing cycles and preparing moved-subtree full names |
| `ImmAuthoring_LayerGetProperties(...)` | read canonical visibility, opacity, and transform without playback overrides |
| `ImmAuthoring_LayerSetProperties(...)` | change canonical name, visibility, opacity, transform, pivot, or timing |

Document authoring properties are distinct from playback overrides such as
`Player::SetLayerVisible` and `SetLayerTransform`. Authoring changes the canonical value that
is queried and eventually saved; playback overrides remain transient and retain their existing
API and behaviour. The application must clear or deliberately preserve an override when an
authored base value changes; the authoring API does not silently reinterpret one as the other.

### 6.4 Drawings and geometry

| Function | Purpose |
|---|---|
| `ImmAuthoring_DrawingCreate(docId, layerId, drawingIdOut)` | reserve an empty drawing in the open batch |
| `ImmAuthoring_DrawingDestroy(docId, layerId, drawingId)` | remove a drawing if the resulting batch leaves no frame referencing it |
| `ImmAuthoring_DrawingSetGeometry(...)` | replace all elements and points in one drawing |
| `ImmAuthoring_FrameGetDrawingHandle(docId, layerId, frameIndex, drawingIdOut)` | resolve the frame's current drawing to its stable handle |
| `ImmAuthoring_FrameSetHandle(docId, layerId, frameIndex, drawingId)` | map a frame to a drawing handle; the suffix preserves the legacy index-based export's ABI |

Geometry is supplied one drawing at a time, with an array of element descriptors and contiguous
or independently described point spans. M1 supports static paint drawings. Point brush
sections and pretessellated drawing mutation return `Unsupported`.

The version-1 geometry ABI uses `ImmAuthoringDrawingGeometry` as the drawing descriptor and
`ImmAuthoringElementGeometry` for each stroke element. Both structures carry `structSize` and
`structVersion`; each element points to an `ImmAuthoringPoint` span. The call copies every
descriptor and point before returning. Version 1 permits 1 through 65,536 elements, 2 through
8,192 points per element, and at most 1,048,576 points in one drawing replacement. Reserved
fields must be zero. Brush, visibility, color-space, flip, finite-value, alpha, width, and
aggregate-count validation happens before the batch is accepted. A non-positive
`biggestStroke` requests computation from the submitted widths; a positive value smaller than
an included point width is rejected rather than silently quantising that width out of range.

The owned open batch stores compact point vectors rather than importer `Element` objects.
`Element` contains a fixed 8,192-point block, so retaining one per submitted element would
consume roughly 512 KiB per element regardless of actual stroke length. During commit
preparation the runtime constructs one temporary `Element` at a time and streams it through
`DrawingStatic::StartAdding`, `Add`, and `StopAdding`. This keeps the fixed-size importer
representation out of the public ABI and prevents short multi-stroke edits from scaling as
the maximum importer capacity.

Drawing destruction uses the handle map rather than erasing an exposed index:

1. Validation considers all `FrameSet` operations in the same batch.
2. The batch is rejected if any resulting frame still references the destroyed drawing.
3. The new layer revision omits the drawing and rewrites internal frame indices.
4. The old drawing and its renderer resources remain owned by the old revision until that
   revision retires.
5. The destroyed handle is never reused.

The live model currently stores quantised paint attributes. `DrawingSetGeometry` therefore
quantises with the same helpers used by export (`bits8` alpha and `bits15` width against
`biggestStroke`). This contract preserves rendered and exported fidelity, not arbitrary
floating-point source precision. If the editor needs lossless source-authoring precision, that
requires a separate product decision and storage design rather than silently changing M1.

### 6.5 Animation and spawn areas

| Function | Purpose |
|---|---|
| `ImmAuthoring_KeySet(...)` | insert or replace a key at `(layer, property, time)` |
| `ImmAuthoring_KeyRemove(...)` | remove that key |
| `ImmAuthoring_SpawnAreaSet(...)` | set canonical volume, transform, and locomotion mask |
| `ImmAuthoring_SpawnAreaGet(...)` | read canonical volume, transform, tracking level, and locomotion mask |
| `ImmAuthoring_SetInitialSpawnArea(...)` | set the document default spawn-area handle |

Mutation validation rejects non-finite values, invalid interpolation modes, unsupported
property/value combinations, and references that will not exist in the resulting batch.

## 7. Dirty model and rebuild granularity

Dirty marks describe what must be prepared; they do not authorize in-place changes to active
renderer resources.

| Dirty unit | Prepared work | Baseline cost scales with |
|---|---|---|
| `DrawingGeometry(layerId, drawingId)` | replacement drawing CPU data; may escalate to layer bundle | dirty layer's drawings |
| `LayerTopology(layerId)` | drawing/frame index map and replacement layer bundle | dirty layer's drawings and frames |
| `LayerProperties(layerId)` | canonical property snapshot; bounds if transform/pivot requires it | O(1), plus bounds work when required |
| `Timeline(layerId)` | timeline evaluation tables | affected keys/frames |
| `SpawnAreas` | spawn-area list and pose cache | spawn-area count |
| `Bounds(layerId)` | layer bounds and document bounds contribution | dirty layer's drawings |

Rules:

1. A geometry edit never rebuilds an unrelated layer.
2. Property-only commits perform no paint tessellation or GPU geometry upload.
3. Frame remapping does not imply geometry regeneration unless the current renderer bundle
   representation requires it; that escalation must be measured and recorded.
4. Per-layer rebuilding is the correctness baseline because the static renderer currently
   owns one chunk buffer per layer.
5. Per-drawing CPU or GPU updates are accepted only if they use the same replacement-and-swap
   lifetime model and demonstrate a user-visible or measured latency benefit.
6. Load, import, and save may use document-wide processing. Interactive commits may not route
   through IMM encoding and reparsing.

## 8. Threading and ordering

1. Public authoring calls are serialized by the player mutex only long enough to validate and
   copy commands into the open batch.
2. Sealed batches enter the existing document command stream as one authoring command per
   commit. A second independent render-thread queue is not introduced.
3. Preparation and presentation occur on the render thread at documented boundaries between
   document update phases.
4. Renderer resource creation and retirement obey the backend's thread-affinity rules.
5. Authoring calls are not re-entrant from renderer callbacks.
6. Commits for one document are prepared and presented in requested-revision order. A failed
   commit does not prevent a later independent commit, but dependency failures are explicit.
7. Unload wins over queued authoring: unresolved commits become `Cancelled`, no new revision
   is presented, and their owned payloads are released.

## 9. Ownership and failure behaviour

1. The plugin owns copied batch payloads after a mutation call succeeds.
2. Prepared model replacements are owned by the sealed commit until presentation, then by the
   active document revision.
3. Prepared renderer bundles are owned by the sealed commit until presentation, then by the
   renderer and eventually its retirement queue.
4. Failed or cancelled batches release all unpublished model and renderer resources.
5. No failure may leave part of a batch visible or leak a reserved renderer slot.
6. Save reads the latest presented revision. It either rejects while newer commits are
   unresolved or is passed an explicit presented revision; it never serializes half a batch.

## 10. Errors

| Code | Name | Meaning |
|---|---|---|
| `0` | `Ok` | call succeeded |
| `-1` | `NotFound` | document or committed handle does not exist |
| `-2` | `InvalidArgument` | malformed structure, invalid count, non-finite value, or bad range |
| `-3` | `Unsupported` | unsupported layer, brush, geometry, property, or ABI version |
| `-4` | `InvalidState` | not attached, unloading, re-entrant call, or no open batch |
| `-5` | `OutOfMemory` | allocation failed |
| `-6` | `ValidationFailed` | batch would create an invalid hierarchy or dangling reference |
| `-7` | `QueueFull` | bounded commit queue cannot accept another sealed batch |
| `-8` | `DependencyFailed` | batch refers to an object from a rejected prior revision |
| `-9` | `Cancelled` | document unload or explicit cancellation prevented presentation |
| `-10` | `RendererFailed` | replacement renderer resources could not be prepared |

Mutation-call errors do not change the open batch. Asynchronous preparation errors are stored
in commit status with the failing command index and, where applicable, object handle. Human
readable diagnostics go to the normal log but are not the only way to identify failure.

## 11. Scope and non-goals for M1

1. No undo/redo; the application may express undo as a later edit batch.
2. No concurrent writers or multi-document transaction.
3. No persistent authoring handles across save/load.
4. No IMM format change.
5. No picture, model, or sound mutation.
6. No point-at-a-time ABI calls.
7. No requirement for lossless source precision beyond the existing IMM paint representation.
8. No per-drawing GPU suballocation requirement; correct per-layer replacement is sufficient.

## 12. Measurement and regression acceptance

`code/appImmUnity/tests/exporter_benchmark.py` should gain these edit cases on the existing
Small (10), Medium (400), and Large (4,000 drawing) corpus:

| Case | Measures |
|---|---|
| `edit-property` | canonical visibility/opacity/transform commit |
| `edit-geometry-small` | replace one short drawing |
| `edit-geometry-large` | replace one 64-point drawing |
| `edit-add-drawing` | add and frame-map one drawing |
| `edit-destroy-drawing` | remap frames and remove one drawing |
| `edit-timeline` | remap one frame without geometry changes |
| `edit-document` | current full compile-and-reload comparison |

Each case reports at least:

1. ABI call time used to validate and copy the batch.
2. Queue latency from `Commit` to preparation start.
3. CPU preparation time.
4. GPU preparation/upload time.
5. End-to-end time from `Commit` to presented revision.
6. Median and p95 over matched content and recorded artifact hashes.

Regression conditions:

1. A document not attached for editing follows the existing load, update, and draw path.
2. Existing plugin exports and managed APIs remain unchanged; new exports are additive and
   checked by `tests/tools/verify_unity_plugin_exports.py`.
3. Existing files load identically and unchanged content exports identically, checked by
   `tests/tools/verify_imm_baseline.py` and recorded baselines.
4. Existing render-metric comparisons remain green on the same corpus and backend.
5. `tests/tools/run_player_regressions.py` runs the player-facing regression set and fails on
   any component failure.
6. Failure injection for CPU allocation, GPU allocation/upload, unload during preparation,
   and invalid cross-reference proves that the old revision stays active and resources retire.
7. A visual or diagnostic test samples every frame during a commit and proves that no frame
   combines model revision N with renderer resources from revision N+1.

M1 is accepted only when the session/handle contract, batch status, atomic layer replacement,
property distinction, drawing deletion, and regression conditions above are implemented. A
prototype that mutates successfully but exposes intermediate state does not satisfy M1.

## 13. Implementation status and handoff

The repository now contains the first complete one-drawing replacement boundary:

1. `Attach` now assigns session-stable drawing handles for every imported paint drawing.
   `ImmAuthoring_DrawingGetHandle` is a transitional discovery function from
   `(layerId, importedDrawingIndex)` to `drawingId`; broader enumeration belongs in the query
   surface rather than making indices public identity.
2. `ImmAuthoring_DrawingSetGeometry` accepts a drawing handle plus versioned drawing and
   element descriptors, validates bounded multi-element point spans, and copies compact point
   data into an owned open batch. It does not mutate the drawing during the ABI call.
3. `Commit` seals a non-empty batch, assigns its requested revision, and queues
   `AuthoringCommit` in the existing per-document command slot. This first slice permits one
   geometry replacement and one unresolved commit at a time.
4. `ImmAuthoring_GetRevisions` and `ImmAuthoring_GetCommitStatus` expose requested, prepared,
   presented, and rejected progress. Status history is bounded.
5. The old `ImmAuthoring_DrawingAdd` and `ImmAuthoring_FrameSet` exports now return
   `Unsupported`; their index-based synchronous behavior contradicted the batch and handle
   contract. Their internal `Player` entry points and the corresponding standalone-viewer
   exercise were removed so there is no side door around the batch boundary.
6. CPU preparation builds a separate `DrawingStatic` and a separate static-renderer pool slot.
   GPU preparation uploads that slot while the active drawing and active slot remain unchanged.
7. Presentation swaps the drawing geometry and renderer-slot identity together under the
   player lock. The replacement object then contains the old geometry and is transferred to
   renderer-owned retirement storage.
8. Partial GPU upload failure now cleans up every resource created for the unpublished slot.
   Cancellation frees the replacement slot and model without changing the active pair.
9. The document-level dirty queues, active-slot refresh, layer unload/reload, and
   `kGpuRefreshDelayFrames` workaround have been removed from the authoring path.
10. The Windows `appImmUnity` and `appImmViewer` Release targets compile with this boundary in
    place.
11. The DirectX `sample1.imm` viewer probe presents a normal replacement as revision 1 with
    status `Presented`; its target drawing-local bounds change. Injected CPU preparation, GPU
    preparation, and pre-presentation failures all report status `Rejected` with
    `RendererFailed`, while the target drawing-local bounds remain bit-for-bit unchanged.
12. `ImmAuthoring_Detach` and `ImmAuthoring_DiscardPending` implement the remaining basic
    session controls. Detach rejects a non-empty open batch or unresolved commit; discard owns
    and releases copied open-batch payloads. `ImmAuthoring_IsAttached` now follows the common
    result-plus-output-pointer ABI convention.
13. `code/appImmUnity/src/imm_authoring.h` is the public C-compatible declaration source for
    result codes, commit states, versioned status structures, point layout, calling convention,
    and the implemented exports. `main.cpp` consumes the same declarations, so ABI drift is a
    compile failure rather than parallel undocumented definitions.
14. Open and sealed geometry batches no longer retain fixed-capacity importer `Element`
    objects. They own compact per-element point vectors; preparation constructs one temporary
    importer element at a time and streams all submitted elements into the replacement. The
    Release Unity plugin and viewer targets build, and the DirectX `sample1.imm` probe still
    presents revision 1 with status `Presented` and changed drawing-local bounds.
15. Each drawing now carries the authoring revision that supplied its active model geometry.
    With `IMM_LIVE_EDIT_TRACE_FRAMES=1`, static paint pre-render records that revision, the
    renderer token selected for the frame, and whether the renderer slot points at that exact
    model geometry. A delayed DirectX `sample1.imm` replacement sampled the target drawing 61
    times with no mismatch: samples 39-40 selected `(revision 0, token 0)` and samples 41-42
    selected `(revision 1, token 30)`. No sample combined an old model with the new renderer
    slot or vice versa.
16. The original hard-coded retirement delay was first moved into
    `Player::Configuration::maxFramesInFlight` as a transitional fallback. That fallback has
    now been removed: no host can silently inherit or guess an in-flight count. After the
    active model and renderer slot swap, the old CPU wrapper retires at the next render
    boundary. GPU-resource lifetime belongs to the backend: Vulkan queues buffer destruction
    against its fence-backed three-slot submission ring, Metal explicitly retains buffers
    encoded through a vertex array until the owning command buffer completes, and D3D/OpenGL
    use their native deferred resource-deletion semantics.
17. The delayed two-element replacement and frame-pair diagnostic also pass on Windows
    OpenGL: revision 1 reaches `Presented`, drawing-local bounds change, and all 61 sampled
    target-drawing frames report an exact model-geometry/renderer-slot match. DirectX and
    OpenGL therefore both show an old-or-new transition with no mixed frame.
18. The same delayed replacement now passes on standalone Windows Vulkan. Revision 1 reaches
    `Presented`, drawing-local bounds change, and all 61 target-drawing frame samples report
    matching model geometry and renderer slots. The earlier Vulkan attempt that stopped before
    GPU load was inconclusive; this completed run supersedes it for the standalone path.
19. Full validation run `35453794170` passed Windows, macOS, iOS, web, and core jobs but exposed
    Android Clang portability failures. The new virtual methods made
    `-Winconsistent-missing-override` diagnose older unannotated `DrawingStatic` and
    `LayerPaintStatic` overrides, and Android's `uint64_t` is not the same underlying C++ type
    as Windows `unsigned long long`. The overrides are now consistently annotated and all
    authoring export definitions use the header's exact fixed-width types. Local Android
    `libImmImporter`, `libImmPlayer`, and `appImmUnity` Debug builds pass after the correction.
20. Geometry queueing now preserves the result taxonomy through `Document`, `Player`, and the
    C ABI. A missing document or drawing returns `NotFound`, a detached session returns
    `InvalidState`, malformed internal input returns `InvalidArgument`, and an already occupied
    one-edit open batch returns `QueueFull`; these cases no longer collapse to `NotFound`.
    Attach, detach, discard, revision/status queries, and handle discovery follow the same
    missing-document versus detached-session distinction.
21. The public header now owns the geometry limits used by validation: 2-8192 points per
    element, at most 65536 elements, and at most 1048576 total points per drawing. Export return
    types are explicitly `int32_t`, and compile-time size/offset assertions protect every
    cross-library structure from accidental packing or field-order changes. Windows x64 and
    Android arm64 builds both pass with those checks enabled.
22. Loading or unloading document content now forcibly ends its authoring session before the
    sequence can be replaced or freed. Open and sealed batches, status history, and handle-map
    entries are released; an in-flight prepared replacement remains owned until the normal GPU
    unload path cancels and frees it. This prevents stale layer pointers and makes all session
    handles invalid at the lifecycle boundary. Detach also clears status history so a later
    attach cannot expose statuses from the previous session. Result `-9` is named `Cancelled`
    consistently with the error contract.
23. The two-element live-edit probe now lives in the shared standalone-viewer layer instead of
    the Windows-only entry point. The actual Apple Metal render loop invokes it for the
    dedicated macOS contract, which requires revision 1 to reach `Presented` with changed
    drawing bounds and rejects any sampled model/renderer geometry mismatch. Android FTL GLES
    and Vulkan validation invokes the same probe only after writing its unchanged baseline
    capture, then waits for the presented result and rejects frame-pair mismatches. Windows
    DirectX still passes after the extraction. Full validation run `35463143679` passed the
    macOS standalone Metal contract and the Android standalone GLES and Vulkan device jobs,
    together with every other executed build, core, web, iOS, engine, GPU, and evidence job.
    The macOS wiring is guarded separately from the shared iOS core because the macOS
    executable owns an independent render loop in `macos/metal_player.mm`.
24. The player-level `maxFramesInFlight` retirement guess has been removed. Static paint
    replacements retain their old model and renderer slot until the next render boundary,
    after which CPU wrappers are released and GPU handles enter the renderer backend's native
    lifetime mechanism. Metal now attaches unique vertex/index buffers used by the frame to
    the current command buffer's completion cleanup, including externally owned command
    buffers. Vulkan's existing deferred-destroy queue advances only as its fenced submission
    ring advances. Full validation run `35471831849` passed every executed build, core, web,
    iOS, engine, GPU, device, and evidence job with this ownership model, including standalone
    Metal, DirectX, OpenGL, Vulkan, Android GLES/Vulkan, Unity, and Godot coverage.
25. `ImmAuthoring_DrawingCreate` now reserves a monotonic drawing handle in the open batch.
    Geometry may target that reserved handle in the same batch; this first creation slice
    requires it to do so before commit. CPU geometry and a renderer slot are prepared without
    changing the active layer. Presentation appends the drawing, adopts the prepared renderer
    slot, and publishes the handle-map entry at one render boundary. Failure removes any
    unpublished append and releases the prepared slot. `LayerPaintStatic` now stores
    individually allocated drawings so growing its index vector cannot relocate existing
    drawing objects that the player or renderer still references. The shared standalone probe
    presents replacement revision 1 and creation revision 2; on local DirectX, OpenGL, and
    Vulkan the layer grows from one drawing to two and the appended index resolves to the
    reserved handle. Android native libraries, the viewer APK, and its instrumentation APK
    compile with the creation path. The new drawing remains intentionally unreferenced until
    frame mapping is implemented. Full hosted validation run `35499760852` passed every
    executed job for this slice.
26. `ImmAuthoring_FrameSetHandle` accepts a drawing handle rather than an exposed drawing
    index. The old index-based `ImmAuthoring_FrameSet` export remains ABI-compatible and
    returns `Unsupported`; it is not a side door around handle identity.
    A frame-only batch resolves an already committed handle. A creation batch may also map its
    reserved handle after supplying that drawing's geometry; item 30 records that extension.
    Preparation validates the layer, frame range, handle, and resolved drawing
    index without changing the active frame table. Presentation writes the one resolved index
    at the render boundary, advances the presented revision, and performs no tessellation or
    GPU upload. The shared probe maps frame zero to the drawing created in revision 2, presents
    that mapping as revision 3, and reads the frame back as the same stable handle. Windows
    Release viewer/plugin builds, DirectX/OpenGL/Vulkan runtime probes, Android
    native/app/instrumentation compilation, and the source contracts pass with this path.
27. `ImmAuthoring_DrawingDestroy` queues deletion by stable drawing handle. This initial
    deletion slice accepts one deletion-only batch and rejects a drawing while any frame still
    references its private storage index. Preparation repeats the reference and renderer-slot
    checks without changing the active document. At the render boundary, presentation extracts
    the drawing without moving any remaining drawing objects, compacts only the layer's private
    indices, removes the destroyed handle permanently, and transfers the old drawing and GPU
    slot to the renderer's existing backend-owned retirement path. Handles for surviving
    drawings remain unchanged. The shared probe first verifies that deletion is rejected while
    revision 3 references the new drawing, restores frame zero to its original handle in
    revision 4 and deletes the now-unreferenced drawing in that same atomic batch. It checks
    that the
    drawing count is restored, the deleted handle no longer resolves, and the original handle
    remains stable.
28. A frame remap followed by drawing deletion may now be sealed as one batch. The ordered
    form is deliberate: the remap establishes the proposed final frame table, after which
    deletion validation ignores only that remapped frame and still rejects any other live
    reference. Preparation resolves both handles and validates the final table without
    mutation. Presentation writes the remap and extracts the drawing under the same render
    boundary; if extraction unexpectedly fails, it restores the previous frame index before
    rejecting the commit. The probe still proves that deletion alone is rejected at revision
    3, then presents the restore-and-delete operation together as revision 4.
29. `ImmAuthoring_LayerSetProperties` introduces a versioned, mask-based property ABI. The
    first supported masks change canonical visibility, opacity, and a uniform-scale rigid
    transform, independently or together; unsupported masks are rejected. The C ABI uses
    translation, a unit quaternion, and positive uniform scale rather than exposing a C++
    matrix type. `Layer` stores these canonical values separately from animation-evaluated
    values, so timeline evaluation no longer destroys imported or authored base properties.
    Playback visibility, opacity, and transform remain explicit transient overrides. The
    command is copied into a property-only batch,
    resolved during preparation, and published at the render boundary without renderer
    resource work. Canonical visibility now has an explicit diagnostic getter distinct from
    effective visibility. The shared probe installs a playback visibility override, presents
    different canonical values as revision 5, and verifies that all three canonical values
    change while the overrides and effective runtime values remain unchanged.
30. Drawing creation, geometry supply, and frame mapping may now be committed as one batch in
    that order. `FrameSetHandle` resolves the reserved creation handle against the proposed
    append index instead of requiring it to exist in the committed handle map. Preparation
    validates the paint layer and frame range while building the drawing and renderer slot;
    presentation appends the drawing, publishes its stable handle, and writes the frame entry
    under the same render-boundary lock. A failed upload or append publishes neither object nor
    mapping. The shared probe now presents creation and frame assignment together as revision
    2, verifies both the appended handle and frame lookup, then atomically restores the frame
    and deletes the new drawing as revision 3. Canonical property mutation consequently moves
    to revision 4. The Windows Release plugin/viewer build and DirectX runtime probe pass this
    sequence.
31. `ImmAuthoring_FrameGetDrawingHandle` exposes the existing handle-based frame query through
    the public C ABI. It requires an attached session, validates signed arguments and the output
    pointer before conversion, and returns the stable drawing handle currently referenced by
    the committed frame table. This gives editors a symmetric query for `FrameSetHandle`
    without exposing the layer's private drawing index.
32. `ImmAuthoring_LayerGetProperties` provides the read side of the version-1 property ABI. It
    returns all supported-mask bits and canonical visibility, opacity, and transform values;
    playback-evaluated values and transient overrides are deliberately excluded. The caller
    supplies the versioned output structure, and reserved fields are normalized to zero.
33. Layer animation-key storage now uses private `std::vector` containers instead of the legacy
    manually allocated array. Existing insertion remains time-sorted and replacement-at-time
    retains its behavior, while allocation failure is now reported instead of dereferencing a
    null insertion result. `CopyAnimKeys` prepares an independent property timeline and
    `ReplaceAnimKeys` swaps it into the layer without allocation. This is the prerequisite for
    publishing a prepared key edit atomically at a render boundary.
34. `ImmAuthoring_KeySet` inserts or replaces one visibility, opacity, or transform key in a
    key-only batch. The version-1 ABI carries seconds, interpolation, and typed value fields;
    unsupported properties and malformed values are rejected before queueing. Visibility is
    stepped by the evaluator, so version 1 accepts only `NONE` interpolation for visibility keys.
    Preparation
    copies the affected property's sorted timeline and applies the edit to that copy.
    Presentation swaps the prepared vector into the layer without allocation or an observable
    intermediate state. The shared probe inserts a far-future visibility key as revision 5 and
    verifies that the committed key count increases without changing current playback state.
35. `ImmAuthoring_KeyRemove` removes one key identified by layer, property, and exact tick after
    converting the ABI's seconds value. Removal uses the same key-only batch and prepared-vector
    presentation path as insertion. A missing key rejects the asynchronous commit with
    `NotFound` and leaves the active timeline untouched. The shared probe removes its revision-5
    visibility key as revision 6 and verifies that the original key count is restored.
36. `ImmAuthoring_SetInitialSpawnArea` queues a stable layer ID as a spawn-area-only batch.
    Queueing rejects a missing layer or a non-spawn layer, and preparation resolves and
    revalidates the target without changing the active sequence. Presentation updates the
    sequence's initial-spawn pointer and marks the spawn-area cache dirty at one render
    boundary. `GetInitialSpawnAreaLayerId` supplies an internal diagnostic without exposing
    the player manager's private spawn-list index as an authoring handle. The shared probe
    presents this change as revision 7 and verifies that the canonical layer ID resolves after
    presentation; the local Windows DirectX run passes that sequence.
37. `ImmAuthoring_SpawnAreaSet` uses a versioned 112-byte C structure to update a sphere or
    axis-aligned box volume, floor/eye tracking level, per-axis locomotion mask, and the owning
    layer's uniform-scale rigid transform. Public validation rejects non-finite values,
    non-positive sphere radii, inverted box bounds, invalid enum or mask values, non-unit
    quaternions, and non-positive scale before queueing. Preparation copies the complete
    canonical spawn state and revalidates the layer type without mutating active state.
    Presentation applies the spawn implementation and layer transform together at the render
    boundary and marks the spawn-area cache dirty. The shared probe changes all affected state
    as revision 8 and verifies the resulting volume, tracking level, locomotion mask, and
    transform; the local Windows DirectX run passes.
38. `ImmAuthoring_SpawnAreaGet` is the symmetric read side of the same versioned structure.
    It requires an attached authoring session, resolves the layer by stable ID, and returns
    canonical spawn and transform values rather than the player manager's evaluated pose or
    private spawn-list index. Inactive sphere or box fields and every reserved field are
    normalized to zero so importer padding or inactive-shape storage cannot cross the ABI.
39. Layer initialization can now be detached from parent publication. The importer reserves
    parent-child and sequence-layer array capacity during preparation, then appends without
    allocation at presentation. Existing import calls retain immediate parent attachment by
    default. These primitives are deliberately separate from the public `LayerCreate` ABI:
    they establish the no-partial-hierarchy guarantee before stable creation handles are
    exposed to callers.
40. `ImmAuthoring_LayerCreate` now reserves a monotonic stable layer ID and accepts a bounded,
    explicitly sized UTF-8 name. The first supported type is `Group`; paint and spawn-area
    creation remain `Unsupported` until their implementation resources can be prepared under
    the same contract. Queueing validates the committed parent and requires a group parent.
    Preparation reserves both sequence and parent-child capacities, validates UTF-8 before the
    batch enters the player, and initializes a detached group. Presentation appends to the
    sequence table and parent without allocation; an unexpected parent publication failure
    rolls back the sequence append and rejects the revision. Discarded or failed handles are
    not reused. The shared probe creates a root child as revision 9 and verifies its stable ID,
    group type, parent ID, and the one-layer count increase; local Windows DirectX passes.
41. `ImmAuthoring_LayerDestroy` now removes one empty, non-root group in a deletion-only batch.
    Queueing and preparation reject the root, non-group layers, groups with children, and stale
    IDs. Presentation removes the sequence-table entry and parent-child entry at one render
    boundary, rolling the sequence entry back if the parent removal unexpectedly fails, then
    deinitializes and deletes the unreachable group. The stable layer ID is never reused.
    Subtree and resource-owning layer deletion remain unsupported until reference and renderer
    retirement rules cover every descendant. The shared probe deletes its revision-9 group as
    revision 13 and verifies that the subtree handles no longer resolve and the layer count is restored;
    local Windows DirectX passes.
42. The first `ImmAuthoring_LayerReparent` slice added allocation-free ordering changes within a
    layer's existing parent. Queueing and preparation revalidate the layer, parent, and final
    child index. Presentation removes and reinserts the existing child pointer using
    already-reserved parent capacity, so readers observe either the old or new ordering at a
    render boundary. After creating a child group as revision 10, the shared probe moves its
    revision-9 group to child index zero as revision 11 and verifies both the stable parent and
    final child index.
43. `ImmAuthoring_LayerReparent` now also supports cross-parent moves. Queueing and preparation
    reject root moves, cycles, non-group destinations, and invalid insertion indices.
    Preparation reserves destination capacity and builds replacement full names for every layer
    in the moved subtree without changing the published tree. Presentation removes the source
    child, inserts it into the destination, updates its parent, and move-publishes all prepared
    names without allocation; an unexpected insertion failure restores the source topology.
    The shared probe performs this move as revision 12 and verifies the new parent, child index,
    and canonical full name.
44. `ImmAuthoring_LayerDestroy` now accepts an entire subtree when every node is an ordinary,
    non-timeline group. Preparation captures and revalidates every stable layer pointer and the
    source child index. Presentation detaches the root once, removes all subtree entries from
    the sequence table in one allocation-free compaction, recursively deinitializes the tree,
    and deletes every layer object. A failed sequence removal restores the parent entry at its
    original index. Paint, spawn-area, media, instance, and timeline descendants remain
    `Unsupported` until their external references and renderer resources have explicit
    retirement handling. The shared probe creates a child group as revision 10, exercises
    reorder and cross-parent movement as revisions 11 and 12, then deletes both group handles
    as revision 13 and verifies the original layer count is restored.

The following experimental evidence still governs the renderer work:

1. Static drawing geometry can be rebuilt through `DrawingStatic::StartAdding`, `Add`, and
   `StopAdding`; `ReplaceGeometry` is the equivalent contiguous-element convenience path.
   Pretessellated drawings reject replacement, which is preferable to silently ignoring it.
2. Geometry conversion uses the exporter's quantisation conventions.
3. Erasing a drawing was rejected because frame buffers and renderer pools use drawing
   indices. Clearing a drawing to zero geometry hung `appImmViewer`.
4. Immediately destroying and rebuilding a layer's GPU buffers hung `appImmViewer`; delaying
   destruction by three frames avoided that observed hang but is evidence for deferred
   retirement, not the final lifetime mechanism.
5. Calling `iSLayerDrawInfoStatic::Init` on an existing live slot caused an access violation.
   The corresponding release sequence in `LayerRendererPaintStatic::UnloadInCPU` is
   `End()` followed by `mLayerInfo.Free(id)`, but doing that in place still does not provide an
   atomic revision swap.
6. Existing layer visibility and transform APIs are playback overrides. They demonstrate that
   the renderer reads some values live, but they do not complete canonical authoring-property
   mutation.

The player no longer attempts to infer GPU completion from render-call counts. It retires an
unreachable drawing's CPU wrappers at the next render boundary and calls the renderer's normal
resource-destruction methods. Those methods must preserve resources already referenced by
submitted work. Vulkan does so with its fence-backed deferred-destroy ring and Metal with
command-buffer completion retention; D3D and OpenGL rely on their API resource-lifetime
contracts. New renderer backends must meet the same destruction contract rather than adding a
player-level frame delay.

The next hierarchy step should define subtree and resource-owning deletion once reference and
renderer-retirement rules cover every descendant. Drawing and spawn-area commands remain
separate batch types. The replacement path already has frame-pair evidence on DirectX, desktop
OpenGL, standalone Windows Vulkan, standalone macOS Metal, and Android GLES/Vulkan.
