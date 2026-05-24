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

## Quick reference — macOS Metal standalone player

Configure the macOS CMake project with the viewer targets enabled:

```bash
cmake -S code/projects/macos -B build/macos -DIMM_BUILD_VIEWER=ON
```

Build the standalone Metal player:

```bash
cmake --build build/macos --target appImmViewerMetal --config Release
```

Run the player with the bundled sample:

```bash
build/macos/viewer/appImmViewerMetal.app/Contents/MacOS/appImmViewerMetal exampleImmFiles/sample1.imm
```

This launches as a regular foreground macOS app for interactive use. You can also start the app bundle directly:

```bash
open build/macos/viewer/appImmViewerMetal.app --args exampleImmFiles/sample1.imm
```

The app bundle declares `.imm` document support for local unsigned builds. Finder/open-file launches are accepted before startup, and opening a `.imm` file while the app is already running reloads the active viewer document and updates the window title. You can also use `File > Open...` or drag a `.imm` file onto the Metal view.

With `sample1.imm`, expect a window titled `sample1.imm - IMM Metal Player`, a 360 forest-style image backdrop, and dark green/yellow stroke geometry visibly composited in front of it. The bundled default settings use the Static paint renderer because that is the visually validated path today. This sample does not prove model layers, separate 2D picture layers, or audio output; the standalone Metal viewer currently uses the null sound backend. Basic navigation is left-drag to look around, `W/A/S/D` to move, `Q/E` down/up, Shift faster, Control slower, and `P` pause/resume.

Run the full automated standalone Metal validation gate:

```bash
cmake --build build/macos --target validateAppImmViewerMetal --config Release
```

This aggregate target runs playback, command-line contract, capture, native-frame failure, app-bundle, content-path launch, and repeated launch/teardown validation. Individual standalone Metal validation targets are also available for narrower checks:

```bash
cmake --build build/macos --target validateAppImmViewerMetalPlayback --config Release
cmake --build build/macos --target validateAppImmViewerMetalCliContract --config Release
cmake --build build/macos --target validateAppImmViewerMetalCapture --config Release
cmake --build build/macos --target validateAppImmViewerMetalNativeFrameFailure --config Release
cmake --build build/macos --target validateAppImmViewerMetalBundle --config Release
cmake --build build/macos --target validateAppImmViewerMetalContentOverride --config Release
cmake --build build/macos --target validateAppImmViewerMetalRepeat --config Release
```

Or run the playback gate through CTest:

```bash
ctest --test-dir build/macos -R appImmViewerMetalPlayback --output-on-failure
```

To run all standalone Metal CTest checks, including command-line contract, capture, native-frame failure, app-bundle, content-path launch, and repeated launch/teardown validation:

```bash
ctest --test-dir build/macos -R appImmViewerMetal --output-on-failure
```

For visual inspection captures:

```bash
IMM_METAL_VALIDATE_CAPTURE_DIR=build/macos/metal-validation-captures \
  code/appImmViewer/scripts/validate_metal_standalone.sh build/macos/viewer/appImmViewerMetal.app/Contents/MacOS/appImmViewerMetal exampleImmFiles/sample1.imm
```

For a single PNG capture from the same Metal validation readback path, set `IMM_METAL_VALIDATE_CAPTURE_PATH` to a `.png` path and pass both settings and content paths explicitly:

```bash
IMM_METAL_VALIDATE_FRAME=1 \
IMM_METAL_VALIDATE_CAPTURE_PATH=build/macos/metal-validation-captures/static.png \
IMM_METAL_EXIT_AFTER_VALIDATE=1 \
  build/macos/viewer/appImmViewerMetal.app/Contents/MacOS/appImmViewerMetal \
  code/projects/macos/appImmViewerMetal-settings.json \
  exampleImmFiles/sample1.imm
```

After generating a Windows DirectX baseline PNG, compare it against the Metal PNG with:

```bash
python3 code/appImmViewer/scripts/compare_captures.py \
  build/baseline-captures/windows-directx-static.png \
  build/macos/metal-validation-captures/static.png
```

The validation target covers static paint playback, pretessellated paint draw-path coverage, the 360 picture backdrop in `sample1.imm`, render-target recreation after resize, command-line argument contract failures, capture output, repeated native-frame setup failures, the generated `.app` bundle layout/metadata, `.imm` document metadata, launching with only a content path from outside the repository root, and repeated app launch/teardown. The app bundle includes a default Metal settings file in `Contents/Resources`; validation requires that this bundled default selects Metal and Static, while a JSON argument still overrides the settings file explicitly. Validation fails on shader/pipeline failures, unsupported Metal renderer paths, blank output, missing picture/360 draw calls, the known old static backdrop-only hash, unexpected draw/triangle counts, and deterministic hash changes for the fixed-size static and pretessellated cases. The pretessellated capture is deterministic but is not yet a visual-equivalence reference.
When capture output is enabled through `IMM_METAL_VALIDATE_CAPTURE_DIR`, the validation script also checks each PPM capture header for the expected format and dimensions. Single-run `IMM_METAL_VALIDATE_CAPTURE_PATH` captures can be `.png` or `.ppm`.

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
