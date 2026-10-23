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
provide the planned desktop interface or self-contained download. The production
CLI entry point now lives in `code/tools/ImmConvert` and references this library.
Its extraction is a packaging prerequisite, not a completed distribution or a
complete Unity-compatible SDK. The current standalone target is Windows.

Build from the repository root:

```powershell
dotnet build code/tools/ImmConversion/ImmConversion.csproj
```

The `code/appImmStrokeReader/tests/QuillToImm` regression harness references this
assembly, rather than recompiling an independent source list. Its existing real
native converter/import/export checks therefore exercise the extracted library.
That regression assembly receives internal access for codec/ABI checks; the
focused `ImmConvertTests` assembly also exercises the shared publication helper.
The simulated Unity editor menu remains in the test assembly, outside the library.

`StandaloneConversion` is the .NET-only application service used by the CLI and
desktop window. It owns native initialization, one active native conversion per
process, cooperative cancellation, output/report protection and diagnostics.
Repeated conversions reuse the same configured native directory; changing it
requires a new process. Original Unity package sources remain unchanged.

User-facing packaging and clean-machine/Unity-consumer acceptance remain tracked
under C5 in `LLM Docs/IMM_CONTENT_PIPELINE_PLAN.md`.
