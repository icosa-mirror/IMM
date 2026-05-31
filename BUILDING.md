# Building

## Quick reference — Windows

**Always use the solution file. Never build a `.vcxproj` directly.**
The projects use `$(SolutionDir)` in their include paths. Building a standalone `.vcxproj`
leaves that variable undefined and produces "cannot open include file" errors.

In PowerShell or cmd.exe:
```
msbuild code\projects\windows\imm.sln /p:Configuration=Release /p:Platform=x64 /m
```

Vulkan renderer development additionally needs a Vulkan shader toolchain on PATH:

- `glslangValidator` or `glslc` for GLSL-to-SPIR-V generation.
- `dxc` for HLSL-to-SPIR-V experiments.
- `spirv-val` and `spirv-dis` for validation and inspection.

On Windows, the LunarG Vulkan SDK provides these tools. A local user-profile install via Scoop is:

```powershell
scoop install vulkan dxc glslang
```

The Windows GitHub Actions build installs `vulkan-sdk` before MSBuild so CI has the same SPIR-V tools available.

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

> **Build the full solution at least once before relying on `-t:`.** On a clean checkout, `-t:appImmUnity` fails with
> `LNK1181: cannot open input file '...\libImmExporter\bin\x64\Release\libImmExporter.lib'`: `appImmUnity` links `libImmExporter.lib`
> but does not declare `libImmExporter` as a solution build dependency, so a single-target build never builds it. Run one full-solution
> build (no `-t:`) first; after that, single-target incremental builds resolve.

See the macOS section below for the standalone viewer, and `code/projects/android/README.md` for Android builds (including the Unity plugin `.so` files). iOS builds follow the CI workflow in `.github/workflows/build.yml`.

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

The app bundle declares `.imm` document support for local unsigned builds. Finder/open-file launches are accepted before startup, and same-process document replacement uses the same viewer teardown/reinit path covered by `validateAppImmViewerMetalReload`; the reload validator waits for the active document to finish loading before teardown and validates the post-reload frame. If a user-opened replacement fails after teardown, the app shows a native failed-open alert and attempts to restore the previous document. You can also use `File > Open...` or drag a `.imm` file onto the Metal view.

With `sample1.imm`, expect a window titled `sample1.imm - IMM Metal Player`, a 360 forest-style image backdrop, a small character standing on a filled mossy branch in front of it, and audio through the macOS AVFoundation backend. The bundled default settings use the Static paint renderer; the automated gate also validates the Pretessellated paint path for this sample. This sample does not prove separate 2D picture layers. Basic navigation is left-drag to look around, `W/A/S/D` to move, `Q/E` down/up, Shift faster, Control slower, and `P` or Space pause/resume. The File menu supports `Open...` and Open Recent entries for successfully loaded IMM files. The Playback menu exposes play/pause, restart, previous, and next. Native audio controls are available from the Audio menu and keyboard: `M` toggles mute, `=` raises document volume, and `-` lowers document volume.

Run the full automated standalone Metal validation gate:

```bash
cmake --build build/macos --target validateAppImmViewerMetal --config Release
```

This aggregate target runs playback, audio decode, longer audio playback, normal-run interactive audio/control smoke, command-line contract, capture, native-frame failure, app-bundle, content-path launch, repeated launch/teardown, and in-process reload validation. When optional authored audio, authored picture, local content sweep, or reference-image CMake paths are configured, the matching local validation targets are included in the same aggregate gate. Individual standalone Metal validation targets are also available for narrower checks:

```bash
cmake --build build/macos --target validateAppImmViewerMetalPlayback --config Release
cmake --build build/macos --target validateAppImmViewerMetalCliContract --config Release
cmake --build build/macos --target validateAppImmViewerMetalCapture --config Release
cmake --build build/macos --target validateAppImmViewerMetalAudio --config Release
cmake --build build/macos --target validateAppImmViewerMetalLongAudio --config Release
cmake --build build/macos --target validateAppImmViewerMetalInteractiveAudio --config Release
cmake --build build/macos --target validateAppImmViewerMetalAuthoredAudio --config Release
cmake --build build/macos --target validateAppImmViewerMetalAuthoredLongAudio --config Release
cmake --build build/macos --target validateAppImmViewerMetalAuthoredPicture2D --config Release
cmake --build build/macos --target validateAppImmViewerMetalAuthoredPicture360 --config Release
cmake --build build/macos --target validateAppImmViewerMetalAuthoredPicture360Cubemap --config Release
cmake --build build/macos --target validateAppImmViewerMetalNativeFrameFailure --config Release
cmake --build build/macos --target validateAppImmViewerMetalBundle --config Release
cmake --build build/macos --target validateAppImmViewerMetalContentOverride --config Release
cmake --build build/macos --target validateAppImmViewerMetalRepeat --config Release
cmake --build build/macos --target validateAppImmViewerMetalReload --config Release
```

Or run the playback gate through CTest:

```bash
ctest --test-dir build/macos -R appImmViewerMetalPlayback --output-on-failure
```

To run all standalone Metal CTest checks, including audio decode, longer audio playback, normal-run interactive audio/control smoke, command-line contract, capture, native-frame failure, app-bundle, content-path launch, repeated launch/teardown validation, in-process reload validation, and any configured optional local-content checks:

```bash
ctest --test-dir build/macos -R appImmViewerMetal --output-on-failure
```

For visual inspection captures:

```bash
IMM_METAL_VALIDATE_CAPTURE_DIR=build/macos/metal-validation-captures \
IMM_METAL_VALIDATE_CAPTURE_FORMAT=png \
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

Baseline capture source:

- Preferred: run the Windows CI job and inspect `ImmViewer-Windows-DirectX-Baseline/windows-directx-static.png`.
- Fallback: run the Windows DirectX capture script manually from a Windows checkout or downloaded `ImmViewer-Windows` artifact.
- Do not treat numeric image comparison as authoritative until the Windows DirectX PNG has been visually inspected.

After generating or downloading a Windows DirectX baseline PNG, compare it against the Metal PNG with:

```bash
python3 code/appImmViewer/scripts/compare_captures.py \
  build/baseline-captures/windows-directx-static.png \
  build/macos/metal-validation-captures/static.png \
  --json-output build/macos/reference-comparison/windows-directx-vs-metal-static.json \
  --diff-output build/macos/reference-comparison/windows-directx-vs-metal-static-diff.png \
  --contact-sheet-output build/macos/reference-comparison/windows-directx-vs-metal-static-contact-sheet.png \
  --diff-scale 8
```

The JSON file records the dimensions, mean absolute channel difference, RMS channel difference, maximum channel difference, and differing-pixel count/percentage. The diff PNG is an amplified absolute RGB difference image. The contact sheet places reference, Metal candidate, and amplified diff side by side for visual review. Use the metrics and images for inspection until an accepted tolerance is chosen.

Current local `sample1.imm` evidence from fresh validation captures:

- Windows DirectX vs Metal static: `meanAbs=1.984359 rms=3.549581 maxChannelDiff=167 differingPixels=696946/921600 (75.623481%)`.
- Metal static vs Metal pretessellated: `meanAbs=0.000386 rms=0.116487 maxChannelDiff=73 differingPixels=55/921600 (0.005968%)`.

These are recorded metrics for inspection and threshold-setting, not an accepted CI threshold.

After the Windows DirectX reference has been visually inspected and tolerances have been chosen, the same comparison can be run through CMake:

```bash
cmake -S code/projects/macos -B build/macos \
  -DIMM_BUILD_VIEWER=ON \
  -DIMM_METAL_REFERENCE_CAPTURE_PATH=/path/to/windows-directx-static.png \
  -DIMM_METAL_REFERENCE_MAX_MEAN_ABS=10 \
  -DIMM_METAL_REFERENCE_MAX_RMS=25 \
  -DIMM_METAL_REFERENCE_MAX_CHANNEL_DIFF=255 \
  -DIMM_METAL_REFERENCE_MAX_DIFFERING_PERCENT=100
cmake --build build/macos --target validateAppImmViewerMetalReferenceCompare --config Release
```

The target generates a fresh `build/macos/reference-comparison/static.png`, compares it against the configured reference, writes `build/macos/reference-comparison/reference-vs-metal-static.json`, `build/macos/reference-comparison/reference-vs-metal-static-diff.png`, and `build/macos/reference-comparison/reference-vs-metal-static-contact-sheet.png`, and fails if any configured threshold is exceeded. Leave `IMM_METAL_REFERENCE_CAPTURE_PATH` unset when the reference artifact is not available.

The Windows CI job uploads `ImmViewer-Windows-DirectX-Baseline`, containing `windows-directx-static.png` and the runtime settings JSON used to create it.
The macOS CI job uploads one standalone artifact, `ImmViewerMetal-macOS`, containing the standalone `.app` bundle plus `metal-static.png` and its validation log under `metal-baseline-captures/`. That PNG is generated through `validate_metal_standalone.sh`, so the uploaded capture path also exercises the same structural render checks, audio-free deterministic playback path, and zero Metal renderer resource cleanup gate as local validation.

To generate the Windows DirectX baseline PNG from a Windows checkout:

```powershell
.\code\appImmViewer\scripts\capture_windows_directx_baseline.ps1
```

The downloaded `ImmViewer-Windows` artifact is self-contained for this baseline capture. From the artifact folder, run:

```powershell
.\capture_windows_directx_baseline.ps1
```

The validation target covers static paint playback, pretessellated paint playback, the 360 picture backdrop in `sample1.imm`, deterministic 2D-picture and 360-cubemap shader smoke draws with pixel readback, Ogg Opus decode plus accepted `Play()` calls, observed `state=playing` transitions, 1.0-second playback-progress markers through the AVFoundation backend, AVFoundation teardown with zero temp-file removal failures and at least one removed temp WAV per validated sound object, zero live Metal renderer resources at teardown, a normal-run audio smoke that selects AVFoundation without validation rendering, native volume/mute smoke controls, playback-control smoke, Open Recent smoke, failed-open restore smoke, render-target recreation after resize, command-line argument contract failures, capture output, repeated native-frame setup failures, the generated `.app` bundle layout/metadata, `.imm` document metadata, launching with only a content path from outside the repository root, repeated app launch/teardown, and in-process document reload. It also requires the Metal renderer report to include both `standaloneProven` and `standaloneUnsupported` feature-surface lines, so the supported/stubbed backend status is visible in validation logs. The app bundle includes a default Metal settings file in `Contents/Resources`; validation requires that this bundled default selects Metal and Static, while a JSON argument still overrides the settings file explicitly. Validation fails on shader/pipeline failures, missing `picture2DShader=1` or `picture360CubemapShader=1` sanity markers, missing Metal renderer feature-report lines, unsupported Metal renderer paths, blank output, missing picture/360 draw calls, the known old static backdrop-only hash, the old opaque-paint hashes from before Metal honored paint alpha coverage, unexpected draw/triangle counts, failing to decode the three distinct Ogg Opus sounds in `sample1.imm`, hitting an Ogg Opus decode/player-creation failure, failing to reach an accepted AVFoundation `Play()` result, failing to observe AVFoundation playback enter `state=playing`, failing to observe playback reach the configured progress threshold, failing AVFoundation audio teardown, leaking Metal renderer resources at teardown, failing the native volume/mute smoke, failing the playback-control smoke, failing failed-open restore, selecting the null backend in the normal-run audio smoke, failing clean normal-run teardown, or any rejected AVFoundation play request. Full-frame hashes for paint playback are not pinned because Metal now honors the animated blue-noise/sample-mask transparency path.
Standalone Metal render validation uses the null sound backend by default to keep render tests deterministic; the focused audio validation targets enable AVFoundation and check `sample1.imm` audio decode and playback progress.
The optional authored-audio validation targets can be enabled with `-DIMM_METAL_AUTHORED_AUDIO_PATH=/path/to/audio.imm`; locally, `/Users/andrewbaker/Documents/Quill/Snoopy/Snoopy.imm` validates three WAV sound objects through AVFoundation.
The embedded macOS Opus build uses the portable non-intrinsics path. This avoids Opus' runtime NEON-detection configure path on Apple Silicon and is sufficient for standalone IMM audio decode.
When capture output is enabled through `IMM_METAL_VALIDATE_CAPTURE_DIR`, the validation script also checks each PPM capture header for the expected format and dimensions. Single-run `IMM_METAL_VALIDATE_CAPTURE_PATH` captures can be `.png` or `.ppm`.

To sanity-check the native audio path directly:

```bash
cmake --build build/macos --target validateAppImmViewerMetalAudio --config Release
```

The expected log signal is `Standalone Metal audio validation passed: opusDecoded=3 wavAdded=0 total=3 playCalls=6 playingStates=6 progressMarkers=6 progressThresholdSec=1.0.` Internally, the first standalone milestone decodes embedded Ogg Opus to PCM temp WAV files for `AVAudioPlayer`; decode or player-creation failure returns `-1` and fails the sound layer load rather than creating a silent placeholder. The same audio contract also requires `AVFoundation audio Deinit complete: ... tempFileRemoveFailures=0` and verifies that `tempFilesRemoved` covers the validated temp-backed sound count, so temp WAV cleanup regressions fail validation.

For a longer bundled-audio gate:

```bash
cmake --build build/macos --target validateAppImmViewerMetalLongAudio --config Release
```

The expected longer-audio signal is `Standalone Metal audio validation passed: opusDecoded=3 wavAdded=0 total=3 playCalls=6 playingStates=6 progressMarkers=6 progressThresholdSec=3.0.`

To sanity-check the normal-run audio path without render validation enabled:

```bash
cmake --build build/macos --target validateAppImmViewerMetalInteractiveAudio --config Release
```

The expected normal-run audio signal is `Standalone Metal interactive audio validation passed: opusDecoded=3 wavAdded=0 total=3 playCalls=6 playingStates=6 progressMarkers=6 progressThresholdSec=1.0.` This target also enables `IMM_METAL_VALIDATE_VOLUME_CONTROLS=1`, `IMM_METAL_VALIDATE_PLAYBACK_CONTROLS=1`, `IMM_METAL_VALIDATE_OPEN_FAILURE_RESTORE=1`, and `IMM_METAL_VALIDATE_RECENT_DOCUMENTS=1`, requiring native volume/mute, playback-control, failed-open restore, and Open Recent smoke logs. The failed-open restore smoke suppresses the native alert while validating that the previous document is restored. The recent-document smoke clears both the app-local MRU list and the AppKit recent list after validating it so automated runs do not leave test entries behind.

To validate a local authored WAV-audio file in addition to `sample1.imm`:

```bash
cmake -S code/projects/macos -B build/macos \
  -DIMM_BUILD_VIEWER=ON \
  -DIMM_METAL_AUTHORED_AUDIO_PATH=/Users/andrewbaker/Documents/Quill/Snoopy/Snoopy.imm
cmake --build build/macos --target validateAppImmViewerMetalAuthoredAudio --config Release
```

The expected local authored-audio signal for `Snoopy.imm` is `opusDecoded=0 wavAdded=3 total=3 playCalls=6 playingStates=6 progressMarkers=6 progressThresholdSec=1.0`.

For a longer authored-audio gate on the same configured file:

```bash
cmake --build build/macos --target validateAppImmViewerMetalAuthoredLongAudio --config Release
```

The expected local authored long-audio signal for `Snoopy.imm` is `opusDecoded=0 wavAdded=3 total=3 playCalls=6 playingStates=6 progressMarkers=6 progressThresholdSec=3.0`.

To scan IMM picture-layer metadata without launching Metal rendering:

```bash
cmake --build build/macos --target ImmPictureScan --config Release
build/macos/tools/ImmPictureScan exampleImmFiles/sample1.imm
```

For a bounded recursive scan of a private cache:

```bash
cmake -S code/projects/macos -B build/macos \
  -DIMM_METAL_PICTURE_SCAN_PATH=/Volumes/andy-desktoppc-1/Imm \
  -DIMM_METAL_PICTURE_SCAN_MAX_FILES=120 \
  -DIMM_METAL_PICTURE_SCAN_SKIP_FILES=0 \
  -DIMM_METAL_PICTURE_SCAN_MAX_SIZE_MB=80 \
  -DIMM_METAL_PICTURE_SCAN_TIMEOUT_SEC=8 \
  -DIMM_METAL_PICTURE_SCAN_NAME_REGEX='(cube|cubemap|skybox|sky|360)' \
  -DIMM_METAL_PICTURE_SCAN_MIN_CUBEMAP_FILES=0
cmake --build build/macos --target scanImmPictureLayers --config Release
```

The equivalent direct script form is:

```bash
code/appImmViewer/scripts/scan_imm_picture_layers.sh \
  --max-files 120 \
  --skip-files 0 \
  --max-size-mb 80 \
  --per-file-timeout-sec 8 \
  --name-regex '(cube|cubemap|skybox|sky|360)' \
  --min-cubemap-files 0 \
  --output build/macos/imm-picture-layers-desktoppc-bounded.tsv \
  /Volumes/andy-desktoppc-1/Imm
```

The TSV reports picture content types directly from the IMM importer: `image2D`, `equirect360`, `cubemap360`, `cubemapCross`, and `cubemapVstrip`. Omit the name regex for blind chunks; include it for targeted filename-hint scans before a Metal render sweep when looking for authored cubemap candidates in a large cache.
The summary counts a file as a cubemap candidate if any of `cubemap360`, `cubemapCross`, or `cubemapVstrip` is nonzero. Recursive scans sort matching `.imm`/`.IMM` paths before applying skip/max limits, so use `--skip-files` to scan later deterministic chunks of a large cache without rescanning the first matching files. Set `--min-cubemap-files 1` or `IMM_METAL_PICTURE_SCAN_MIN_CUBEMAP_FILES=1` when a scan is expected to contain authored cubemap content and should fail otherwise. `IMM_METAL_PICTURE_SCAN_MAX_FILES=0` means unbounded; keep a positive limit for large private caches.
To summarize one or more scan TSVs without rescanning content, run `code/appImmViewer/scripts/scan_imm_picture_layers.sh --summarize-tsv path/to/*.tsv`.

To sweep a local folder of `.imm` files without making those private assets part of CI:

```bash
cmake -S code/projects/macos -B build/macos \
  -DIMM_BUILD_VIEWER=ON \
  -DIMM_METAL_CONTENT_SWEEP_PATH=/Users/andrewbaker/Documents/Quill \
  -DIMM_METAL_CONTENT_SWEEP_MAX_FILES=100 \
  -DIMM_METAL_CONTENT_SWEEP_MAX_BYTES=50000000 \
  -DIMM_METAL_CONTENT_SWEEP_MAX_FRAME=300 \
  -DIMM_METAL_CONTENT_SWEEP_MIN_PASSED=1 \
  -DIMM_METAL_CONTENT_SWEEP_FAIL_ON_FAILED=0
cmake --build build/macos --target validateAppImmViewerMetalContentSweep --config Release
```

The sweep recursively finds `.imm` files, runs relaxed standalone Metal validation, writes `build/macos/metal-content-sweep.tsv`, and writes logs/captures under `build/macos/metal-content-sweep-logs` and `build/macos/metal-content-sweep-captures`. When `IMM_METAL_CONTENT_SWEEP_PATH` is configured, the bounded sweep is also included in `validateAppImmViewerMetal` and CTest as `appImmViewerMetalContentSweep`. It requires at least `IMM_METAL_CONTENT_SWEEP_MIN_PASSED` nonblank passed renders, classifies blank-at-camera files as `blank`, and can be made strict with `IMM_METAL_CONTENT_SWEEP_FAIL_ON_FAILED=1`. Set `IMM_METAL_CONTENT_SWEEP_MAX_FILES=0` or `IMM_METAL_CONTENT_SWEEP_MAX_BYTES=0` only when an unbounded local sweep is intentional.

For large private caches, bound the sweep before pointing it at a multi-GB tree:

```bash
IMM_METAL_SWEEP_MAX_FILES=100 \
IMM_METAL_SWEEP_MAX_BYTES=20000000 \
IMM_METAL_SWEEP_OUTPUT=build/macos/large-cache-sweep.tsv \
IMM_METAL_SWEEP_LOG_DIR=build/macos/large-cache-sweep-logs \
  code/appImmViewer/scripts/validate_metal_content_sweep.sh \
  build/macos/viewer/appImmViewerMetal.app/Contents/MacOS/appImmViewerMetal \
  /Volumes/andy-desktoppc-1/Imm
```

The TSV includes `sizeBytes` and uses `skipped_size` / `skipped_limit` rows when those bounds exclude files.

To run the optional authored 2D-picture validation against a local/private IMM file:

```bash
cmake -S code/projects/macos -B build/macos \
  -DIMM_BUILD_VIEWER=ON \
  -DIMM_METAL_AUTHORED_2D_PICTURE_PATH="/Volumes/andy-desktoppc-1/Imm/896098972954263_Suzanne_by_Starmen tv.imm"
cmake --build build/macos --target validateAppImmViewerMetalAuthoredPicture2D --config Release
```

This target requires `picture2DDrawCalls>=1` inside the player validation loop and writes PNG captures under `build/macos/authored-picture2d-captures`. It is optional because the asset is not in the repository or CI.

To run the optional authored 360 equirect-picture validation against a local/private IMM file:

```bash
cmake -S code/projects/macos -B build/macos \
  -DIMM_BUILD_VIEWER=ON \
  -DIMM_METAL_AUTHORED_360_PICTURE_PATH="/Volumes/andy-desktoppc-1/Imm/3990504411070340_MELANCHOLY2021_by_CRYHARDSTUDIOS.imm"
cmake --build build/macos --target validateAppImmViewerMetalAuthoredPicture360 --config Release
```

This target requires `picture360DrawCalls>=1` and writes PNG captures under `build/macos/authored-picture360-captures`. It is optional because the asset is not in the repository or CI.

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
