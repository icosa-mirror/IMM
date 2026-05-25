# Windows Build Helpers

Use these helpers from the repository root to rebuild Windows Unity plugins, sync sample Unity package binaries, and build the Godot GDExtension bring-up target.

## Commands

Batch:

```batch
code\projects\windows\build-unity-plugins.bat
```

PowerShell:

```powershell
.\code\projects\windows\build-unity-plugins.ps1
```

Optional configuration:

```powershell
.\code\projects\windows\build-unity-plugins.ps1 -Configuration Debug
```

Godot GDExtension:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -BootstrapGodotCpp -BuildGodotCpp
```

Godot smoke:

```powershell
.\code\projects\windows\run-godot-smoke.ps1 -GodotExe "C:\Path\To\Godot.exe"
.\code\projects\windows\run-godot-smoke.ps1 -GodotExe "C:\Path\To\Godot.exe" -RequireExtension
```

Unity parity capture:

```powershell
.\code\projects\windows\capture-unity-parity.ps1 -UnityExe "C:\Path\To\Unity.exe"
.\code\projects\windows\capture-unity-parity.ps1 -UnityExe "C:\Path\To\Unity.exe" -SyncBuiltPlugins
```

End-to-end Unity/Godot parity:

```powershell
.\code\projects\windows\run-unity-godot-parity.ps1 `
  -UnityExe "C:\Path\To\Unity.exe" `
  -GodotExe "C:\Path\To\Godot.exe" `
  -RequireExtension
```

## What the helper does

1. Finds `MSBuild.exe` (`MSBUILD_EXE_PATH`, then `vswhere`, then PATH fallback)
2. Rebuilds `appImmUnity` and `appImmStrokeReader` in `imm.sln`
3. Copies Windows plugin outputs into:
   - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64`
   - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64`
4. Ensures `com.immersive-foundation.imm-stroke-reader` also gets its required Windows runtime dependencies (`zlib1.dll`, `jpeg62.dll`, `libpng16.dll`, `ogg.dll`, `vorbis.dll`)

## Godot helper behavior

`build-godot-extension.ps1`:

1. Optionally clones `godot-cpp` into `thirdparty\godot-cpp`
2. Resolves the requested SCons executable and, when bootstrapping `godot-cpp`, the Git executable before invoking them
3. Resolves a Visual Studio C++ MSBuild toolchain from `-MSBuildExe`, `MSBUILD_EXE_PATH`, preferred VS 2022 install paths, `vswhere`, or PATH
4. Rebuilds `appImmGodot` from `imm.sln`
5. Optionally builds `godot-cpp` for Godot 4.2
6. Runs `code\appImmGodotGDExtension\SConstruct`, which preflights the required `godot-cpp` include trees, key headers, import libraries, and GDExtension source list before invoking MSVC
7. Copies the IMM runtime DLL dependencies beside the Godot extension
8. Verifies the staged DLL set landed in `code\ImmGodotSampleProject\bin\windows\release`
9. Writes `godot-extension-dlls.txt` beside the staged DLLs with size, timestamp, and SHA-256 metadata
10. When passed `-SummaryDir`, writes `godot-build-summary.txt/json` so toolchain, bootstrap, MSBuild, SCons, dependency staging, DLL verification, and success phases are visible in CI artifacts, including resolved MSBuild/SCons/Git paths when available

`run-godot-smoke.ps1` launches `scripts/smoke_test_runner.gd` headlessly and requires the `IMM Godot smoke test passed` marker plus the `IMM Godot smoke lifecycle cycles: 2` marker. With `-RequireExtension`, it preflights the built DLLs, sets `IMM_GODOT_REQUIRE_EXTENSION=1`, and the smoke runner loads `NativeSmokeScene.tscn` so the native `ImmViewerNode` class must be present. Passing `-LogDir` writes `godot-smoke-output.log`, `godot-smoke-summary.txt`, `godot-smoke-summary.json`, `godot-matrix-diagnostics.json`, copies `project.godot`, `imm_viewer.gdextension`, and `NativeSmokeScene.tscn`, captures any discovered native IMM log, and writes a hashed DLL manifest for CI triage. The wrapper parses the matrix diagnostics JSON and requires the expected schema, Godot engine version, Compatibility renderer method, camera id, 16-float matrix arrays, and deterministic document/camera/projection values before native smoke can pass. Missing Godot executable, missing document, missing runtime DLL preflight failures, and post-run diagnostics failures are recorded in the summaries before the wrapper exits nonzero.

The Windows CI job runs the Godot GDExtension helper after the solution build, captures `artifacts\godot-build\build-godot-extension.log` plus `godot-build-summary.txt/json`, downloads Godot, runs `run-godot-smoke.ps1 -RequireExtension`, and uploads `ImmGodotBuild-Windows`, `ImmGodotGDExtension-Windows`, plus `ImmGodotSmoke-Windows`. The Godot build, staged DLL, and smoke-log uploads run with `if: always()` so compiler, loader, and smoke failures still leave triage artifacts when files exist. On manual `workflow_dispatch`, passing the optional `unity_exe` input also runs `run-unity-godot-parity.ps1 -RequireExtension -SyncBuiltUnityPlugins`, compares Unity/Godot diagnostics with `--require-extended`, writes `unity-godot-parity-summary.txt/json` plus `unity-godot-parity-comparison.log/json`, and uploads `ImmUnityParity-Windows` from `artifacts\parity`. Passing the optional `parity_document` input applies the same IMM document path to the Godot native smoke and Unity parity capture steps; when it is omitted during manual parity capture, CI resolves `exampleImmFiles\sample1.imm` once and passes that same absolute path to both engines.

## Matrix parity comparison

After capturing Unity diagnostics with `capture-unity-parity.ps1` and Godot smoke diagnostics with `run-godot-smoke.ps1 -LogDir artifacts\godot-smoke`, compare them with:

```powershell
python .\code\projects\windows\compare-matrix-diagnostics.py `
  --unity artifacts\unity-parity\unity-matrix-diagnostics.log `
  --godot artifacts\godot-smoke\godot-matrix-diagnostics.json `
  --require-extended `
  --summary-json artifacts\unity-parity\unity-godot-parity-comparison.json
```

The tool compares `world_to_head` and `projection` with a default tolerance of `1e-4` and reports the largest mismatch. With `--require-extended`, it also requires engine-version metadata and compares document identity, document transform, background color, document state, bounding-box min/max, and spawn-area state.
`--summary-json` writes a machine-readable pass/fail summary with per-check deltas for CI triage, including early parse/schema/shape errors. `capture-unity-parity.ps1` runs Unity in batchmode against `Assets\StreamingAssets\sample1.imm` by default and writes `artifacts\unity-parity\unity-matrix-diagnostics.log` plus `unity-parity-summary.txt/json`, including the Unity editor/runtime version parsed from the diagnostics payload. Passing `-SyncBuiltPlugins` first copies the freshly built `ImmUnityPlugin.dll` plus runtime audio DLLs into the Unity sample package, which is the CI parity-capture mode. For interactive parity capture, press `N` in the Unity sample while the document is loaded; the Unity sample then logs the same camera id, matrix values, and document transform that the Godot smoke runner submits before rendering, plus the loaded document diagnostics needed for broader parity checks.
`run-unity-godot-parity.ps1` wraps the full Phase 1 parity sequence: Godot smoke, Unity batch capture, and strict comparator output under `artifacts\parity`. It writes `unity-godot-parity-summary.txt/json` at the top level to identify whether failure happened in preflight, Godot smoke, Unity capture, or comparison. Passing `-DocumentPath` applies the same absolute document path to Godot smoke and Unity batch capture so the comparator is not comparing different IMM documents; if omitted, the helper resolves the repository `exampleImmFiles\sample1.imm` and passes it to both engines.
