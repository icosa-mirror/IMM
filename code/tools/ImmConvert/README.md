# Quill to IMM command line

`imm-convert` converts a Quill project folder containing `Quill.json` and
`Quill.qbin` into an IMM file, without Unity. It uses the same project converter
as the Unity package. Unsupported content is rejected unless omissions are
explicitly permitted; packaging does not expand the converter's fidelity claims.

```powershell
imm-convert "C:\Art\My Quill Project" "C:\Exports\My Scene.imm" --report "C:\Exports\My Scene.json"
```

Existing IMM output requires `--overwrite`. Conversion goes into a private file
and is published only on success. Without `--overwrite`, a destination created
by another process during conversion is preserved too. Reports must be separate
from the IMM output and the two Quill source files. An existing report at the
explicitly chosen report path is replaced atomically.

Use `--allow-omissions` only when partial output is intended. The console says
"Converted with omissions" and the JSON report sets `partial: true`, with the
individual issues. Explicitly allowed partial conversion exits successfully.
The default input limit is 256 MiB; `--input-mib N` changes it. Ctrl+C requests
cooperative cancellation at the existing converter/compiler cancellation points.

| Exit code | Meaning |
|---|---|
| 0 | Conversion published, including explicitly permitted partial output |
| 1 | Conversion rejected; output was not replaced |
| 2 | Invalid arguments or conflicting paths; no report is written |
| 3 | Runtime, publication or report-writing error |
| 130 | Cancelled |

JSON reports use schema `imm-quill-conversion-report-v1`. They include tool
version, source/output paths, timestamps, exit code, `succeeded`,
`outputPublished`, `partial`, published byte count and individual issues. A
report-writing failure can return 3 after an IMM was published; console output
identifies the report error. Publication and report writing are separate file
operations, not one multi-file transaction. Progress/diagnostics go to stderr;
the successful output summary goes to stdout.

## Current build and native dependencies

This is the production CLI entry point. Development builds require Windows x64,
.NET 8 and the native runtime files. `package-windows.ps1` now creates a prototype
self-contained ZIP with the desktop window (`QuillToImm.exe`), CLI, native and
app-local Microsoft CRT files and notices.
The extracted app is tested with global .NET discovery disabled and its actual
local CLR/CRT/native module locations checked. The desktop form probe loads its
production UI/host/WinForms/runtime payload from that same archive. This is not a
clean Windows VM or live mouse/keyboard test. Clean-machine, live input and dependency licence documentation
remain C5 work in `LLM Docs/IMM_CONTENT_PIPELINE_PLAN.md`.

Build from the repository root:

```powershell
dotnet build code/tools/ImmConvert/ImmConvert.csproj --configuration Release
```

Build the portable CLI archive into a new output directory:

```powershell
pwsh -NoProfile -File code/tools/ImmConvert/package-windows.ps1 -OutputDirectory artifacts/imm-convert-portable
```

The packager uses Visual Studio's redistributable CRT directory (or an explicit
`-CrtDirectory`), not Windows system DLLs. It preserves .NET/NuGet and native
dependency notices and records source revision, dirty-tree state and file hashes.
An existing output directory is rejected to preserve previous artifacts. The
prototype's dependency licence documentation check is explicitly incomplete; see
`THIRD_PARTY.md`. No GitHub release is created by this script.

Place the complete native runtime alongside the application, or use
`--native-directory <directory>` during development. The CLI checks for missing
files before native conversion. The complete set is:

1. `ImmUnityPlugin.dll`, `ImmStrokeReader.dll`, `Audio360.dll`.
2. `opusenc.dll`, `vorbisenc.dll`.
3. `zlib1.dll`, `jpeg62.dll`, `libpng16.dll`, `ogg.dll`, `vorbis.dll`, `opus.dll`.

These files currently come from the two Unity packages' `Plugins/x86_64`
directories. No Unity Editor or Python process participates in conversion.
Resolving native files uses the application/explicit native directory, not the
process working directory.

When a JSON report is requested, its `runtime` section records the actual CLR
directory and selected loaded native module paths. Module-inspection errors are
reported without turning a successful conversion into a failure.

The required Windows CI step builds this CLI, runs the real-filesystem atomic
publication checks and exercises actual native conversion from an unrelated
working directory, including Unicode/spaced paths, missing dependencies,
overwrite/source protection, failure reports and explicit partial conversion.
