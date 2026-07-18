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

The large case is the Phase 0-3 planning envelope: 8 authored layers, 300 frame
mappings per paint layer, 4,000 strokes, and 256,000 points. It is a benchmark
envelope rather than a serialized-format limit; larger documents must return a
measured result and must not rely on undefined behavior.

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
