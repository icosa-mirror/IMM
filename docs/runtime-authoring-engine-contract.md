# IMM runtime authoring engine contract

This contract fixes the Phase 0 boundary for the first Windows paint-animation
authoring release. It describes engine behavior; application UI and input policy
are intentionally absent.

## Supported content

- Windows x64 Unity Editor and standalone player.
- Still and animated sequences at a document-wide integer frame rate.
- Nested group and paint layers.
- Layer name, local transform, pivot, visibility, opacity, timeline duration,
  and repeat count.
- Paint drawings, ordered stroke elements, and frame-to-drawing mappings.
- Point, segment, circle, ellipse, and square brush sections.
- Always-visible and quadratic-fade stroke visibility.
- Position, normal, direction, RGB, alpha, width, length, and time per point.

Pictures, sound, models, effects, references, instances, spawn areas, comic
chapters, and cross-platform authoring are outside the Phase 0-3 contract.

## Identity and ownership

- Every authoring document has a stable 64-bit ID and monotonically increasing
  64-bit revision.
- Layers, drawings, and strokes have stable positive 64-bit IDs unique within
  their document. IDs are never reused during a document's lifetime.
- A document owns its complete hierarchy. A layer owns its child order. A paint
  layer owns its drawings and frame mappings. A drawing owns its strokes. A
  stroke owns its point buffer.
- Removing an owner removes its descendants. References to removed IDs return a
  structured `NotFound` result and never address a replacement object.
- Public managed objects do not expose native pointers. Exporter handles are
  compilation details with deterministic lifetime.

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
features, native export failure, and cancellation.

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

Phase 1 records rather than assumes thresholds. The report must separate managed
construction, native export, player load-to-ready, first rendered frame, output
bytes, managed memory delta, and native process working-set delta. Phase 1's
minimum lifecycle gate is 100 small-document export/load/unload cycles without a
monotonically growing live-document count or unmanaged input allocation count.

Measured results will set release limits before Phase 4. No cache or incremental
player-mutation design is justified solely by the four-point legacy sample.
