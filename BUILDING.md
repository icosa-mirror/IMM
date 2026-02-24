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

The build auto-copies the output DLL to:
```
code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll
```

See `README.md` for macOS, iOS, and Android build instructions.

## Key source files

| What to change | File |
|---|---|
| New native API function | `code/appImmStrokeReader/src/main.cpp`, `strokeStore.cpp`, `strokeStore.h` |
| C# P/Invoke bindings | `code/ImmUnitySampleProject/Packages/.../Runtime/ImmStrokeReader.cs` |
| IMM→SharpQuill conversion | `code/ImmUnitySampleProject/Packages/.../Runtime/SharpQuillCompat.cs` |

## Publishing changes

To make changes available to downstream projects, commit to this repo and push to the
`upm` branch. Downstream projects reference the package via the GitHub URL and will pick
up changes on next package resolution.
