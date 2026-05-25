# Windows Build Helpers

Use these helpers from the repository root to rebuild Windows Unity/Godot plugins and sync sample project binaries.

## Unity Commands

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

## Unity Helper Behavior

1. Finds `MSBuild.exe` (`MSBUILD_EXE_PATH`, then `vswhere`, then PATH fallback)
2. Rebuilds `appImmUnity` and `appImmStrokeReader` in `imm.sln`
3. Copies Windows plugin outputs into:
   - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64`
   - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64`
4. Ensures `com.immersive-foundation.imm-stroke-reader` also gets its required Windows runtime dependencies (`zlib1.dll`, `jpeg62.dll`, `libpng16.dll`, `ogg.dll`, `vorbis.dll`)

## Godot Commands

Batch:

```batch
code\projects\windows\build-godot-extension.bat -Configuration Debug -GodotCppPath C:\path\to\godot-cpp
```

PowerShell:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -Configuration Debug -GodotCppPath C:\path\to\godot-cpp
```

To also build `godot-cpp` before the extension:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -Configuration Release -GodotCppPath C:\path\to\godot-cpp -BuildGodotCpp
```

To clone the default Godot 4.2-compatible bindings into `thirdparty\godot-cpp` and build them:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -Configuration Release -BootstrapGodotCpp -BuildGodotCpp
```

When `-BuildGodotCpp` is passed, the helper reuses a cached/existing `godot-cpp` library plus generated bindings when both are present, and rebuilds only when the cache is incomplete.

Use `-GodotCppRef <tag-or-branch>` or `GODOT_CPP_REF` to override the default `godot-4.2-stable` bindings ref.

To build the extension and immediately run the native Godot smoke test:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -Configuration Release -BootstrapGodotCpp -BuildGodotCpp -RunSmoke -GodotExe C:\path\to\Godot_v4.2.2-stable_win64.exe
```

To run local Godot extension checks without MSBuild, SCons, or `godot-cpp`:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -VerifyOnly
```

The same local checks can be run directly on any platform with Python:

```powershell
python code\appImmGodotGDExtension\verify_local.py
```

To include the script-stub Godot project smoke when Godot is installed:

```powershell
$env:IMM_GODOT_RUN_LOCAL_SMOKE = "1"
$env:GODOT_EXE = "C:\path\to\Godot_v4.2.2-stable_win64.exe"
python code\appImmGodotGDExtension\verify_local.py
```

To check the Windows Godot build toolchain/dependency paths without compiling or cloning `godot-cpp`:

```powershell
.\code\projects\windows\build-godot-extension.ps1 -Configuration Release -BootstrapGodotCpp -BuildGodotCpp -PreflightOnly
```

After building the GDExtension, run the Godot project smoke test with a Godot executable on PATH or via `GODOT_EXE`:

```powershell
.\code\projects\windows\run-godot-smoke.ps1 -Configuration Release -RequireExtension
```

Or pass the executable directly:

```powershell
.\code\projects\windows\run-godot-smoke.ps1 -Configuration Release -GodotExe C:\path\to\Godot_v4.2.2-stable_win64.exe -RequireExtension
```

To save smoke output and run metadata for debugging:

```powershell
.\code\projects\windows\run-godot-smoke.ps1 -Configuration Release -GodotExe C:\path\to\Godot_v4.2.2-stable_win64.exe -RequireExtension -LogDir artifacts\godot-smoke
```

Native smoke logs also include `godot-extension-dlls.txt`, which records every expected staged DLL with found/missing status, byte size, and UTC timestamp before Godot launches.

To check Godot executable resolution, smoke scene selection, and native dependency staging without launching the project:

```powershell
.\code\projects\windows\run-godot-smoke.ps1 -Configuration Release -GodotExe C:\path\to\Godot_v4.2.2-stable_win64.exe -RequireExtension -PreflightOnly
```

## Godot Helper Behavior

1. Runs `code/appImmGodotGDExtension/verify_local.py` to check sample/native API parity, `.gdextension` manifest paths, Compatibility renderer setting, script-stub/native scene structure, native `ImmViewerNode` registration and method bindings, `ImmViewerNode` camera registration plus camera/viewport render queue ownership, Windows `godot-cpp` bootstrap/CI wiring, source paths for the IMM runtime dependency DLLs staged by SCons, PowerShell helper syntax when PowerShell is available, `ImmGodot` C ABI export alignment, local Python files, and the native syntax-only compile when `clang++` is available.
2. Stops after local verification when `-VerifyOnly` is passed.
3. Resolves `godot-cpp` from `-GodotCppPath`, `GODOT_CPP_PATH`, or `thirdparty\godot-cpp`; with `-BootstrapGodotCpp`, a full build clones `https://github.com/godotengine/godot-cpp.git` at `-GodotCppRef` when missing.
4. Finds `MSBuild.exe`.
5. Stops after tool/dependency resolution when `-PreflightOnly` is passed.
6. Rebuilds `appImmGodot` in `imm.sln`.
7. Optionally builds `godot-cpp` with SCons, skipping the build when a matching library and generated bindings already exist.
8. Reruns `verify_local.py` with `GODOT_CPP_PATH` set to the resolved `godot-cpp` checkout, so generated Godot C++ header mismatches are caught before linking when generated bindings are present.
9. Runs `code/appImmGodotGDExtension/SConstruct`.
10. Writes `imm_godot_extension.dll`, copies `ImmGodotPlugin.dll`, stages the IMM runtime dependency DLLs, verifies the complete staged DLL set, and writes `godot-extension-dlls.txt` in:
   - `code/ImmGodotSampleProject/bin/windows/debug`
   - or `code/ImmGodotSampleProject/bin/windows/release`
11. Runs `run-godot-smoke.ps1 -RequireExtension` when `-RunSmoke` is passed.

The GitHub Actions Windows job additionally downloads Godot 4.2.2, caches `thirdparty\godot-cpp` by `GODOT_CPP_REF`, runs script-stub smoke before the native build with `run-godot-smoke.ps1 -LogDir artifacts\godot-smoke-script`, builds the GDExtension, runs native smoke with `run-godot-smoke.ps1 -RequireExtension -LogDir artifacts\godot-smoke-native`, uploads smoke logs as `ImmGodotSmokeLogs-Windows`, and uploads the full staged DLL set plus `godot-extension-dlls.txt` as `ImmGodotGDExtension-Windows`. Both smoke modes call `load_document()`, require `is_loaded()`, check document state/background color, exercise chapter/bounds/layer/spawn-area query APIs, exercise playback controls, queue render work, and validate render diagnostics including adapter graphics/before/after callback counts. The wrapper requires both a zero Godot exit code and the `IMM Godot smoke test passed` marker in output. The `-RequireExtension` smoke mode first verifies the GDExtension DLL, `ImmGodotPlugin.dll`, and staged IMM runtime dependency DLLs, then loads `NativeSmokeScene.tscn`, which uses a native `ImmViewerNode`; omitting the switch uses the script-stub `SampleScene.tscn`.
