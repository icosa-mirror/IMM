# Windows Native DLL Dependency Strategy

## Context

Two Unity packages currently ship Windows native plugins:

- `com.immersive-foundation.imm-unity` (`ImmUnityPlugin.dll`)
- `com.immersive-foundation.imm-stroke-reader` (`ImmStrokeReader.dll`)

Both plugins depend on shared codec/runtime libraries (`zlib1.dll`, `jpeg62.dll`, `libpng16.dll`, `ogg.dll`, `vorbis.dll`).

If those shared DLLs are duplicated in both package plugin folders, Unity can report duplicate native plugin conflicts during build/import.

## Current Implemented Solution

### Ownership

- Shared DLLs are owned by `imm-stroke-reader` package:
  - `Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64`
- `imm-unity` package no longer includes those 5 shared DLLs.

### How `ImmUnityPlugin.dll` still works

`ImmUnityPlugin.dll` was changed to **delay-load** these 5 DLLs instead of requiring them at immediate load time.

Implementation details:

1. In `code/appImmUnity/appImmUnity.vcxproj`:
   - Added `delayimp.lib`
   - Added delay-load list:
     - `zlib1.dll`
     - `jpeg62.dll`
     - `libpng16.dll`
     - `ogg.dll`
     - `vorbis.dll`

2. In `code/appImmUnity/src/main.cpp`:
   - Added Windows `DllMain` preload logic.
   - On `DLL_PROCESS_ATTACH`, plugin attempts to `LoadLibraryW(...)` those 5 DLLs from:
     - `Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64`
   - This allows delayed imports to resolve when first used.

3. `imm-unity` package has dependency on `imm-stroke-reader` in:
   - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/package.json`

## Pros/Cons of Current Solution

### Pros

- Removes duplicate plugin conflicts in Unity.
- Keeps reverse ownership model (shared DLLs in stroke-reader package only).
- Requires relatively small code/build changes.

### Cons

- Not true static linking; still runtime DLL dependent.
- Path-based preload logic is brittle if package layout/names change.
- More moving parts than a direct linker-only solution.
- Debugging can be less obvious if preload path stops matching actual install layout.

## Alternative Options

## Option A: Shared DLLs owned by `imm-unity` (original direction)

Keep shared DLLs beside `ImmUnityPlugin.dll`; remove duplicates from stroke-reader.

### Pros

- `ImmUnityPlugin.dll` load path is simple and robust.
- Minimal runtime indirection.

### Cons

- Opposite ownership from desired direction.
- Stroke-reader package no longer self-contained for those shared deps.

## Option B: Current delay-load + preload (implemented now)

Shared DLLs live in stroke-reader package; imm-unity resolves them via delay-load and preload.

### Pros

- Achieves reverse ownership now.
- Avoids duplicate Unity plugin conflicts.

### Cons

- Moderate fragility due to path assumptions.
- Still dynamic runtime dependency.

## Option C: True static linking (recommended long-term)

Statically link zlib/jpeg/png/ogg/vorbis into consuming plugin(s), so those 5 DLLs are no longer runtime dependencies.

### Pros

- Most robust deployment model.
- Removes this class of runtime missing-DLL errors.
- Simplifies Unity package file layout.

### Cons

- Requires build/link rework and verification of static library artifacts.
- Larger plugin binaries.
- Some up-front integration/testing effort.

## Recommendation

- Short term: keep **Option B** if reverse ownership is required immediately.
- Long term: migrate to **Option C** (true static linking) for lower maintenance risk.

## Validation Checklist

After any change to dependency model:

1. Run Windows plugin build script:
   - `code/projects/windows/build-unity-plugins.ps1`
2. Verify package DLL layout in both plugin folders.
3. Verify dependency graph:
   - `dumpbin /dependents ImmUnityPlugin.dll`
4. Open Unity and confirm:
   - no duplicate plugin import/build errors
   - no "Failed to load ImmUnityPlugin.dll because dependencies could not be loaded" errors
