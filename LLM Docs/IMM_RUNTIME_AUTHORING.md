# IMM runtime authoring: status, architecture, and delivery plan

Reviewed against repository source on 2026-10-14 at `f93d3f23`.

The [content pipeline roadmap](IMM_CONTENT_PIPELINE_PLAN.md) owns shared conversion,
reader and authoring dependencies and delivery order. This document owns the detailed
runtime authoring architecture, milestones and acceptance requirements.

This is the consolidated planning and architecture document for runtime animation authoring.
It replaces the previous engine plan, live-editor roadmap, and live-document architecture.
Implemented behavior, recorded validation, and future requirements are distinguished below.
This review was static; it did not rerun builds, benchmarks, devices, or CI.

## 1. Current position

The original managed paint-authoring engine is implemented, including transactions, import,
memory/file export, and compile-and-reload preview. Spawn areas and native exporter builds for
macOS, Android, and iOS followed the original Windows delivery.

The successor native live-edit path now supports atomic geometry replacement, drawing creation
and frame mapping, drawing deletion, canonical properties, selected animation keys, spawn-area
edits, and group hierarchy operations. It uses owned batches, stable identities, prepared
replacements, and renderer-owned resource retirement rather than mutating active GPU state.

The live editor SDK is not yet complete. Matched live-edit latency acceptance is unrecorded;
live-revision save/publish and managed API cutover remain outstanding. Native mutation breadth
and application integration are still restricted. Platform evidence for an earlier geometry
slice does not establish acceptance of every later command on every platform.

| Area | Current state | Remaining acceptance or implementation |
|---|---|---|
| Original managed authoring engine | Delivered; preview still compiles and reloads IMM bytes | Retain as explicit compatibility path during cutover |
| Native atomic live-edit foundation | Implemented with recorded failure and frame-consistency evidence | Matched latency measurements and complete acceptance audit |
| Native editing breadth | Drawings, properties, keys, spawn areas, and group hierarchy supported within limits | Resource-owning layer lifecycle and broader batch combinations |
| Save/publish from live revision | Not delivered by the current live authoring surface | Writer projection, revision selection, round-trip acceptance |
| Managed live authoring | Not delivered | Bindings, transaction/revision mapping, preview cutover |
| Surfaces/platforms | Shared viewer probe and substantial backend/build evidence | Complete live SDK integration and per-operation acceptance |
| Optional optimisations | No acceptance claim | Measure a limiting interaction before proposing changes |

## 2. Scope and source authority

The product objective is to let an application edit a playing IMM document at interactive
latency. This repository supplies the engine and SDK; the consuming application supplies its
viewport tools, timeline, layer tree, inspectors, undo, autosave, and other editor UI.

The intended live scope includes static paint and groups, drawings and frame mappings,
canonical properties, supported animation keys, spawn areas, and save/publish. It excludes a
new IMM format, persistent authoring handles across reload, concurrent writers, multi-document
transactions, lossless arbitrary source precision, and picture/model/sound/effect mutation.
Per-drawing GPU suballocation is not a prerequisite.

Implementation declarations and code are authoritative for what callers can use today:

1. [Public C ABI](../code/appImmUnity/src/imm_authoring.h): signatures, exact types, structures,
   limits, result codes, and calling convention.
2. [Player document](../code/libImmPlayer/src/document.cpp) and
   [document declarations](../code/libImmPlayer/src/document.h): session, batches, preparation,
   presentation, and lifecycle.
3. [Managed authoring source](../code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/Authoring):
   the currently shipped mutable graph, compiler, importer, and preview coordinator.
4. [Plugin reference](../docs/unity-plugins.md): platform packaging, wrappers, and exporter work.
5. [Original engine contract](../docs/runtime-authoring-engine-contract.md): detailed managed
   contracts and historical benchmark/test evidence. Its Windows-only and spawn-area exclusions
   are stale; they must not override the current source or this document's status. Its
   compile-and-reload decision is historical, not the live architecture decision.

This document owns the consolidated objective, intended guarantees, delivery order, and
acceptance requirements. It does not redefine exported types or claim planned APIs exist.

## 3. Delivered managed engine and compatibility path

The original Phases 0–6 delivered the following:

1. A runtime-safe mutable document independent of editor UI and Unity scene objects, with
   document metadata, nested group/paint hierarchy, drawings, strokes, frame mappings,
   properties, supported animation keys, stable document-local IDs, and revisions.
2. Atomic managed transactions with expected-revision conflict detection. Rejected edits leave
   content unchanged; committed transactions emit one revision/change notification. Disposing
   an uncommitted transaction aborts it. This managed model is not yet mapped onto live commits.
3. Validation and compilation to IMM file or owned memory output, with structured errors,
   source revision, statistics, operation limits, progress, and cancellation.
4. Preview coordination that compiles immutable snapshots, ignores superseded results, keeps
   the last valid preview on failure, preserves requested playback state/time, and releases
   replaced documents/buffers. The coordinator still calls `ExportToMemory` and
   `LoadDocumentFromMemory`; interactive managed preview has not switched to live edits.
5. Supported paint import through the stroke reader, structural comparison, and round trip.
   Import reports issues and lossiness; `CanOverwriteSource` requires a successful lossless
   import. Arbitrary IMM files are not promised a lossless round trip.
6. Capability reporting, packaging, an opt-in `Samples~/RuntimeAuthoring` sample, and an
   independent package-consumer assembly. Capability flags describe the original engine,
   not acceptance of the live path.

The managed/exporter scope includes still and animated sequences; groups, paint, and spawn
areas; transforms/pivots/visibility/opacity; drawing/frame operations; supported stroke brush
sections and visibility modes; and supported layer animation keys. Legacy component
position/rotation/scale keys are not a substitute for supported transform-key authoring.
Managed/exporter key breadth is greater than the live ABI's visibility/opacity/transform subset.

The offline graph/exporter now has explicit `ImmAuthoringFlip` values for layer,
pivot and transform-key reflections. Import and structural comparison retain them.
Two additive native exporter calls preserve the legacy transform ABI. Windows native
regression CI and Windows/Android/macOS/iOS builds and package export checks pass in
run 39609583383. Unity package compilation/tests also pass, including the reflection
graph/comparison test and all 15 authoring document tests. Three D3D12 runtime tests
were skipped on the Linux host. Unity player integration and device acceptance
remain pending; the shared roadmap tracks current failures and validation status.
This does not expand the current live mutation ABI's transform contract.

The offline graph also has optional `SetRootProperties` / snapshot `RootProperties`
for the canonical Root group: visibility, opacity, transform/pivot flips and timeline
duration/repeats. Transactions, snapshots and structural comparison retain them;
`ImmExporter_SetRootLayerProperties` serializes them without changing the legacy
transform ABI. Unset properties retain the old compiler defaults. Windows native
serialization and graph checks pass locally; other-platform/Unity CI is pending.
Root key/chapter construction, root property import and direct live root mutation
remain incomplete. This is not full root import/export or live-authoring acceptance.

Paint round trips preserve the codec's representation, not arbitrary source floats: alpha and
width are quantised, always-visible direction is omitted by the codec, and point length/time
are derived. Unsupported or reinterpreted content must remain explicit in import diagnostics.
Valid zero-width tips are now retained by the importer as well as the graph/compiler.
The native four-brush export/import/export regression passes locally and in Windows
CI run 39616195981. Negative/nonfinite width sanitization remains unchanged.

September additions implemented spawn-area graph/import/export/comparison support and exporter
builds for Windows, macOS, Android arm64, and iOS. `ImmAuthoringRuntime.Capabilities` currently
enables authoring on 64-bit processes; that check alone is not a backend validation result.
The formerly missing `ImmExporter_LayerAddAnimationKey` and
`ImmExporter_PaintSetMaxRepeatCount` exports were also implemented.

Pictures, sound, models, effects, references/instances, and complete comic/chapter authoring
remain outside the delivered managed scope. The compiler rejects comic export. These are
separate potential extensions, not prerequisites for the live paint editor.

## 4. Measured motivation and acceptance budget

The recorded full compile-and-reload `edit-document` baseline is:

| Corpus | Drawings | Recorded full compile/reload |
|---|---:|---:|
| Small | 10 | 1.79 ms |
| Medium | 400 | 51.61 ms |
| Large | 4,000 | 561.74 ms |

These are historical baseline figures, not measurements of today's live implementation.
The September stage investigation identified serialization and decoding as the dominant
costs: approximately 18 ms graph construction, 120 ms memory serialization, 510 ms file
serialization, 240 ms player CPU loading, and 431 ms synchronous stroke-reader import for
the large workload. Those stage figures represent distinct measured paths; do not add them
as if every preview performs all of them or compare them as matched live-edit results.

The live acceptance targets remain:

1. One changed drawing: Small and Medium presented in under 16 ms; Large under 50 ms.
2. Canonical visibility/opacity/transform: presented in under 2 ms, without tessellation or
   geometry upload.
3. Work scales with affected content rather than unrelated document content. No unrelated
   layer is rebuilt, and interactive commits do not encode and parse IMM bytes.

The [exporter benchmark](../code/appImmUnity/tests/exporter_benchmark.py) currently records the
compile-and-reload comparison. The proposed live cases and matched median/p95 acceptance
results are still missing. Functional viewer probes do not establish those latency targets.

## 5. Live document architecture and identity

`ImmPlayer::Document` owns the existing `ImmImporter::Sequence`. CPU/GPU updates consume that
sequence through the layer renderers supplied to the document. The live target is this runtime
document, not a second permanent graph or an alternative file format. Owned batches, handle
maps, prepared timelines, and replacement drawing/renderer objects are transactional state.

The architecture requires:

1. Every rendered frame observes one coherent presented revision.
2. Public identity is independent of drawing vector positions, renderer slots, and GPU offsets.
3. Old GPU resources remain valid for submitted work through backend-owned lifetime handling.
4. Playback without attached authoring retains the normal load/update/draw path.

| Object | Current public identity | Resolution/lifetime |
|---|---|---|
| Document | `int32_t docId` | Existing player document slot |
| Layer/spawn area | `int32_t layerId` | Stable layer ID; current implementation searches the hierarchy |
| Drawing | `uint64_t drawingId` | Session handle map resolves to layer and private drawing storage |
| Commit | `uint64_t revision` | Assigned when a batch is sealed |
| Element | Index within one submitted drawing | Drawing-owned geometry |
| Animation key | Layer, property, exact tick | ABI seconds are converted to ticks |

Layer IDs are currently 32-bit, not the 64-bit layer handles proposed in the earlier design.
New layer IDs and drawing handles are allocated monotonically within the active session and
are not reused after destruction, discard, or failed creation. Drawing objects are individually
allocated so growing/compacting private index tables does not relocate surviving objects.
Frame tables may store private drawing indices; public frame calls use drawing handles.

Handles are invalid after content replacement/unload and are not promised across save/reload.
Persistent IDs require a separately approved format extension or sidecar. A reserved drawing
handle can be used by geometry and frame mapping later in its supported creation batch; no
general cross-batch dependency/query-overlay facility is currently promised.

## 6. Session, batches, and revision semantics

The current document supports one open batch and one unresolved sealed/prepared commit.
Multiple concurrent writers and arbitrary mixed-operation transactions are unsupported.

1. `Attach` enables editing and discovers imported drawing identities.
2. Mutation calls validate input and copy caller payloads into the open batch. They do not
   mutate the presented model or renderer during the ABI call.
3. Queries return committed/presented state, not an overlay of pending commands.
4. `Commit` validates the supported batch shape, assigns a revision, transfers owned payloads
   to the normal document command stream, and empties the open batch.
5. Preparation resolves references and builds independent model/renderer state. Presentation
   publishes it at a render boundary under the player lock.
6. Callers poll `GetRevisions`/`GetCommitStatus`. Successful queueing does not mean presentation.

| Revision/state | Meaning |
|---|---|
| requested | Sealed batch accepted into the queue |
| prepared | Required replacement state built successfully |
| presented | Published atomically to playback/rendering |
| rejected | Failed batch; old active state retained |

Commit states are `Unknown`, `Queued`, `Preparing`, `Prepared`, `Presented`, and `Rejected`.
Rejected revision numbers are not reused. History retains the most recent 64 commit statuses;
it is not an unlimited session audit trail. Applications needing a complete history must retain
results themselves. Detach/content replacement clears session status and handle state.

An empty or unsupported batch shape is rejected as `InvalidState` by current `CommitEdits`.
An occupied sealed/presentation slot returns `QueueFull` without assigning a revision or
discarding the open batch. `DiscardPending` releases unsealed commands; it is not cancellation
of an unresolved commit. Detach rejects a non-empty open batch or unresolved work. Load/unload
forcibly invalidates the session, releases pending payloads, and cleans prepared resources
through the normal renderer unload/cancellation path.

Current supported batch shapes are:

| Shape | Restriction |
|---|---|
| Geometry replacement | One drawing |
| Drawing creation + geometry + optional frame mapping | In that order, all targeting the reserved drawing in one layer |
| Frame mapping | One frame to a committed handle |
| Drawing deletion | One drawing, unreferenced in the resulting frame table |
| Frame remap + drawing deletion | Remap first; no other remaining frame may reference the deleted drawing |
| Canonical layer properties | One layer; visibility/opacity/transform masks may be combined |
| Animation key insertion/replacement/removal | One key edit |
| Default spawn selection | One existing spawn layer |
| Spawn state update | One existing spawn layer |
| Group creation, group-subtree deletion, or reparent | One hierarchy operation per batch |

The `DependencyFailed` result is reserved in the ABI. Its presence does not imply an implemented
multi-commit dependency queue. Managed expected-revision transactions likewise must not be
assumed to exist in the native ABI before M4 maps their semantics explicitly.

## 7. Atomic preparation, publication, and resource lifetime

Preparation resolves references, checks layer types, frame references, hierarchy cycles,
geometry/value limits, and allocation requirements without changing presented content.
Geometry preparation builds a separate `DrawingStatic` and static-renderer slot; GPU upload
populates that slot while the old drawing/slot remains active. Publication swaps model geometry
and renderer-slot identity together. Upload/preparation/presentation failure releases unpublished
resources and preserves the old active pair.

The current implementation replaces affected drawings and slots; a whole-layer replacement is
an allowed correctness fallback, not a claim that every edit currently clones a layer. Neither
path may rebuild unrelated layers. Property, key, frame, spawn, and group-only operations avoid
paint geometry upload. They prepare only the relevant values, timelines, reference tables,
hierarchy capacity, or subtree names and publish without allocation where required.

Retirement belongs to the renderer backend:

1. Replaced drawing CPU wrappers and slots are retained until the next render boundary.
2. GPU destruction then uses the backend's submitted-work lifetime mechanism.
3. Vulkan uses its fence-backed deferred-destroy submission ring. Metal retains vertex/index
   buffers until the owning command buffer completes, including externally owned command buffers.
4. DirectX and OpenGL rely on native deferred resource-deletion semantics.
5. There is no player-level fixed three-frame delay or guessed `maxFramesInFlight` fallback.
   A new backend must meet the same lifetime contract before live authoring is enabled.

Derived state must follow the presented revision: drawing/layer bounds, frame references,
animation timelines/evaluated values, moved-subtree full names, and spawn-area list/pose caches.
Canonical base visibility/opacity/transform are stored separately from animation evaluation and
transient playback overrides. Authoring does not silently clear overrides; applications decide
whether to preserve or clear them. Spawn publication marks the spawn cache dirty.

Public calls use the player mutex; sealed edits enter the existing document command slot rather
than a second independent render queue. Preparation/presentation and GPU operations follow
render-thread/backend affinity. Calls are not re-entrant from renderer callbacks. Unload wins
over pending authoring and must never publish a partial revision.

## 8. Implemented C ABI and mutation limits

The header is the exact declaration source and is included by the plugin implementation.
Exports return signed `int32_t` result codes; Windows uses `__stdcall`, other supported targets
use the header's platform visibility/calling convention. Structures use fixed-width scalar
fields and explicit pointer/count spans, without C++ containers or `bool`. Versioned descriptors
start with `structSize`/`structVersion`; current callers supply version 1 and at least the
required size. Do not assume arbitrary older structure prefixes are accepted.

Pointer-backed mutation data is copied before return. Strings are explicitly sized UTF-8 spans.
Reserved input fields must meet the declared validation; readback normalizes reserved/inactive
fields. Compile-time size/offset assertions protect cross-library layouts.

| Surface | Implemented exports (all prefixed `ImmAuthoring_`) |
|---|---|
| Session/revision | `Attach`, `Detach`, `IsAttached`, `DiscardPending`, `Commit`, `GetRevisions`, `GetCommitStatus` |
| Layers | `LayerCreate`, `LayerDestroy`, `LayerReparent`, `LayerGetProperties`, `LayerSetProperties` |
| Drawings/frames | `DrawingGetHandle`, `DrawingCreate`, `DrawingDestroy`, `DrawingSetGeometry`, `FrameGetDrawingHandle`, `FrameSetHandle` |
| Keys | `KeySet`, `KeyRemove` |
| Spawn | `SetInitialSpawnArea`, `SpawnAreaSet`, `SpawnAreaGet` |

`DrawingGetHandle(layerId, importedDrawingIndex)` is transitional discovery, not public drawing
identity by index. Legacy index-based `DrawingAdd` and `FrameSet` remain exported but return
`Unsupported`; callers must use handle-based commands.

### 8.1 Geometry and frame operations

Version 1 supports static paint drawings with 1–65,536 elements, 2–8,192 points per element,
and at most 1,048,576 total points per drawing. Point brush sections and pretessellated drawing
mutation are unsupported. Validation covers counts/overflow, finite values, brush, visibility,
color space, flip, alpha, and width. A non-positive `biggestStroke` requests computation; a
positive value below a submitted width is rejected.

Open/sealed batches store compact point vectors, not fixed-capacity importer `Element` blocks.
Preparation streams through one temporary importer element at a time using `StartAdding`,
`Add`, and `StopAdding`. Quantisation follows existing exporter alpha/width conventions.

Creation requires geometry before commit and may frame-map the reserved handle in the same
batch. Failed preparation publishes neither drawing nor frame mapping. Deletion rejects live
frame references, compacts only private storage indices, preserves surviving object/handle
identity, and retires the old model/renderer slot. Clearing geometry to simulate deletion is
not the supported deletion path.

### 8.2 Properties and keys

`LayerSetProperties` supports canonical visibility, opacity, and uniform-scale rigid transform,
individually or together. Version 1 carries translation, unit quaternion, and positive uniform
scale. Name, pivot, timing, and other property masks are not implemented. Readback excludes
animation-evaluated values and transient overrides.

Live keys support visibility, opacity, and transform. `KeySet` inserts/replaces at the converted
tick; preparation copies the affected sorted timeline and publication swaps that vector.
Visibility accepts only `NONE` interpolation because evaluation is stepped. Unsupported
property/interpolation/value combinations are rejected. `KeyRemove` uses exact converted ticks;
a missing key rejects the asynchronous commit with `NotFound` and leaves the timeline intact.

### 8.3 Spawn and hierarchy

Spawn updates support sphere or axis-aligned box volume, floor/eye tracking, per-axis
locomotion mask, and the owning layer's rigid transform. Validation rejects invalid enums/masks,
non-finite values, non-positive radii/scale, inverted bounds, and non-unit quaternions. Volume
and transform publish together; default selection targets an existing spawn layer ID.

Layer creation currently supports only `Group`, with an existing group parent and bounded
UTF-8 name. Preparation initializes a detached group and reserves sequence/parent capacity;
publication appends without allocation and rolls back unexpected partial publication.

Reparent supports sibling reorder and cross-parent moves, rejecting roots, cycles, invalid
indices, and non-group destinations. Cross-parent preparation builds all affected subtree full
names and destination capacity; presentation updates topology/names together with rollback on
unexpected insertion failure.

Deletion supports a non-root subtree containing only ordinary, non-timeline groups. It detaches
once and removes sequence entries atomically. Paint, spawn, media, instance, and timeline
descendants remain unsupported until all references and renderer resources have retirement
rules. Paint/spawn layer creation is likewise not delivered by the live layer-creation ABI.

### 8.4 Result taxonomy and diagnostics

| Code | Name | Meaning |
|---|---|---|
| 0 | `Ok` | Call succeeded; for commit, queued rather than necessarily presented |
| -1 | `NotFound` | Missing document, committed object, or requested key |
| -2 | `InvalidArgument` | Malformed structure/count/value/range |
| -3 | `Unsupported` | Unsupported surface, content, or ABI version |
| -4 | `InvalidState` | Detached/invalid lifecycle or invalid batch shape |
| -5 | `OutOfMemory` | Allocation failure |
| -6 | `ValidationFailed` | Invalid proposed hierarchy/reference state |
| -7 | `QueueFull` | Open operation capacity or unresolved commit capacity occupied |
| -8 | `DependencyFailed` | Reserved dependency-failure result |
| -9 | `Cancelled` | Cancellation/lifecycle invalidation |
| -10 | `RendererFailed` | Replacement renderer preparation/presentation failure |

Immediate validation failures leave the submitted operation unapplied. Asynchronous failures
are exposed in commit status with result, command index, and object where available. Structured
results are the application error interface; normal-operation logs must not emit per-point data.

The shared viewer probe uses `[IMM_LIVE_EDIT...]` diagnostic prefixes and
`IMM_VIEWER_LIVE_EDIT=<frame>`. `IMM_LIVE_EDIT_TRACE_FRAMES=1` enables model/renderer-pair
tracing. Failure controls include `IMM_LIVE_EDIT_FAIL_PREPARE_CPU`,
`IMM_LIVE_EDIT_FAIL_PREPARE_GPU`, and `IMM_LIVE_EDIT_FAIL_PRESENT`.
The managed coordinator uses `[IMM_AUTHOR_PREVIEW]`; the old plan's other proposed subsystem
prefixes are not a delivered logging requirement.

## 9. Recorded validation and limits of the evidence

The following evidence was recorded before this consolidation; it was not rerun on 2026-10-14.

1. Original managed delivery (2026-07-19): early Phase 4 verification passed 21 PlayMode tests;
   Phase 5 recorded two 29/29 runs; the detailed Phase 6 contract records two 37/37 runs. These
   are successive historical suites, not current counts or proof of later spawn/live coverage.
   The original lifecycle gate exercised 100 compile/load/play/unload cycles.
2. Geometry replacement: Windows DirectX normal replacement reached `Presented` and changed
   drawing bounds. Injected CPU/GPU/pre-presentation failure produced `Rejected` with unchanged
   target bounds. Delayed DirectX, OpenGL, and standalone Windows Vulkan runs each sampled 61
   target frames with matching model geometry and renderer slots across the transition.
3. Hosted run `35453794170` exposed Android Clang override/type portability failures despite
   successful independent jobs. Fixed-width export definitions and consistent overrides fixed
   local Android native builds; that failed run is not an accepted full validation result.
4. Hosted run `35463143679` passed the macOS standalone Metal contract and Android standalone
   GLES/Vulkan device jobs plus the other executed validation jobs. Android captured unchanged
   baseline output before invoking the live probe. The earlier inconclusive standalone Windows
   Vulkan attempt was superseded by a completed successful probe.
5. Hosted run `35471831849` passed the executed build/core/web/iOS/engine/GPU/device/evidence
   jobs with backend-owned retirement, including Unity/Godot coverage. This is evidence for
   that slice, not blanket live authoring acceptance for all consumers.
6. Hosted run `35499760852` passed every executed job for drawing creation. Later frame,
   property, key, spawn, and hierarchy slices have recorded local Windows DirectX probe
   evidence; do not inherit all-platform coverage from earlier runs without checking reports.
7. The shared probe now exercises replacement, atomic create-and-map, restore-and-delete,
   canonical properties under overrides, key insertion/removal, spawn changes when available,
   group creation, reorder, cross-parent movement, and group-subtree deletion through revision
   13. Latest focused hierarchy commits were dated 2026-09-20. Probe coverage is not the same
   as managed SDK coverage, a complete failure matrix, or matched latency acceptance.

Android Vulkan direct-display portability remains a separate integration issue: a local phone
can expose a 1x1 Unity Vulkan display render buffer while the camera surface is full resolution.
That does not negate standalone/Firebase Android Vulkan rendering or live-probe evidence.

## 10. Delivery milestones and acceptance

Milestone numbering is retained to make existing discussions understandable. Implementation
and acceptance are separate: no milestone is complete merely because its APIs exist.

| Milestone | Current assessment | Exit requirements |
|---|---|---|
| M0: architecture/harness | Architecture consolidated; harness partly established | Keep compile/reload baseline, failure injection, revision-consistency checks, and reproducible evidence |
| M1: one-drawing vertical slice | Substantially implemented; formal acceptance pending | Atomic old-or-new presentation, safe retirement, session/handle/status contracts, property distinction/drawing deletion, no IMM encode/parse, regressions, Large edit under 50 ms |
| M2: editing breadth/cache correctness | Partially implemented | Finish agreed layer lifecycle/batch scope, verify derived caches for every operation, property edits under 2 ms, no unrelated rebuild |
| M3: save/publish | Outstanding | Serialize an explicitly selected presented revision; define unresolved-commit behavior; preserve compatibility and round-trip/render baselines |
| M4: managed cutover | Outstanding | Map managed transactions/revisions and diagnostics onto live native commands; pass managed tests on the live path; remove IMM byte processing from interactive preview |
| M5: surfaces/platforms | Early viewer/backend groundwork delivered | Integrate Unity sample/viewer and validate supported operations/lifetimes/latency on each supported platform/backend; report unsupported surfaces explicitly |
| M6: measured optimisation | Deferred | Only pursue a demonstrated interaction bottleneck after correctness and earlier acceptance |

M3 save must never observe a partially prepared batch. Its API must either reject unresolved
newer commits or explicitly select a presented revision. Existing managed export does not
establish this live-save guarantee. IMM remains the persistence, import, publish, and process
boundary format; explicit compatibility preview may still compile/reload during migration.

### 10.1 Measurement and regression requirements

Extend the benchmark with `edit-property`, `edit-geometry-small`, `edit-geometry-large`,
`edit-add-drawing`, `edit-destroy-drawing`, and `edit-timeline`, retaining `edit-document` as
the full-reload comparison. Use Small/Medium/Large matched workloads and artifact hashes.
Report ABI validation/copy, queue latency, CPU preparation, GPU preparation/upload, and time
from `Commit` to presented revision, with median and p95. State workload geometry/key counts,
platform/backend, build, and failure outcomes. Do not substitute queue-call duration for
visible presentation latency.

Acceptance checks must establish:

1. Non-authoring playback remains unchanged; additive plugin exports and managed compatibility
   are checked with the repository's export verifier and consumer coverage.
2. Existing content load/export baselines remain valid using
   `tests/tools/verify_imm_baseline.py` and render-metric comparisons.
3. `tests/tools/run_player_regressions.py` passes the player-facing regression set; relevant
   platform jobs retain their required gates.
4. Failure injection covers CPU/GPU allocation or upload, pre-presentation failure, unload
   during preparation, and invalid cross-reference: old state remains active and resources
   are released. Existing examples do not substitute for missing cases.
5. Every sampled frame during publication has matching model/renderer revision. Verify stale
   handles, deletion references, overrides, hierarchy/name consistency, and spawn caches.
6. Save/round-trip and managed live-path checks are added when those milestones exist.

### 10.2 Next work and stop conditions

1. Close the acceptance evidence gap for the implemented vertical slice: add matched live-edit
   measurements and audit the regression/failure matrix before claiming M1 completion.
2. The next native hierarchy step is resource-owning subtree deletion, with explicit reference
   cleanup and renderer retirement for every descendant. Paint/spawn layer creation also
   needs prepared implementation resources before publication. Keep unsupported types explicit.
3. Complete the agreed M2 scope, then implement presented-revision save/publish and managed
   cutover in that order. Preserve the original compatibility path until the relevant live
   surface/backend is accepted.
4. If the Large M1 edit cannot meet 50 ms without unrelated work, stop expanding mutation
   breadth and profile the limiting stage. Follow
   [the performance investigation procedure](../code/projects/web/performance/INVESTIGATION_PROCEDURE.md)
   for web performance work; record baseline, plausible end-to-end saving, experiment budget,
   acceptance checks, and stop condition before changing production behavior.
5. Keep live authoring gated off on a backend that cannot provide atomic publication/resource
   lifetime. Never silently compile/reload while reporting live-edit latency.
6. Do not promote an optimisation that only creates CPU headroom without the required visible
   latency benefit. Compare independent changes on matched artifacts and retain rejection
   decisions; revisit only with new evidence or a changed target.
7. Do not promise persistent IDs, arbitrary mixed batches, new media types, or an editor UI as
   incidental additions. Each requires explicit scope/contract work.

## 11. Retained implementation decisions

The previous experimental chronology is condensed here so rejected approaches are not retried
without new evidence:

1. Direct active-geometry mutation, index-based identity, in-place active-slot refresh, and
   document dirty queues were replaced by owned batches and prepared publication.
2. Immediate GPU layer teardown/rebuild hung the viewer; reinitializing an existing live
   static-renderer slot caused an access violation. Those paths are not atomic publication.
3. Clearing a drawing to zero geometry hung the viewer. Explicit reference-checked drawing
   deletion and backend retirement replaced that workaround.
4. A three-frame destruction delay avoided one observed hang but did not prove GPU completion.
   Both the fixed delay and player-level frames-in-flight guess were removed.
5. Compact batch point vectors replaced fixed-capacity importer-element payloads, which cost
   roughly 512 KiB per element even for short strokes. Temporary importer storage remains
   confined to preparation.
6. Playback overrides are transient. Canonical authoring properties and their query surface
   are separate, and animation evaluation must not overwrite authored base values.
7. The old plan's estimated engineer-month totals and completion claims are historical and
   are not estimates or acceptance results for the remaining live editor work.
