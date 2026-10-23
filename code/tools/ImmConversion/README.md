# Standalone IMM conversion library

`ImmConversion` is the .NET 8 Windows-host build of the shared Quill reader,
converter, offline authoring graph/compiler and native declarations. It compiles
the existing Unity package sources directly; mapping and serialization rules are
not copied into a separate implementation. It does not need a Unity installation.

The Unity packages continue compiling those sources against real Unity types.
This project's `UnityTypes.cs` provides the compatibility definitions needed by
the standalone host. Do not import this standalone assembly into Unity alongside
the package: Unity consumes the shared package sources instead.

The native exporter and reader DLLs and their codec dependencies remain required
for native operations. This project does not yet bundle those dependencies or
provide the planned desktop interface, production CLI or self-contained download.
Its extraction is a packaging prerequisite, not a completed distribution or a
complete Unity-compatible SDK. The current standalone target is Windows.

Build from the repository root:

```powershell
dotnet build code/tools/ImmConversion/ImmConversion.csproj
```

The `code/appImmStrokeReader/tests/QuillToImm` regression harness references this
assembly, rather than recompiling an independent source list. Its existing real
native converter/import/export checks therefore exercise the extracted library.
Only that regression assembly receives internal access for codec/ABI checks.
The simulated Unity editor menu remains in the test assembly, outside the library.

User-facing packaging and clean-machine/Unity-consumer acceptance remain tracked
under C5 in `LLM Docs/IMM_CONTENT_PIPELINE_PLAN.md`.
