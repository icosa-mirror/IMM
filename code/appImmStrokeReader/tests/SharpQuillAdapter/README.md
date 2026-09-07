# SharpQuill adapter regression

This executable compiles the production SharpQuill adapter, managed native
declarations, and SharpQuill model against .NET 8. It calls the real Windows
stroke-reader library without starting Unity. Unity logging and the wrapper's
unused convenience types have small stand-ins in `UnityTypes.cs`.

Run from the repository root after building the Windows stroke reader:

```powershell
dotnet run --project code/appImmStrokeReader/tests/SharpQuillAdapter/SharpQuillAdapter.csproj -- code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll exampleImmFiles/sample1.imm artifacts/sharpquill-adapter-native.log
```

The native library needs its dependency DLLs alongside it. Create the log's parent
directory before running. The Windows build CI job runs this check after building
the reader.

The fixture contains paint, pictures, and spawn areas. Both picture-inclusion
settings must preserve the native paint count; spawn areas must not become empty
paint layers. Before the correction, the test reports 34 paint layers instead of
30. With the correction it reports 30 paint layers and either zero or one picture.

`AdapterSource` can point to a previous adapter source file when checking that
the regression detects the original behavior. Keep the managed declarations and
native library paired; this check does not require an older native ABI.

## Package update comparison (2026-09-07)

The installed UPM revision `5e6ee762c5e3f7c81b0e73b512228307d79be5da`
was compared with the native Windows package built by CI run `34151461579`
at `e357df162ab274af7a9e516587d320ac59ac177d`. The adapter correction changes
managed conversion only; the native ABI remains unchanged.

| Workload | Compared strokes | Compared points | Pictures |
| --- | ---: | ---: | ---: |
| sample1.imm | 1,171 | 58,405 | 1 |
| Multi-chapter workload | 129,585 | 3,335,073 | 555 |
| Heavy workload | 11,438,946 | 98,690,016 | 15 |

Counts include first, middle, and last chapters where available and all drawings
in those selected chapters. Each reader ran in a separate process and passed
load/unload/reload checks. Exact per-drawing hashes matched for stroke metadata
and point positions, normals, colors, alpha, and widths. Common layer metadata,
local/world transforms, animation frame buffers, chapter drawing selection, and
picture metadata/pixels also matched.

The observed differences were additional spawn layers and picture repeat-count
metadata (zero versus one), which picture conversion does not consume. Point
view-direction components were excluded from the comparison because Open Brush
reconstructs tangents from positions instead of consuming them. This is a data
compatibility result for the selected workloads, not an exhaustive equivalence
claim for every IMM file.

Native DLL SHA-256 values:

1. Installed: `cf65a82dabe0ad33866c114902738ebb3ba51d070645814c120d60d75aa4b2b3`
2. Candidate: `fd232c242a46bf42b8bb48d0006e814520ea2a8d06e17a0f4a88f004fdd35ed4`
