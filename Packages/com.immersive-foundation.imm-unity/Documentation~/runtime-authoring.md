# Runtime authoring engine

The `ImmPlayer.Authoring` API is the engine boundary for procedural and imported
paint animation. It contains no editor-window, tool, selection, or application
UI policy.

## Platform capabilities

Read `ImmAuthoringRuntime.Capabilities` before exposing an operation. The first
authoring release supports Windows x64 Editor and standalone player. Playback
plugins are packaged for the existing additional targets, but their presence
does not imply that authoring is supported there. `Supports` accepts combined
`ImmAuthoringFeature` flags and returns true only when every requested feature
is present.

## Ownership and lifetime

- A successful `ImmAuthoringDocument.Create` or importer result transfers
  ownership of the document to the caller. Dispose it exactly once.
- Snapshots are immutable managed values and do not own native resources.
- Memory export returns an owned `byte[]`; native temporary memory has already
  been released.
- `ImmAuthoringPreviewCoordinator` owns its installed `ImmDocument`. Use
  `ClearPreview` or destroy the coordinator to unload it. Do not unload its
  `InstalledDocument` separately.
- The player retains memory-load input until asynchronous loading finishes or
  the document is unloaded.

## Threading

Mutable document operations are synchronized. A transaction has one writer.
Snapshots can be read on any managed thread. Compilation and import can run on
a worker thread. Progress callbacks run on the thread performing the operation;
they must be short, non-blocking, and must marshal Unity object access to the
main thread. Native player installation and all coordinator component methods
are Unity-main-thread operations.

## Progress and cancellation

Pass `ImmAuthoringOperationOptions` to compiler or importer overloads. Progress
reports contain a stage, completed units, total units, fraction, and message.
File export reports `WritingOutput` after serialization and does not report
`Completed` until the destination has been replaced.
Cancellation is cooperative at validation, source-inspection, layer, drawing,
stroke, frame, and serialization boundaries. A native parser or serializer call
already in progress cannot be interrupted; cancellation is observed immediately
after it returns. A cancelled memory operation exposes no partial buffer or
document. File export writes a same-directory temporary file and only replaces
the destination after successful serialization and the final cancellation
checkpoint.

## Default content limits

| Resource | Default |
|---|---:|
| Input IMM bytes | 256 MiB |
| Output IMM bytes | 256 MiB |
| Layers | 4,096 |
| Hierarchy depth | 64 |
| Drawings | 65,536 |
| Strokes | 1,000,000 |
| Points per stroke | 1,000,000 |
| Total points | 16,000,000 |
| Frame mappings | 1,000,000 |
| Animation keys | 1,000,000 |
| Layer-name characters | 1,024 |

These are safety envelopes, not IMM format maxima. Supply a custom positive
`ImmAuthoringLimits` instance when a product needs a smaller budget. Import
checks byte size before native parsing and counts before large managed graph
allocations where the source API exposes them. Export validates the immutable
snapshot before native graph creation and rejects oversized output before
returning it.

## Errors and recovery

Public failures carry an `ImmAuthoringErrorCode`, message, and optional object
ID. `ResourceLimitExceeded`, `CorruptInput`, and `Cancelled` are expected
controlled outcomes. Invalid arguments and unsupported IMM features are also
returned rather than logged or thrown. Constructor misuse for limits can throw
`ArgumentOutOfRangeException`; operating-system failures during file export are
converted to `NativeExportFailed`.

A failed import owns no document. A failed or cancelled export returns no data
and does not mutate its source. A failed preview replacement leaves the last
installed preview intact. Callers can retry with the same valid source after a
failure.

## Persistence and schema

There is no second engine-level authoring persistence schema in Phase 6.
Persistent interchange uses IMM through the documented import/export subset,
so no snapshot migration API is required. Stable IDs are runtime document
identity and are not promised to survive an IMM round trip.
