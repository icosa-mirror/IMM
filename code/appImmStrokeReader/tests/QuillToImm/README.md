# Static Quill to IMM conversion checks

The .NET 8 harness compiles the production reader, converter, authoring graph,
compiler and native declarations. Small Unity data stand-ins replace engine types;
the real Windows exporter and stroke-reader DLLs perform encode/read-back. This
does not replace Unity compilation or visual playback acceptance.

1. Run the synthetic native regression suite from the repository root. Use a fresh
   artifact directory for each local run; Windows CI runs this with freshly built DLLs:

   ```powershell
   dotnet run --project code/appImmStrokeReader/tests/QuillToImm -- --self-test code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64/ImmUnityPlugin.dll code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll artifacts/quill-converter-regressions
   ```

2. Convert a private project using the same runtime API. The output parent directory
   must exist. Omit `--paint-only` to reject all omitted media/viewpoints/unknown layers:

   ```powershell
   dotnet run --project code/appImmStrokeReader/tests/QuillToImm -- $projectFolder $outputImm code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64/ImmUnityPlugin.dll code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll --paint-only
   ```

3. Compare the decoded paint/group subset with a local reference export. This includes
   every stored stroke/point, rather than skipping source strokes affected by codec
   point removal. Viewpoints, media, root keys and visual playback are outside this check:

   ```powershell
   dotnet run --project code/appImmStrokeReader/tests/QuillToImm -- --compare-paint $referenceImm $outputImm code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll
   ```

The suite covers native brush/visibility mapping, nonidentity group transform/pivot,
hierarchy, zero-width stroke tips, invalid zero-extent/negative-width strokes,
string-valued held frames,
reflection and animation rejection, unknown source layers/brushes, explicit partial
output, source-file overwrite protection and preservation of existing output on failure.
The no-argument `QBinReader.ReadDrawingData()` brush fallback remains unchanged;
conversion explicitly requests strict brush validation.

Private projects and generated artifacts remain local. Console prefixes are
`[IMM_QUILL_CONVERTER]`, `[IMM_QUILL_CONVERTER_TEST]` and
`[IMM_QUILL_CONVERTER_COMPARE]`.
