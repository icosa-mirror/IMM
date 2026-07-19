# IMM runtime authoring engine contract

This contract records the implemented Phase 0-6 boundary for the first Windows
paint-animation authoring release. It describes engine behavior; application UI
and input policy are intentionally absent.

## Supported content

- Windows x64 Unity Editor and standalone player.
- Still and animated sequences at a document-wide integer frame rate.
- Nested group and paint layers.
- Layer name, local transform, pivot, visibility, opacity, timeline duration,
  and repeat count.
- Paint drawings, ordered stroke elements, and frame-to-drawing mappings.
- Segment, circle, ellipse, and square brush sections. The native IMM v2
  exporter rejects point brush sections, so the managed model rejects them
  before native work begins.
- Always-visible and quadratic-fade stroke visibility.
- Position, normal, RGB, alpha, and width per point. View direction is preserved
  for quadratic-fade strokes; the IMM binary codec omits it for always-visible
  strokes. Point length and time are derived by the codec and are not claimed as
  arbitrary lossless stored attributes.
- Visibility, opacity, transform, draw-in-time, action, loop, and offset layer
  animation keys. Legacy position, rotation, and scale keys are rejected in
  favour of the IMM v2 transform key.

Pictures, sound, models, effects, references, instances, spawn areas, comic
chapters, root/comic animation, and cross-platform authoring are outside the
Phase 0-6 contract.

## Identity and ownership

- Every authoring document has a stable 64-bit ID and monotonically increasing
  64-bit revision.
- Layers, drawings, frame mappings, strokes, and animation keys have stable
  positive 64-bit IDs unique within their document. IDs are never reused during
  a document's lifetime. A frame's list index may change while its ID remains
  stable.
- A document owns its complete hierarchy. A layer owns its child order. A paint
  layer owns its drawings and frame mappings. A drawing owns its strokes. A
  stroke owns its point buffer.
- Removing an owner removes its descendants. References to removed IDs return a
  structured `NotFound` result and never address a replacement object.
- Public managed objects do not expose native pointers. Exporter handles are
  compilation details with deterministic lifetime.

## Supported import and round trip

- `ImmAuthoringImporter.ImportFromFile` and `ImportFromMemory` load supported
  sequence metadata, nested group/paint hierarchy, local transforms, pivots,
  layer properties, drawings, ordered strokes, frame mappings, and supported
  animation keys into `ImmAuthoringDocument`.
- Imported content uses the same query, mutation, transaction, snapshot,
  compilation, and preview APIs as newly created content.
- A successful caller owns the returned document and must dispose it.
- Every import reports `Lossiness`, structured `Issues`, statistics, and
  `CanOverwriteSource`. Safe overwrite is true only for a successful import
  with no issue that omitted, repaired, reparented, or reinterpreted content.
- Unsupported sequence/layer/brush/property data, root animation, capability
  bits, transform flip, invalid values, reparenting, and frame-rate changes are
  reported explicitly. Unsupported content is not silently treated as a
  lossless round trip.
- `ImmAuthoringStructuralComparer` compares the represented hierarchy,
  ordering, layer properties, animation keys, frame mappings, strokes, and
  points with an explicit floating-point tolerance and the binary-codec
  exceptions listed above.

## Mutation and revisions

- Successful mutations increment the revision exactly once.
- Rejected mutations do not change content or revision.
- A transaction groups mutations into one atomic commit and one revision.
- Transactions specify an expected base revision. Committing against any other
  revision returns `RevisionConflict` without changing the document.
- Disposing an uncommitted transaction aborts it.
- Change notifications are emitted after commit and identify the new revision
  and affected stable IDs. Application-level undo is built above transactions.

## Validation and errors

Public operations return a typed result with a stable error code, message, and,
where applicable, the responsible object ID. Initial error codes cover invalid
arguments, missing objects, invalid ownership, hierarchy cycles, invalid frame
or drawing references, revision conflicts, disposed objects, unsupported
features, native export failure, cancellation, resource-limit exhaustion, and
corrupt input.

Validation checks at least:

- Acyclic hierarchy with one root-owned path per layer.
- Finite transforms and point attributes.
- Opacity and alpha in the range 0-1.
- Positive frame rate, width, and point counts where required.
- Frame mappings that reference drawings in the same paint layer.
- Non-empty names and valid enum values.

## Threading and cancellation

- Mutable document operations are synchronized and may be called from managed
  worker threads unless a method is explicitly documented as Unity-main-thread
  only.
- A transaction has one writer. Read snapshots are immutable and can be used on
  other threads.
- Native compilation and export may run off the Unity main thread after a
  snapshot has been captured.
- Cancellation is cooperative between compilation stages. Cancellation returns
  no partial public buffer and leaves the source document unchanged.
- `ImmAuthoringOperationOptions` supplies cancellation, progress, and content
  limits to compilation and import. Callbacks run on the operation's thread.
- Preview requests expose the latest thread-safe compiler progress value for
  polling from an application UI.

## Serialization and buffers

- File and memory exports are deterministic for the same validated snapshot and
  exporter version.
- Export results carry the source revision, bytes written, elapsed time,
  warnings, and a typed status.
- Managed memory exports return an owned `byte[]`. The native temporary buffer is
  copied and released before the call returns.
- The player must retain its own input allocation for asynchronous memory loads;
  this is handled by `ImmPlayerManager.LoadDocumentFromMemory`.

## Benchmark corpus and initial thresholds

The runtime sample defines three synthetic cases:

| Case | Layers | Strokes/layer | Points/stroke | Frames |
|---|---:|---:|---:|---:|
| Small | 1 | 10 | 16 | 12 |
| Medium | 4 | 100 | 32 | 120 |
| Large | 8 | 500 | 64 | 300 |

The large case is the Phase 0-3 planning envelope: 8 authored layers, 300 frame
mappings per paint layer, 4,000 strokes, and 256,000 points. It is a benchmark
envelope rather than a serialized-format limit; larger documents must return a
measured result and must not rely on undefined behavior.

Phase 6 adds explicit default safety limits: 256 MiB input and output, 4,096
layers, hierarchy depth 64, 65,536 drawings, 1,000,000 strokes, 1,000,000
points per stroke, 16,000,000 total points, 1,000,000 frame mappings,
1,000,000 animation keys, and 1,024 characters per layer name. Products can
provide smaller positive limits per operation. Exceeding a configured limit
returns `ResourceLimitExceeded` without a partial public document or buffer.

The Phase 0 reference machine is Windows 11 x64, Unity 2022.3.62f2, Direct3D 11,
an AMD Ryzen 7 7800X3D (8 cores/16 logical processors), 32 GB system memory, and
an NVIDIA GeForce RTX 4090 with 24 GB video memory. Initial engineering targets
on that machine are:

| Case | Compile + serialize | Player load-to-ready | First rendered frame |
|---|---:|---:|---:|
| Small | p95 <= 100 ms | p95 <= 250 ms | p95 <= 33.4 ms |
| Medium | <= 1 s | <= 1 s | <= 50 ms |
| Large | <= 10 s | <= 5 s | <= 100 ms |

The report must separate managed construction, graph compilation, serialization,
player load-to-ready, first rendered frame, output bytes, managed memory delta,
and native process working-set delta. Phase 1's minimum lifecycle gate is 100
small-document export/load/unload cycles. The live-document and retained input
allocation counts must return to baseline after every cycle. After a forced
collection, retained managed memory must be within 8 MiB of baseline and process
working set within 64 MiB of baseline. A monotonic increase over repeated runs is
a failure even when it remains below those allowances.

## Recorded Phase 0-1 baseline and decision

The benchmark harness was run in the Unity Editor on the reference machine on
2026-07-19. These Phase 0 figures use the legacy file exporter and measure
player load until `IsSequenceReady`; the Phase 1 lifecycle test separately waits
for the player `Loaded` state before inspecting or releasing a document.

| Case | Construction | File export | Load to sequence-ready | Next rendered frame | Bytes |
|---|---:|---:|---:|---:|---:|
| Small | 1.298 ms | 1.910 ms | 6.029 ms | 3.799 ms | 3,161 |
| Medium | 4.444 ms | 48.349 ms | 58.868 ms | 1.983 ms | 94,103 |
| Large | 80.060 ms | 466.323 ms | 202.166 ms | 2.245 ms | 984,363 |

The optimized Phase 1 memory path completed 100 construct, compile, serialize,
load-to-`Loaded`, seek-capable playback, unload, and input-buffer release cycles
in 1.83 seconds. Retained managed memory was 2,224,128 bytes and retained process
working set was zero bytes in that isolated run; every cycle returned native
document and input-buffer counts to zero. A subsequent complete-suite run
reported 1,376,256 retained managed bytes and zero retained working-set bytes.

Decision: retain compile-and-reload through Phase 3. The representative corpus
is inside the initial latency and memory gates, batch point transfer removes the
per-point interop path from the mutable compiler, and owned memory export removes
temporary-file serialization from runtime preview. Incremental live mutation is
therefore deferred to Phase 4 measurement rather than required by Phases 0-3.
These are development gates, not cross-hardware release guarantees. Phase 4 may
revise them using product target hardware and representative application data.
No cache or incremental player-mutation design is justified solely by the
four-point legacy sample.

## Recorded Phase 4 verification

Phase 4 retains compile-and-reload and installs previews through
`ImmAuthoringPreviewCoordinator`. The coordinator compiles immutable snapshots
off the Unity main thread, loads owned memory through the player, and does not
mark a replacement installed until its requested playback settings have had a
Unity frame to reach the native state machine.

The Windows Unity PlayMode suite was run on the reference machine on 2026-07-19.
All 21 tests passed in 1.96 seconds. The suite includes:

- Exact-revision request validation and observable installed revisions.
- Playback state, time, and document-to-world preservation during replacement.
- Cancellation, supersession, and reentrant terminal-state listeners.
- Failed replacement while retaining the last valid native preview.
- 25 sequential authoritative preview replacements returning managed document
  and input-buffer ownership to baseline.
- The Phase 1 100-cycle compile, memory-load, playback, and unload gate.

The native Windows plugin was rebuilt from the same source and the exporter C
ABI smoke test exported and validated its fixture. Runtime preview replacement
therefore remains on the measured memory compile-and-reload path; incremental
mutation of an already loaded player document remains deferred.

## Recorded Phase 5 verification

Phase 5 maps the stroke-reader's supported paint data into the mutable managed
graph. File and owned-memory import return a document, lossiness decision,
structured issues, safe-overwrite decision, and import statistics. The returned
document can be queried and edited by stable ID, compiled, re-imported,
structurally compared, and installed through the Phase 4 preview coordinator.

The Windows native plugins were rebuilt from the same source on 2026-07-19. A
regression fixture mixes always-visible and quadratic-fade strokes inside each
drawing; this covers the binary direction channel, which stores values only for
directional strokes. The fixture also covers all four supported brush sections,
both supported stroke visibility modes, repeated frame-to-drawing mappings,
nested group/paint layers, layer and pivot transforms, and these animation-key
properties: visibility, opacity, transform, draw-in-time, action, loop, and
offset. The explicitly exercised interpolation set is none, linear, smoothstep,
and ease-in.

The focused import/mutate/re-export/re-import/playback test passed, followed by
two final complete Windows Unity PlayMode runs: 29 of 29 tests passed in 2.001
seconds and 29 of 29 tests passed in 1.970 seconds. Unsupported content is separately
verified to make the import lossy and `CanOverwriteSource` false. Point brushes
and obsolete component transform keys are rejected before native export.

The runtime sample then generated 164,811 IMM bytes in memory, imported them as
a lossless mutable revision with 450 strokes and zero structural differences,
installed that graph, changed all 450 strokes plus one frame mapping and an
opacity animation key by stable ID, and installed revision 2 from memory without
writing a file. The final live state reported authoring revision 2, installed
revision 2, one committed modification, lossless import, safe overwrite, and
zero structural differences. The Game view rendered the resulting multicolour
deformed ribbon, and the Unity console contained no errors.

## Recorded Phase 6 verification

Phase 6 packages capability detection, immutable operation limits, progress,
cooperative cancellation, atomic file replacement, controlled corrupt-input
handling, public production documentation, and a minimal independent consumer
assembly. The package has no second authoring persistence format: IMM remains
the engine persistence boundary, so snapshot-schema migration is not required.

The Windows Unity PlayMode production suite exercises progress stages, output
and graph limits, pre-cancelled memory/file operations, atomic preservation of
an existing destination, mid-graph cancellation after at least 100 compiled
units, retry recovery, malformed memory, import limits, import cancellation,
100 edit/export/import/dispose cycles, managed-memory retention, capability
queries, 100 native preview replacements returning ownership to baseline, and
an application assembly that references only `ImmUnity.Runtime`.
Two final complete runs passed 37 of 37 tests in 3.097 seconds and 3.094
seconds respectively.

The Phase 6 sample reported Windows x64 playback, mutable graph, memory/file
export, paint import, preview, progress, and cancellation capabilities. It
reported validation, compilation, serialization, source inspection, and graph
import stages; returned the expected `ResourceLimitExceeded`, `Cancelled`, and
`CorruptInput` failures; then installed revision 2 after editing 450 strokes,
one frame mapping, and one animation key entirely in memory. The Game view
rendered the multicolour procedural ribbon. No new error or exception was
written during that run.
