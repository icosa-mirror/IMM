# IMM Runtime Animation Engine Plan

## 1. Objective

Extend the existing IMM Unity integration from a playback-only runtime into an engine and SDK that applications can use to create, modify, preview, import, and export IMM animations at runtime.

This repository will provide the runtime animation engine. It will not provide the application-level animation editor UI. A separate application will consume the APIs defined here and supply its own interaction model, viewport tools, timeline, layer tree, inspectors, persistence workflow, and user experience.

The initial engine scope should focus on paint animation. Additional IMM layer types can be added incrementally after the paint authoring path is stable and measured with representative content.

## 2. Repository Boundary

### 2.1 In scope for this repository

- A mutable runtime representation of supported IMM content.
- Stable identifiers for documents, layers, drawings, frames, strokes, and animation keys.
- Runtime APIs for creating, querying, changing, reordering, and deleting supported content.
- Native C ABI and managed C# bindings suitable for use by Unity applications.
- Validation and structured error reporting.
- Transactions or revisions for applying edits safely.
- Compilation from mutable runtime data into IMM playback data.
- IMM serialization to files and memory.
- Loading generated content into the existing IMM player.
- A defined mechanism for updating authoritative playback previews.
- Import of supported existing IMM content into the mutable representation.
- Compatibility, lifecycle, performance, and round-trip tests.
- Minimal samples and diagnostic harnesses that demonstrate engine APIs.
- Platform builds and native dependency packaging.

### 2.2 Out of scope for this repository

- Timeline, dope-sheet, curve-editor, layer-tree, and inspector UI.
- Viewport drawing, erasing, selection, transform gizmos, and picking UX.
- Keyboard shortcuts and input bindings.
- Onion-skin visualization as an application feature.
- Application-level undo history and command presentation.
- Project browsers, recent-file lists, autosave policy, and recovery UI.
- Dialogs, progress windows, notifications, and user-facing error presentation.
- Product-specific editable-project formats unless a format is required as a stable engine interchange contract.
- Collaboration, accounts, cloud storage, and application-specific metadata.

The engine may expose primitives that make these features possible, such as atomic edit transactions, stable IDs, change notifications, cancellation, progress callbacks, and snapshots. The consuming application decides how those primitives appear to users.

## 3. Current State

### 3.1 Playback API

The Unity package currently exposes document loading, playback, seeking, chapter navigation, layer inspection, and limited layer overrides through:

- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmNativePlugin.cs`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmDocument.cs`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs`

The player can load IMM data from a file or memory and can temporarily override layer visibility, opacity, and transform. These overrides affect playback but do not mutate or serialize the imported IMM document.

### 3.2 Export API

The repository contains an initial Unity-facing exporter in:

- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmExporter.cs`
- `code/appImmUnity/src/main.cpp`
- `code/libImmExporter/`

The current managed surface can create sequences, nested groups, paint layers, drawings, stroke elements, stroke points, and frame mappings, then export them to an IMM file.

The native exporter contains additional concepts, including animation keys and other layer implementations, that are not exposed through the Unity C ABI. The current exporter bridge in `appImmUnity/src/main.cpp` is also guarded by `WINDOWS`.

### 3.3 Import API

The stroke reader package can inspect supported IMM paint data through:

- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ImmStrokeReader.cs`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/SharpQuillCompat.cs`
- `code/appImmStrokeReader/`

It exposes hierarchy, transforms, drawings, strokes, points, frame buffers, animation metadata, and selected picture data. It is useful for paint import but is not currently a lossless general IMM round-trip API.

## 4. Principal Architectural Constraint

The exporter and player currently own separate native representations:

```text
Runtime authoring data
        |
        v
libImmExporter document graph
        |
        v
serialized IMM bytes
        |
        v
libImmImporter/libImmPlayer document graph
        |
        v
CPU and GPU playback resources
```

There is no API for passing a live exporter sequence directly to the player, and there is no general API for incrementally adding, replacing, or deleting geometry in a loaded player document.

The initial engine should therefore support a reliable compile-and-reload path. Incremental native-player mutation should only be implemented if measurements with realistic documents show that compile-and-reload cannot meet the consuming application's latency requirements.

## 5. Proposed Engine Architecture

```text
Application-owned editor state and UI
                 |
                 v
       IMM Runtime Authoring API
                 |
        +--------+--------+
        |                 |
        v                 v
 Mutable document   Change/revision events
        |
        v
 Validation and compilation
        |
        +-----------------------+
        |                       |
        v                       v
 IMM file/memory output    libImmPlayer preview
```

### 5.1 Mutable runtime document

Add a runtime-safe model independent of application UI and Unity scene objects. It should represent:

- Document type, frame rate, background, capabilities, and resource requirements.
- Group and paint-layer hierarchy.
- Layer name, visibility, opacity, transform, pivot, timing, duration, and repeat settings.
- Drawings and frame-to-drawing mappings.
- Stroke elements, brush properties, and points.
- Supported animation keys and interpolation.
- Stable document-local IDs.
- A monotonically increasing document revision.

The public API should support both individual operations and efficient bulk construction.

### 5.2 Ownership model

The runtime API must define ownership explicitly:

- A document owns all layers.
- Paint layers own drawings and frame mappings.
- Drawings own strokes.
- Strokes own point buffers.
- Handles become invalid when their owning document or parent object is destroyed.
- Managed wrappers prevent or clearly reject calls through disposed handles.

Applications must not receive raw native pointers.

### 5.3 Edit transactions and revisions

Support atomic edit transactions for related mutations:

```text
BeginEdit(document, expectedRevision)
  AddLayer(...)
  ReplaceStrokePoints(...)
  SetFrameDrawing(...)
CommitEdit() -> newRevision or structured error
```

Required semantics:

- A failed operation can abort the transaction without exposing a partially changed document.
- An expected revision detects stale writers.
- A successful commit emits one change notification containing the new revision and affected IDs.
- Read-only snapshots remain coherent while a write transaction is prepared.

The consuming application can build undo, collaboration, or autosave behavior above this contract without those features living in this repository.

### 5.4 Authoritative playback preview

The engine should provide a coordinator that compiles a document revision and loads it into `libImmPlayer`.

It should:

- Accept explicit preview requests from the application.
- Support cancellation or supersession of obsolete requests.
- Ignore results compiled from stale revisions.
- Preserve requested playback time and playback state across replacement.
- Keep the last valid preview if compilation or loading fails.
- Dispose replaced documents and buffers deterministically.
- Return structured status, timing, and diagnostics.

The engine will not decide when the application should request a preview.

### 5.5 Serialization

Support two output paths:

- Export to a named IMM file.
- Export to an owned memory buffer for immediate player loading or application-controlled storage.

The API must define buffer ownership and release rules. Export results should include byte size, source revision, warnings, errors, and timing statistics.

An application-specific editable-project format is not required here. If the mutable document needs persistence independent of IMM, expose a versioned engine snapshot format with documented compatibility semantics and no UI state.

## 6. Initial Supported Feature Set

### Included

- Windows x64 runtime authoring.
- New documents.
- Still and animated sequences.
- Group and paint layers.
- Nested layer hierarchy.
- Layer transforms, pivots, visibility, and opacity.
- Multiple drawings per paint layer.
- Frame-to-drawing mappings.
- Segment, circle, ellipse, and square brush sections with always-visible or
  quadratic-fade visibility.
- Stroke point position, normal, color, alpha, and width. Direction is preserved
  for quadratic-fade strokes; always-visible direction is omitted by the binary
  codec. Length and time are derived by that codec rather than treated as
  arbitrary lossless stored values.
- Visibility, opacity, transform, draw-in-time, action, loop, and offset layer
  animation keys. Obsolete component position/rotation/scale keys are rejected
  in favour of transform keys.
- Import and export of the supported paint subset.
- Authoritative playback through `libImmPlayer`.

### Deferred

- Effects.
- Models.
- Pictures.
- Sound.
- References and instances.
- Spawn areas.
- Complete comic/chapter authoring.
- Cross-platform authoring.
- Guaranteed lossless round trip for arbitrary IMM files.
- Incremental GPU mutation in a loaded player document.

## 7. Public API Requirements

The exact naming can change during implementation, but the supported operations should include the following categories.

### 7.1 Document lifecycle

- Create and destroy an authoring document.
- Import supported content from a file or memory.
- Query document revision and capabilities.
- Validate a document.
- Export to a file or memory.
- Create an immutable read snapshot.

### 7.2 Layer operations

- Create group and paint layers.
- Query layer type and properties.
- Rename a layer.
- Set transform, pivot, visibility, opacity, timing, duration, and repeat count.
- Reparent and reorder a layer.
- Remove a layer and its descendants.
- Enumerate children in stable order.

### 7.3 Drawing and frame operations

- Create, clone, query, and remove drawings.
- Append, insert, replace, move, and remove frame mappings.
- Resize a frame sequence safely.
- Query the drawing referenced by a frame.
- Detect and reject dangling drawing references.

### 7.4 Stroke operations

- Create, clone, query, replace, and remove strokes.
- Upload or retrieve points in batches.
- Update brush and visibility properties.
- Reorder strokes within a drawing.
- Recompute or validate bounds.

### 7.5 Animation-key operations

- Add, replace, query, move, and remove keys.
- Support explicit property and value types.
- Support interpolation modes implemented consistently by exporter and player.
- Validate duplicate times and unsupported property/interpolation combinations.

### 7.6 Diagnostics

- Structured result codes.
- Descriptive errors retrievable without parsing logs.
- Warnings for lossy import/export behavior.
- IDs identifying the failing document object.
- Timing and memory statistics for compilation and preview load.

## 8. Phased Delivery Plan

## Phase 0: Contract, scope, and benchmarks

Estimated effort: 1 week.

Status: completed on 2026-07-19. The contract, corpus, reference hardware,
thresholds, and recorded baseline are in
`docs/runtime-authoring-engine-contract.md`.

Engine work:

- Define the first supported paint and animation subset.
- Define native and managed ownership rules.
- Define transaction, revision, cancellation, and error contracts.
- Establish representative small, medium, and large documents.
- Define expected maximum layers, frames, strokes, and points.
- Establish latency and memory targets.

New testable functionality:

- No new production feature.
- A benchmark harness can report current export, player load, first-render, and memory behavior for representative documents.

Exit criteria:

- API contract and supported subset are documented.
- Benchmark corpus and target hardware are identified.
- Performance thresholds are explicit.

## Phase 1: Runtime-generated animation spike

Estimated effort: 1–3 weeks.

Status: completed on 2026-07-19. The runtime sample and benchmark harness prove
generation, file/memory export, playback, seeking, rendering, and the 100-cycle
lifecycle gate.

Engine work:

- Expose timeline, duration, and repeat settings currently hidden by the managed exporter wrapper.
- Create a multi-frame paint animation from runtime data.
- Export it and load it into `libImmPlayer`.
- Exercise playback and seeking.
- Repeat construction, export, load, and unload cycles.
- Record compilation, serialization, load, first-render, and memory measurements.

New testable functionality:

- A sample component can generate a multi-frame paint animation at runtime.
- The generated animation can be exported to IMM.
- The generated IMM can be played and scrubbed by the existing player.
- A diagnostic harness can repeat the cycle and report timing and memory.

Exit criteria:

- The generated animation plays correctly in the native renderer.
- At least 100 rebuild/load/unload cycles complete without accumulating documents or native memory.
- Measurements determine whether compile-and-reload is viable.

Decision gate:

- Retain compile-and-reload if it meets realistic latency targets.
- Add memory export and batch point upload, then remeasure if serialization or interop overhead dominates.
- Plan incremental player mutation only if the optimized path remains insufficient.

## Phase 2: Mutable runtime document API

Estimated effort: 3–5 weeks.

Status: completed on 2026-07-19. The public managed graph covers the supported
hierarchy, drawings, stable frame mappings, strokes, animation keys, snapshots,
validation, revisions, notifications, and lifecycle rules without application UI.

Engine work:

- Implement mutable documents with stable IDs.
- Implement group and paint-layer hierarchy operations.
- Implement drawing, frame, stroke, and batch-point operations.
- Implement read snapshots.
- Implement validation and structured errors.
- Implement revision tracking and change notifications.
- Add managed wrappers independent of application UI.

New testable functionality:

- Unity scripts can create, query, modify, reorder, and delete animation content after document creation.
- Scripts can replace stroke geometry and frame mappings without reconstructing their own authoring representation.
- Invalid edits return structured errors without corrupting the document.
- Observers receive revision and affected-object notifications after successful edits.

Exit criteria:

- All initial-scope content can be created and mutated through public APIs.
- Stable IDs survive unrelated insertions and reordering.
- Invalid hierarchy and dangling references are prevented.
- Managed and native lifecycle tests pass under repeated use.

## Phase 3: Transactions, compilation, and memory export

Estimated effort: 3–5 weeks.

Status: completed on 2026-07-19. Atomic expected-revision transactions,
deterministic snapshot compilation, batch point transfer, file/owned-memory
export, structured results, and supported cancellation paths are verified.

Engine work:

- Implement atomic edit transactions with expected-revision checks.
- Compile mutable documents deterministically into `libImmExporter` sequences.
- Add batch point transfer across the managed/native boundary.
- Add export-to-memory with explicit ownership.
- Return source revision, warnings, statistics, and structured failures.
- Add cancellation where exporter stages can support it safely.

New testable functionality:

- Scripts can apply a related group of edits atomically.
- Failed or stale transactions leave the document unchanged.
- Any valid document revision can be exported directly to memory or a file.
- Large strokes can be transferred without one P/Invoke call per point.
- Export results identify the exact revision that was compiled.

Exit criteria:

- Repeated compilation is deterministic for the supported subset.
- Exported memory buffers and files load successfully through the IMM importer/player.
- Buffer ownership and cancellation paths pass lifecycle tests.
- Compilation errors identify affected stable IDs.

## Phase 4: Runtime player-preview integration

Estimated effort: 2–4 weeks.

Status: completed on 2026-07-19. The preview coordinator's revision,
replacement, playback preservation, supersession, failure retention, and
lifecycle behavior are covered by the runtime suite and sample.

Engine work:

- Implement the preview coordinator.
- Compile requested revisions and load them through the player's memory-load API.
- Supersede obsolete requests safely.
- Preserve requested playback state, time, and document-to-world transform.
- Keep the last valid preview after a failed replacement.
- Report preview timings and state transitions.

New testable functionality:

- A script can request native playback for a specific mutable-document revision.
- After changing layers, frames, or strokes, a new authoritative preview can replace the old one.
- Rapid requests do not install stale previews.
- A failed preview build does not destroy the last working preview.
- Repeated preview replacements do not leak documents or buffers.

Exit criteria:

- Previewed revision is observable and matches the requested document revision.
- Playback state and requested time survive replacement within defined tolerances.
- Failure, cancellation, and supersession paths are covered by tests.

## Phase 5: Supported IMM paint import and round trip

Estimated effort: 4–8 weeks.

Status: completed on 2026-07-19 for the explicitly supported Windows x64 paint
subset defined in `docs/runtime-authoring-engine-contract.md`.

Engine work:

- Map stroke-reader output into the mutable runtime model.
- Import group/paint hierarchy, transforms, drawings, strokes, and frame mappings.
- Import supported animation keys.
- Detect unsupported or lossy source content.
- Add structural comparisons between source and exported documents.
- Define safe overwrite policy at the API level through lossiness status, leaving file-choice UX to the application.

New testable functionality:

- A script can open a supported paint-based IMM as a mutable document.
- Imported layers, frames, drawings, and strokes can be queried and changed through the same API as new content.
- The modified document can be exported and replayed.
- The import result reports whether unsupported content prevents a lossless round trip.

Exit criteria:

- Supported paint fixtures pass import, mutation, export, re-import, and playback tests.
- Unsupported content is reported before export.
- No lossless-round-trip claim is made for fields not explicitly tested.

Completion evidence:

- File and memory import map supported hierarchy, transforms, drawings,
  strokes, frames, and animation keys into the mutable authoring graph.
- Import results expose structured issues, statistics, lossiness, and a safe
  overwrite decision; unsupported-content coverage verifies that lossy imports
  cannot be treated as overwrite-safe.
- The mixed-visibility round-trip fixture covers every supported brush section,
  both supported stroke visibility modes, and every supported animation-key
  property. It passes import, stable-ID mutation, export, re-import, structural
  comparison, and native playback.
- Two final complete Windows Unity PlayMode runs passed 29 of 29 tests. The extended
  runtime sample also imported its generated IMM bytes in memory, modified 450
  strokes, one frame mapping, and one animation key by stable ID, installed
  revision 2 without a file, and rendered the changed result with zero
  structural differences.

## Phase 6: Production hardening and engine packaging

Estimated effort: 4–8 weeks.

Status: completed on 2026-07-19 for the Windows x64 paint-animation engine.
Runtime capability queries, operation progress, cooperative cancellation,
configurable safety limits, atomic file replacement, corrupt-input recovery,
soak/memory tests, package documentation, the minimal sample, and an
independent package-consumer assembly are implemented and verified. IMM is the
only engine persistence format, so the conditional snapshot/schema-migration
item does not apply.

Engine work:

- Add large-document progress and cancellation callbacks.
- Add bounded resource use and documented content limits.
- Harden corrupt-input and invalid-call handling.
- Add long-session and memory-pressure tests.
- Add snapshot/schema migration if engine-level persistence is required.
- Package runtime assemblies, native plugins, documentation, and minimal samples.
- Add platform and architecture capability queries.

New testable functionality:

- Long compilation/import operations expose progress and can be cancelled safely.
- Corrupt files and invalid operations return controlled failures.
- Capability APIs report which authoring and playback features are available on the current platform.
- Long-running edit/compile/preview loops remain within defined memory limits.
- A clean sample project demonstrates every supported engine operation without providing a product editor UI.

Exit criteria:

- Soak, malformed-input, cancellation, and recovery tests pass.
- Public API documentation includes ownership, threading, limits, and error behavior.
- The package can be consumed by the separate application without referencing sample code.

Recorded result: the final Windows Unity PlayMode suite passed 37 of 37 tests
in 2.061 seconds. The live sample reported all Phase 6 capability flags and
progress stages, exercised controlled limit/cancellation/corrupt-input
failures, and then rendered the revision-2 450-stroke animation after in-memory
import and stable-ID edits. `Assets/Phase6PackageConsumer` compiles as a separate
assembly referencing `ImmUnity.Runtime` only, and its reflection smoke test
passed.

## Phase 7: Extended IMM feature surface and platforms

Estimated effort: 3–7 additional engineer-months depending on selected increments.

Possible engine increments:

1. Complete animation-key support.
2. Spawn areas and viewpoints.
3. Pictures and 360-degree imagery.
4. Sound and spatial-audio properties.
5. Models.
6. References and instances.
7. Comic and chapter structures.
8. macOS, Android, and iOS authoring builds.

New testable functionality depends on the selected increment:

- Scripts can create, mutate, import, export, and preview the newly supported layer or animation type.
- Capability queries expose availability on the running platform.
- Round-trip and player-compatibility fixtures cover the new feature.

Each increment requires mutable model support, C ABI, managed bindings, import, export, validation, lifecycle tests, and player compatibility. Application UI remains outside this repository.

## 9. Incremental Player Mutation Decision

Incremental mutation of loaded playback data is not part of the baseline plan. It should be treated as a separate engine milestone only if Phase 1 and Phase 4 measurements show that optimized memory compile-and-reload cannot meet required latency.

Potential work would include:

- Stable native playback handles across edits.
- Thread-safe edit command queues.
- CPU geometry regeneration for changed drawings.
- Partial GPU buffer replacement and destruction.
- Bounds, visibility, and timeline cache invalidation.
- Synchronization with Unity render events.
- Transaction rollback after an update failure.

If required, expose queued transactions rather than raw player or importer pointers. The render thread should observe only committed revisions.

New testable functionality from this optional milestone would be immediate native-renderer updates after a stroke, frame, or property mutation without rebuilding the entire playback document.

## 10. Testing Strategy

### 10.1 Managed API tests

- Stable ID allocation.
- Layer insertion, removal, reparenting, reordering, and cycle prevention.
- Drawing and frame-reference operations.
- Stroke creation, replacement, cloning, reordering, and deletion.
- Keyframe validation.
- Transactions, stale revisions, commit, and abort.
- Snapshots and change notifications.
- Managed disposal behavior.

### 10.2 Native ABI tests

- Null, stale, wrong-type, and wrong-owner handles.
- Invalid indices and enums.
- Empty and large strokes.
- Batch point upload and retrieval.
- Repeated document construction and destruction.
- Export failure and cancellation.
- Memory buffer ownership and release.

### 10.3 Compatibility tests

- Build known mutable-document fixtures.
- Export each fixture to memory and file.
- Read it through the supported importer.
- Load and play it through `libImmPlayer`.
- Compare hierarchy, drawings, frames, strokes, keys, and properties within defined tolerances.
- Render selected deterministic frames where stable reference comparison is practical.

### 10.4 Performance tests

Measure separately:

- Mutation and transaction time.
- Snapshot creation time.
- Managed/native point-transfer time.
- Exporter graph-construction time.
- Serialization time and output size.
- Player load-to-ready time.
- First-rendered-frame time.
- Peak managed and native memory.
- Memory after repeated compile and preview cycles.

Use document sizes established in Phase 0 rather than extrapolating from the existing small exporter samples.

### 10.5 Runtime soak tests

- Repeated mutate/compile cycles.
- Repeated transaction abort and commit.
- Repeated frame-map changes.
- Repeated preview replacement.
- Repeated import/export/re-import.
- Concurrent read snapshots while edits are prepared.
- Extended playback while newer revisions are compiled.

## 11. Diagnostics

Use distinct searchable prefixes for engine subsystems:

- `[IMM_AUTHOR_MODEL]`
- `[IMM_AUTHOR_EDIT]`
- `[IMM_AUTHOR_COMPILE]`
- `[IMM_AUTHOR_PREVIEW]`
- `[IMM_AUTHOR_EXPORT]`
- `[IMM_AUTHOR_IMPORT]`

Compilation and preview logs should include document ID, source revision, installed preview revision, elapsed time, byte size, and result code. Avoid per-point logging during normal operation.

Expected validation failures must be returned as structured results. Logs are for internal failures and lifecycle diagnostics, not the primary application error interface.

## 12. Risks and Mitigations

### Compile-and-reload preview is too slow

- Measure realistic content first.
- Export directly to memory.
- Batch point transfer.
- Allow applications to control preview request timing.
- Add incremental player mutation only if measurements require it.

### Exporter and player interpret animation differently

- Treat player playback as authoritative.
- Add compile/import/play fixtures early.
- Test frame mapping, timing, interpolation, actions, and repeat counts explicitly.

### Existing files contain unsupported data

- Return explicit lossiness status from import and export validation.
- Preserve source files; the application chooses overwrite policy.
- Do not claim general lossless round trip until every relevant field is covered.

### Managed/native lifetime defects

- Define ownership in the public contract.
- Use safe managed wrappers and deterministic disposal.
- Validate handle types and owners in the C ABI.
- Test failure and repeated-lifecycle paths.

### Application behavior leaks into the engine

- Keep APIs UI-framework and input-system independent.
- Expose mechanisms such as revisions, transactions, progress, and cancellation rather than product policies.
- Keep samples minimal and API-focused.

### Platform scope expands prematurely

- Complete Windows authoring first.
- Keep managed contracts platform-neutral.
- Expose capabilities so consuming applications can degrade cleanly.
- Treat each additional native platform as a separately validated increment.

## 13. Effort Summary

Estimates assume one developer familiar with Unity, C#, C++, native plugin boundaries, and the IMM codebase. They cover engine and SDK work only, not the separate application UI.

| Engine deliverable | Estimated effort |
|---|---:|
| Runtime generation and playback spike | 1–3 weeks |
| Mutable paint-animation API and deterministic export | 7–12 weeks cumulative |
| Runtime preview integration and supported paint import | 3–5 months cumulative |
| Production-ready Windows paint-animation engine | 5–7 months cumulative |
| Broad IMM authoring engine and additional platforms | 8–14 engineer-months cumulative |

Some native API, managed model, compilation, and test work can overlap after the contracts are stable.

## 14. Recommended First Commitment

Commit initially to Phases 0 and 1. The first concrete deliverable should be a minimal Unity sample and automated harness that:

1. Constructs a multi-frame paint animation from runtime data.
2. Exports it through the existing native exporter.
3. Loads it through the existing native player.
4. Plays and seeks the animation.
5. Repeats construction, export, load, and unload while recording latency and memory.

This provides the evidence needed to choose between the lower-risk compile-and-reload architecture and a larger incremental-player-mutation workstream. It also establishes an engine-only vertical slice without committing this repository to any application UI.
