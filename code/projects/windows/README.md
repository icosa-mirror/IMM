# Windows Build Helpers

Use these helpers from the repository root to rebuild Windows Unity plugins and sync the sample Unity package binaries.

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

## What the helper does

1. Finds `MSBuild.exe` (`MSBUILD_EXE_PATH`, then `vswhere`, then PATH fallback)
2. Rebuilds `appImmUnity` and `appImmStrokeReader` in `imm.sln`
3. Copies Windows plugin outputs into:
   - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64`
   - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64`
4. Ensures `com.immersive-foundation.imm-stroke-reader` also gets its required Windows runtime dependencies (`zlib1.dll`, `jpeg62.dll`, `libpng16.dll`, `ogg.dll`, `vorbis.dll`)
