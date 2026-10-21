# Runtime authoring engine

The `ImmPlayer.Authoring` API is the engine boundary for procedural and imported
paint animation. It contains no editor-window, tool, selection, or application
UI policy.

## Platform capabilities

Read `ImmAuthoringRuntime.Capabilities` before exposing an operation. The first
authoring release began on Windows; native offline exporters are now built for
Windows, macOS, Android arm64 and iOS. Capability flags alone do not establish
live editing or backend acceptance. `Supports` accepts combined
`ImmAuthoringFeature` flags and returns true only when every requested feature
is present.

## Quill static paint conversion

`QuillToImmConverter.ConvertStaticPaintProject(projectFolder, outputPath)` maps
static paint/groups through the authoring graph and compiler. The output directory
must exist. Default mode rejects viewpoints, media and unknown source layers.
Call with `allowOmittedLayers: true` only when an explicitly partial paint subset
is wanted; inspect `IsPartial` and every `Issues` entry before accepting the output.

1. Preserves four native brush sections, rotational-opacity flags, local transforms,
   pivots, static properties and held references to a single drawing. Hidden paint
   is excluded through static ancestor visibility, with a diagnostic.
2. Reflections, animated timing/keys, active keep-alive and nonidentity root properties
   are unsupported. Navigation and media conversion are not part of this profile.
3. Zero-width tips are valid. Widths must be nonnegative and a stroke must have
   nonzero spatial or width extent. The native codec computes bounds and quantizes
   values; the converter does not clamp widths or add another simplifier.
4. Reuses atomic file export. Preflight/mapping failure leaves existing output
   unchanged and source files cannot be selected as the output.

The native .NET regression harness is under `code/appImmStrokeReader/tests/QuillToImm`
in the repository. Data comparison does not replace Unity/visual playback acceptance.

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
