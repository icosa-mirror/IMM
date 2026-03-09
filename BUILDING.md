# Building

## Quick reference — Windows

**Always use the solution file. Never build a `.vcxproj` directly.**
The projects use `$(SolutionDir)` in their include paths. Building a standalone `.vcxproj`
leaves that variable undefined and produces "cannot open include file" errors.

In PowerShell or cmd.exe:
```
msbuild code\projects\windows\imm.sln /p:Configuration=Release /p:Platform=x64 /m
```

In bash (Git Bash, WSL): use `-p:` not `/p:` — bash strips leading `/` from flags:
```
msbuild "code/projects/windows/imm.sln" -p:Configuration=Release -p:Platform=x64 -m
```

Each target auto-copies its output DLL to the corresponding UPM package:

| Target | Output DLL location |
|--------|---------------------|
| `appImmUnity` | `…/com.immersive-foundation.imm-unity/Plugins/x86_64/ImmUnityPlugin.dll` |
| `appImmStrokeReader` | `…/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll` |
| `appImmStrokeWriter` | `…/com.immersive-foundation.imm-stroke-writer/Plugins/x86_64/ImmStrokeWriter.dll` |

To build a single plugin, pass `-t:<target>`:
```
msbuild "code/projects/windows/imm.sln" -t:appImmStrokeWriter -p:Configuration=Release -p:Platform=x64 -m
```

See `README.md` for macOS, iOS, and Android build instructions.

## Key source files

| What to change | File |
|---|---|
| Reader native API | `code/appImmStrokeReader/src/main.cpp`, `strokeStore.cpp`, `strokeStore.h` |
| Reader C# bindings | `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ImmStrokeReader.cs` |
| Reader IMM→SharpQuill | `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/SharpQuillCompat.cs` |
| Writer native API | `code/appImmStrokeWriter/src/main.cpp` |
| Writer C# bindings | `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-writer/Runtime/ImmStrokeWriter.cs` |
| Writer high-level API | `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-writer/Runtime/ImmStrokeWriterDocument.cs` |

## Publishing changes

To make changes available to downstream projects, commit to this repo and push to the
`upm` branch. Downstream projects reference the package via the GitHub URL and will pick
up changes on next package resolution.
