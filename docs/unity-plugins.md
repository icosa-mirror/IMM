# IMM Unity plugins

Reference for the two native Unity plugins in this repository, the C# packages that wrap
them, and the gaps between what the binaries export and what the C# wrappers support.

Written against `fc9665e1` (2026-09-17). Every binary figure below was read from the
committed binaries themselves, not from the build files.

How to re-check after any native or wrapper change:

```powershell
python tests/tools/verify_unity_plugin_exports.py            # every platform
python tests/tools/verify_unity_plugin_exports.py --platform windows --json out.json
python tests/tools/verify_unity_csharp.py                    # compile both package runtimes
```

The first tool reads the `[DllImport]` entry points out of the C# packages and the export
tables out of the shipped binaries (PE, ELF, Mach-O and static archives, no external
tools required). It exits 1 on drift.

The second compiles the package runtime assemblies with MSBuild against the Unity Editor's
own managed DLLs, using the `.csproj` files Unity writes into `code/ImmUnitySampleProject`.
That catches C# errors the export check cannot see — see
[G19](#g19--nothing-compiles-the-unity-package-c-on-push) for why this is currently a local
check rather than a CI gate.

There are no known platform gaps left, so the check is expected to pass for all four
platforms. Right after changing native plugin source it will fail for the platforms whose
committed binaries have not been rebuilt yet (CI rebuilds Android, iOS and macOS and the
`sync-binaries` job commits the results); use `--platform windows` or `--platform android`
to check only what was built locally.

CI runs the same tool in `build.yml` — in `package-unity-plugins`, after the four platform
artifacts are staged into the packages, and again in `sync-binaries` before it commits — so
drift fails the build instead of surfacing as an `EntryPointNotFoundException` at runtime
(see [G16](#g16--no-automated-export-vs-dllimport-check)).

Last verified green on all four platforms against the binaries from sync commit
`32e99926`, which is also the first sync in which the exporter API reaches iOS and macOS.
The one entry point it reports as a known gap is `ImmUnityRegisterRenderingPlugin` on iOS,
which Unity compiles from `Plugins/iOS/ImmUnityPluginRegister.mm` rather than taking from
the static library.

---

## 1. Plugin inventory

| Plugin | Unity package | Platform | Binary | Format | Exports (all symbols) | Plugin API | Size | sha256 (16) |
|---|---|---|---|---|---|---|---|---|
| ImmUnityPlugin | `com.immersive-foundation.imm-unity` | Windows x86_64 | `Plugins/x86_64/ImmUnityPlugin.dll` | PE | 97 | 97 | 4.43 MB | `e749d3416a95cc92` |
| ImmUnityPlugin | " | Android arm64-v8a | `Plugins/Android/libs/arm64-v8a/libImmUnityPlugin.so` | ELF | 4389 | 97 | 15.20 MB | `9f7b0f380c46d420` |
| ImmUnityPlugin | " | iOS (static) | `Plugins/iOS/libImmUnityPlugin.a` | ar | 3075 | 97 | 4.30 MB | `3ec0cf52173196a7` |
| ImmUnityPlugin | " | macOS | `Plugins/OSX/ImmUnityPlugin.bundle/Contents/MacOS/ImmUnityPlugin` | Mach-O | 2776 | 97 | 3.36 MB | `288f1802eb4abe0d` |
| ImmStrokeReader | `com.immersive-foundation.imm-stroke-reader` | Windows x86_64 | `Plugins/x86_64/ImmStrokeReader.dll` | PE | 38 | 38 | 343 KB | `4d39d985393ac9fd` |
| ImmStrokeReader | " | Android arm64-v8a | `Plugins/Android/arm64-v8a/libImmStrokeReader.so` | ELF | 2071 | 38 | 7.17 MB | `4055acda5b5148e1` |
| ImmStrokeReader | " | iOS (static) | `Plugins/iOS/libImmStrokeReader.a` | ar | 125 | 38 | 86 KB | `a4cebc357ad5357d` |
| ImmStrokeReader | " | macOS | `Plugins/macOS/libImmStrokeReader.dylib` | Mach-O | 1460 | 38 | 1.63 MB | `9284550c60929172` |

"Plugin API" counts the plugin's own `extern "C"` entry points present in that binary.
The larger "all symbols" figure is the whole default-visibility symbol table: on Android,
iOS and macOS the plugins are built without an export map or `-fvisibility=hidden`, so
every C++ symbol of the statically linked `libImmCore` / `libImmImporter` / `libImmPlayer`
is exported as well. Only the `extern "C"` names are a supported interface.

Every platform now builds the full API: the 21 `ImmExporter_*` entry points are part of
the Android, iOS and macOS binaries as well, since `libImmExporter` (with the vendored
libopusenc for Opus audio) is wired into all four build systems. The table above is from
the binary sync in commit `32e99926`, the first one to carry the exporter on all four
platforms, so every row's "Plugin API" column is the same 97 (or 38) entry points.

Local build outputs under the `exe/` folders (not shipped; only the stroke reader DLL here
is tracked in git):

| Path | Exports | Note |
|---|---|---|
| `code/appImmUnity/exe/ImmUnityPlugin.dll` | 97 | current; identical export set to the shipped package DLL |
| `code/appImmUnity/exe/Release/ImmUnityPlugin.dll` | 60 | leftover from an earlier output layout; both configs now write the flat `exe\` path |
| `code/appImmStrokeReader/exe/ImmStrokeReader.dll` | 38 | current; rebuilt by CI and committed, same export set as the shipped package DLL |

Windows runtime dependencies (from the PE import tables):

- `ImmUnityPlugin.dll`: `Audio360.dll`, `opusenc.dll`, `zlib1.dll`, `jpeg62.dll`, `libpng16.dll`, `ogg.dll`, `vorbis.dll`, `vorbisenc.dll`, plus system `d3d11`/`opengl32`/`DSOUND`.
- `ImmStrokeReader.dll`: `zlib1.dll`, `jpeg62.dll`, `libpng16.dll`, `ogg.dll`, `vorbis.dll`.

Each package ships only its own subset, so `imm-unity` alone cannot load on Windows —
the `zlib1`/`jpeg62`/`libpng16`/`ogg`/`vorbis` DLLs come from the stroke-reader package.
This is why the READMEs require installing the stroke reader first (see
[G10](#g10--imm-unity-depends-on-the-stroke-reader-package-for-its-windows-dll-dependencies)).

---

## 2. ImmUnityPlugin

Source: `code/appImmUnity/src/main.cpp` (~3100 lines) plus the shared engine bridge
`code/appImmShared/src/imm_engine_bridge.cpp` — the same bridge file is compiled into the
Godot plugin, so plugin-side changes affect both engines.

### 2.1 Responsibilities

1. Unity graphics-plugin callbacks (`UnityPluginLoad`/`UnityPluginUnload`,
   `GetRenderEventFunc`, `GetRenderEventAndDataFunc`, `ConfigureVulkanRenderEvent`).
2. Renderer selection and ownership: D3D11/D3D12 through `IUnityGraphicsD3D11`,
   OpenGL/GLES, Metal through `IUnityGraphicsMetal`, Vulkan through
   `IUnityGraphicsVulkanMinimal`. `IMM_UNITY_VULKAN` is defined in source
   (`main.cpp:107`) for `WINDOWS || __ANDROID__ || ANDROID` only; Metal is selected by
   `#if defined(__APPLE__)`.
3. Playback: document load/unload, playback state, time, chapters, volume, bounding box.
4. Scene-graph editing: per-layer visibility/opacity/transform overrides and diagnostics.
5. Spawn areas / viewpoints: enumeration, active area, pose, screenshot, "needs update" signal.
6. Paint authoring and export (`ImmExporter_*`), Windows only.
7. Runtime flag plumbing (`SetRuntimeFlag`) so raw `getenv` toggles work on Android.

### 2.2 Native API (97 entry points)

`C#` column: `Native` = `Runtime/ImmNativePlugin.cs`, `Export` = `Runtime/ImmExporter.cs`.
"callers" lists managed code that actually invokes it; `—` means the P/Invoke exists but
nothing in the repository calls it.

#### Unity graphics-plugin callbacks

| Export | C# | Notes |
|---|---|---|
| `UnityPluginLoad` | not declared | Unity calls it directly; not a C# P/Invoke |
| `UnityPluginUnload` | not declared | " |
| `GetRenderEventFunc` | Native | `ImmPlayerManager` |
| `GetRenderEventAndDataFunc` | Native | `ImmPlayerManager` |
| `ConfigureVulkanRenderEvent` | Native | `ImmPlayerManager` (Android/Windows Vulkan) |

On iOS the plugin binary cannot register itself; `Plugins/iOS/ImmUnityPluginRegister.mm`
calls `UnityRegisterRenderingPluginV5(UnityPluginLoad, UnityPluginUnload)` and is
triggered from C# by `ImmUnityRegisterRenderingPlugin()` (iOS-only `#if` in
`ImmNativePlugin.cs:39`).

#### Lifecycle, frame work, cameras

| Export | C# | callers |
|---|---|---|
| `Init(colorSpace, antialiasing, logFileName, tmpFolderName)` | Native | `ImmPlayerManager`, `ImmExporter` |
| `End()` | Native | `ImmPlayerManager` |
| `GlobalWork(enabled)` | Native | `ImmPlayerManager` |
| `PrepareCamera(cameraID)` | Native | `ImmPlayerManager` |
| `SetMatrices(cameraID, stereoType, 6 matrices)` | Native | `ImmPlayerManager` |
| `SetCameraViewport(cameraID, width, height)` | Native | `ImmPlayerManager` |
| `SetVulkanCameraRenderBuffers(...)` | Native | `ImmPlayerManager` |
| `SetVulkanCameraEyeRenderBuffers(...)` | Native | `ImmPlayerManager` |
| `IsReadyForDocumentLoad()` | Native | `ImmPlayerManager` |
| `SetVulkanDedicatedQueueAllowed(allowed)` | Native | `ImmPlayerManager` |
| `Debug()` | Native | — |
| `SetRuntimeFlag(name, value)` | Native | `ImmPlayerManager` |

#### Documents, layers, playback

| Export | C# | callers |
|---|---|---|
| `LoadFromFile(fileName)` | Native | `ImmPlayerManager` |
| `LoadFromMemory(fileName, size, data)` | Native | `ImmPlayerManager` |
| `IsDocumentActive(id)` | Native | `ImmPlayerManager` |
| `Unload(id)` | Native | `ImmDocument`, `ImmPlayerManager` |
| `SetDocumentToWorld(id, doc2world)` | Native | `ImmDocument` |
| `IsSequenceReady(docId)` | Native | `ImmDocument`, `ImmPlayerManager` |
| `GetLayerCount` / `GetLayerInfoByIndex` | Native | `ImmDocument` |
| `SetLayerVisible` / `ClearLayerVisibilityOverride` | Native | `ImmDocument` |
| `SetLayerOpacity` | Native | `ImmDocument` |
| `SetLayerTransform` / `ClearLayerTransformOverride` | Native | `ImmDocument` |
| `GetLayerDiagnostics` | Native | `ImmDocument` |
| `Pause` / `Resume` / `Hide` / `Show` / `Continue` | Native | `ImmDocument`, `ImmAuthoringPreviewCoordinator` |
| `SkipForward` / `SkipBack` / `SetChapter` / `Restart` | Native | `ImmDocument` |
| `GetChapterCount` / `GetCurrentChapter` | Native | `ImmDocument` |
| `SetTime` / `GetTime` / `GetPlayTime` | Native | `ImmDocument`, `ImmAuthoringPreviewCoordinator` |
| `GetPlayerInfo` / `GetDocumentState` / `GetDocumentInfoEx` | Native | `ImmDocument`, `ImmPlayerManager` |
| `GetSound` / `SetSound` | Native | `ImmDocument` |
| `GetBoundingBox` | Native | `ImmDocument` |

#### Spawn areas

| Export | C# | callers |
|---|---|---|
| `GetSpawnAreaCount` / `GetSpawnAreaList` | Native | `ImmDocument` |
| `GetActiveSpawnAreaId` / `SetActiveSpawnAreaId` | Native | `ImmDocument` |
| `GetInitialSpawnAreaId` | Native | `ImmDocument` |
| `GetSpawnAreaInfo` | Native | `ImmDocument` |
| `GetSpawnAreaPose` | Native | — |
| `GetSpawnAreaNeedsUpdate` / `SetSpawnAreaNeedsUpdate` | Native | — |

#### Exporter (Windows only)

| Export | C# | callers |
|---|---|---|
| `ImmExporter_CreateSequence` / `DestroySequence` | Export | `ImmExporter`, `ImmAuthoringCompiler` |
| `ImmExporter_CreatePaintLayer` / `CreateGroupLayer` | Export | `ImmExporter`, `ImmAuthoringCompiler` |
| `ImmExporter_CreateSpawnAreaLayer` | Export | `ExportSequence` |
| `ImmExporter_SetInitialSpawnArea` / `SpawnAreaSetProperties` | Export | `ExportSpawnAreaLayer`, `ImmAuthoringCompiler` |
| `ImmExporter_CreateDrawing` / `DestroyDrawing` / `GetDrawingIndex` | Export | `ImmExporter`, `ImmAuthoringCompiler` |
| `ImmExporter_DrawingInit` / `DrawingGetElement` | Export | `ImmExporter`, `ImmAuthoringCompiler` |
| `ImmExporter_ElementInit` / `ElementSetPoint` / `ElementSetPoints` | Export | `ImmExporter`, `ImmAuthoringCompiler` |
| `ImmExporter_ComputeElementBounds` / `ComputeDrawingBounds` | Export | `ImmExporter`, `ImmAuthoringCompiler` |
| `ImmExporter_PaintAddFrame` | Export | `ImmExporter`, `ImmAuthoringCompiler` |
| `ImmExporter_LayerAddAnimationKey` | Export | `ImmAuthoringCompiler` |
| `ImmExporter_PaintSetMaxRepeatCount` | Export | `ImmAuthoringCompiler` |
| `ImmExporter_ExportToFile` / `ExportToMemory` | Export | `ImmExporter` |
| `ImmExporter_GetMemoryData` / `GetMemorySize` / `DestroyMemory` | Export | `ImmExporter` |

#### Entry points added by the gap fixes

| Export | Managed surface |
|---|---|
| `GlobalWorkEx(enabled, budgetMicroseconds)` | `ImmPlayerManager.GlobalWorkBudgetMicroseconds` |
| `SetCameraViewportEx(cameraID, x, y, width, height, minDepth, maxDepth, forceViewport)` / `ClearCameraViewport` | `ImmPlayerManager.SetCameraViewport` / `ClearCameraViewport` |
| `GetChapterInfoEx(id, int64_t* lengths, int maxChapters, int* hasPlays)` | `ImmDocument.TryGetChapterInfo` |
| `GetDocumentHasAudio` / `CancelDocumentLoad` / `PauseAt` / `ResumeAt` | `ImmDocument.HasAudio` / `CancelLoading` / `PauseAt` / `ResumeAt` |
| `GetLoadTimeInMs` / `UnloadAll(sync)` | `ImmPlayerManager.LastLoadTimeMs` / `UnloadAllDocuments` |
| `SetPerformanceMeasurementEnabled` / `GetPerformanceInfo` | `ImmPlayerManager.SetPerformanceMeasurementEnabled` / `GetPerformanceInfo` |
| `ImmExporter_LayerAddAnimationKey` / `ImmExporter_PaintSetMaxRepeatCount` | `ImmAuthoringCompiler` |
| `ImmExporter_CreateSpawnAreaLayer` / `ImmExporter_SetInitialSpawnArea` / `ImmExporter_SpawnAreaSetProperties` | `ExportSequence.CreateSpawnAreaLayer` / `SetInitialSpawnArea`, `ExportSpawnAreaLayer.SetVolume`, `ImmAuthoringDocument.CreateSpawnAreaLayer` |
| `StrokeReader_GetBuildId` | `ImmStrokeReader.GetBuildId()` |
| `StrokeReader_GetDrawingBiggestStroke` | `StrokeReaderDocument.TryGetDrawingBiggestStroke` |
| `StrokeReader_GetLayerSpawnAreaInfo` | `StrokeReaderDocument.GetLayerSpawnAreaInfo`, `ImmAuthoringImporter` |

### 2.3 Renderer / platform behaviour worth knowing

- **Windows D3D/GL**: `Init` creates the renderer immediately and takes Unity's device.
- **Windows Vulkan**: requires the `IUnityGraphicsVulkan` instance/device/queue from
  Unity; returns `-1` if unavailable. `externalDepthReverseZ = true`,
  `initializeFullscreen = false`.
- **Android**: renderer creation is deferred to the render thread
  (`IsReadyForDocumentLoad()` reports readiness only after the deferred init completes).
  Vulkan hands IMM Unity's device/queue; otherwise GLES is used. Flat Android Vulkan
  composition renders into explicit Unity `RenderTexture` slots rather than the display
  buffer (see the package README).
- **Apple**: Metal with `metalUnityProjectionAdjusted`, `reverseDepthBuffer`,
  `overrideFrontIsCCW = true`; OpenGL core is the only alternative. Unsupported renderers
  return `-1` from `Init`.
- **`GlobalWork`** passes the historical 9000 µs work budget; `GlobalWorkEx` (and
  `ImmPlayerManager.GlobalWorkBudgetMicroseconds`) carry an explicit one.
- **`SetCameraViewport`** only stores width/height and only on `__APPLE__`
  (`main.cpp:1602`); `SetCameraViewportEx` sets origin, sub-rect size, depth range and the
  force flag on every platform, and `ClearCameraViewport` restores the default.

### 2.4 Data structures (ABI verified against the native definitions)

| C# struct | Native | Layout |
|---|---|---|
| `PlayerInfo` | `Player::PlayerInfo` (`player.h:59`) | 3 floats (`mBackgrundColor` → `backgroundColor`) |
| `DocumentState` | `Player::DocumentState` (`player.h:200`) | 2 int enums |
| `LayerDiagnosticsNative` | `Player::LayerDiagnostics` (`player.h:119`) | 9 int + 2 float |
| `LayerInfoNative` | `UnityLayerInfo` (`main.cpp:2018`) | 6 int + float + int + `bound3` (6 floats) + 5 int + `char16_t[128]` + `char16_t[256]`; marshalled as `ByValTStr` with `CharSet.Unicode` |
| `Bounds3` / `LayerBounds3` | `ImmCore::bound3` | 6 floats (min/max per axis) |
| `SerializedSpawnArea` | `main.cpp:2129` | pointer, int, 2 enums, `bool`, 32-byte volume, 32-byte transform, int, 24-byte screenshot |
| `SpawnAreaPose` | `SerializedSpawnAreaPose` (`main.cpp:2285`) | 8 floats + 3 int |
| `TransformNative`, `PointNative` | `ImmExporterTransformC`, `ImmExporterPointC` (`main.cpp:2327`) | exporter transform / point records |

`GetSpawnAreaInfo` marshals two pointers rather than copies: the name is a **static**
256-byte buffer that the next call overwrites (`main.cpp:2194`), and the screenshot
pointer references the importer's image data, so it is valid only while that image is
loaded.

### 2.5 Runtime flags

Raw `getenv` toggles read by the plugin (all overridable at runtime through
`SetRuntimeFlag`, which `ImmPlayerManager` feeds from
`<persistentDataPath>/imm_debug_flags.txt` on Android):

`IMM_UNITY_VK_DONTCARE_RENDERPASS`, `IMM_UNITY_VK_NO_MODIFIES_STATE`,
`IMM_UNITY_VK_ENSURE_PREVIOUS`, `IMM_UNITY_VK_NO_ENSURE_INSIDE`,
`IMM_UNITY_VK_QUERY_STATE_ONLY`, `IMM_UNITY_VK_QUERY_STATE_NO_LOG`,
`IMM_UNITY_VK_QUERY_RESTORE_INSIDE`, `IMM_UNITY_VK_HOST_COLOR_FORMAT`,
`IMM_UNITY_VK_ASSUME_HOST_DEPTH`, `IMM_UNITY_VK_NO_HOST_DEPTH_ATTACHMENT`,
`IMM_UNITY_VK_USE_HOST_DEPTH`, `IMM_UNITY_VK_DEBUG_HOST_CLEAR`,
`IMM_UNITY_VK_DEBUG_HOST_CLEAR_ONLY`, `IMM_UNITY_VK_BEGIN_END_ONLY`,
`IMM_UNITY_VK_SKIP_HOST_RENDER`, `IMM_UNITY_VK_SAMPLE_EVENT1`,
`IMM_UNITY_VK_FORCE_HOST_RENDER`, `IMM_UNITY_VK_FORCE_EXTERNAL_IMAGE`,
`IMM_UNITY_METAL_OFFSCREEN`, `IMM_UNITY_METAL_USE_PLUGIN_COMMAND_BUFFER`,
`IMM_UNITY_METAL_USE_OWNED_ENCODER`, `IMM_UNITY_METAL_NOOP_EVENT`,
`IMM_UNITY_METAL_SKIP_DRAW`.

The shared engine bridge additionally reads `IMM_GODOT_NATIVE_CAPTURE_PATH` and
`IMM_GODOT_DEBUG_CAMERA` (`imm_engine_bridge.cpp:249`, `:302`) — the Godot-flavoured
names apply to the Unity plugin too.

C#-side flags read by `ImmPlayerManager`: `IMM_UNITY_FORCE_CAMERA_CALLBACK`,
`IMM_UNITY_VK_SAMPLE_WAIT_FOR_END_OF_FRAME`.

None of these flags is documented in the packages.

---

## 3. ImmStrokeReader

Source: `code/appImmStrokeReader/src/main.cpp` + `strokeStore.cpp/.h`. It links only
`libImmImporter` and `libImmCore` (no player, no renderer, no exporter) and reads an IMM
file into a plain in-memory stroke/layer graph. All 38 entry points exist on all four
platforms.

### 3.1 Native API (38 entry points)

All are declared in `Runtime/ImmStrokeReader.cs`.

| Group | Entry points |
|---|---|
| Build/init | `StrokeReader_GetBuildId`, `Init`, `IsInitialized`, `End` (—), `GetDocumentCount` |
| Load | `LoadFromFile`, `LoadFromMemory`, `Unload` |
| Document | `GetDocumentInfo` |
| Layers | `GetLayerCount`, `GetLayerInfo`, `GetLayerTransform` |
| Layers (authoring view) | `GetAuthoringLayerCount`, `GetAuthoringLayerInfo`, `GetAuthoringLayerTransform` |
| Spawn area | `GetLayerSpawnAreaInfo` |
| Animation | `GetLayerAnimationKeyCount`, `GetLayerAnimationKey`, `GetLayerAnimationInfo`, `GetAuthoringLayerAnimationInfo` |
| Drawings / frames | `GetDrawingCount`, `GetDrawingIndexForChapter`, `GetFrameBuffer`, `GetAuthoringDrawingCount`, `GetAuthoringFrameBuffer` |
| Strokes | `GetStrokeCount`, `GetStrokeInfo`, `GetStrokePoints`, `GetAuthoringStrokeCount`, `GetAuthoringStrokeInfo`, `GetAuthoringStrokePoints`, `GetDrawingBiggestStroke` |
| Pictures | `GetPictureInfo`, `GetPicturePixelData` |
| Chapters | `GetChapterCount`, `GetChapterCountFromFile`, `GetCurrentChapter`, `SetChapter` |

`GetBuildId` returns `IMM_STROKE_READER_BUILD_ID=2026-07-19-PHASE5` — the intended way to
confirm which native build Unity actually loaded.

### 3.2 Managed wrapper

`Runtime/ImmStrokeReader.cs` (bindings + `StrokeReaderDocument`), `Runtime/SharpQuillCompat.cs`
(adapter exposing the stroke data through the vendored SharpQuill object model in
`Runtime/ThirdParty/SharpQuill/`), and editor tooling
(`Editor/ImmStrokeReaderTestEditor.cs`, `Editor/ImmToQuillConverterEditor.cs`).

---

## 4. C# packages

| Package | Namespace | Assembly | Runtime files |
|---|---|---|---|
| `com.immersive-foundation.imm-unity` | `ImmPlayer` | `ImmUnity.Runtime` | `ImmNativePlugin.cs` (P/Invoke), `ImmPlayerManager.cs` (MonoBehaviour, rendering), `ImmDocument.cs` (per-document API), `ImmExporter.cs` (paint export + `Native` class), `Authoring/*` (mutable graph, compiler, importer, preview coordinator, capabilities) |
| `com.immersive-foundation.imm-stroke-reader` | `ImmPlayer` | `ImmStrokeReader.Runtime` | `ImmStrokeReader.cs`, `SharpQuillCompat.cs`, `ThirdParty/SharpQuill/*`, `Editor/*` |

Dependency: `imm-unity`'s `package.json` requires `imm-stroke-reader` `0.1.0`, and
`ImmUnity.Runtime.asmdef` references that package's assembly by GUID
(`09b9ff65b0a14d8a8bafc507b3d5b6f1`). Both packages declare the same C# namespace
`ImmPlayer`, which is why `ImmAuthoringImporter` can call
`ImmStrokeReader.StrokeReader_GetAuthoringLayerAnimationInfo` directly.

Managed entry surface: `ImmPlayerManager` (singleton MonoBehaviour: `Initialize`,
`Shutdown`, `LoadDocument`, `LoadDocumentFromMemory`, `UnloadDocument`, `SetCameraMatrices`,
`SetStereoCameraMatrices`, `IssueRenderEvent`, `GetPlayerInfo`, plus Android-Vulkan
validation hooks) and `ImmDocument` (state, chapters, time, volume, transform, bounding
box, spawn areas, layers).

`code/ImmStrokeReaderUPM/` is **not** a package: it contains only a `package.json`
(without the Newtonsoft dependency) and a short README, references a
`Samples~/Examples` folder that does not exist, and has no `Runtime/` or `Plugins/`. The
live package is the one under `code/ImmUnitySampleProject/Packages/`.

---

## 5. Build and packaging

| Platform | Project | Output | Copy into the package |
|---|---|---|---|
| Windows x64 | `code/projects/windows/imm.sln` → `appImmUnity.vcxproj`, `appImmStrokeReader.vcxproj` (both `OutDir` = `exe\`) | `ImmUnityPlugin.dll`, `ImmStrokeReader.dll` | `PostBuildEvent` copy into `Plugins/x86_64` |
| Android arm64-v8a | Gradle + CMake (`code/appImmUnity/Projects/Android`, `code/appImmStrokeReader/Projects/Android`) | `libImmUnityPlugin.so`, `libImmStrokeReader.so` | `copyToUnity` Gradle task, attached with `finalizedBy` to `assembleDebug`/`assembleRelease` |
| iOS | CMake (`code/projects/ios`) with an `xcrun libtool -static` POST_BUILD merge of the archive set | `libImmUnityPlugin.a`, `libImmStrokeReader.a` | copy into `Plugins/iOS` (skipped for the iphonesimulator sysroot) |
| macOS | CMake (`code/projects/macos`): `ImmUnity` as `MODULE`/`BUNDLE`, `ImmStrokeReader` as `SHARED` | `ImmUnityPlugin.bundle`, `libImmStrokeReader.dylib` | copy into `Plugins/OSX`, `Plugins/macOS` |

Defines: Windows gets `WIN32;NDEBUG;_WINDOWS;_USRDLL;RENDERINGPLUGIN_EXPORTS`
(`IMMSTROKEREADER_EXPORTS` for the stroke reader) and `WINDOWS` from
`code/projects/windows/ImmCommonPropertySheet.props`; Android adds `-D__ANDROID__ -DANDROID`
(unity) and `-DANDROID -Werror` (stroke reader); iOS adds `IMM_IOS=1`; macOS adds no
plugin-specific define. Exports are `__declspec(dllexport)` / `visibility("default")` —
there is no `.def` file or export list, which is why the non-Windows libraries expose
their full symbol tables.

Linked libraries: Windows `ImmUnityPlugin` links `libImmPlayer` + `libImmImporter` +
`libImmExporter` + `libImmCore`; every other configuration links only the first, second
and fourth (plus codecs). `libImmExporter` appears in no other build file.

CI (`.github/workflows/build.yml`) builds all four platforms, assembles
`ImmStrokeReaderPlugin-Unity.zip` / `ImmPlayerPlugin-Unity.zip`, and the `sync-binaries`
job **commits the packaged binaries back into the repository** as
`chore: update committed runtime binaries (Unity + Godot + Web) [skip ci]`. `publish-upm`
regenerates a `upm` branch; the `release` job runs `gh release create`.

Existing CI verification only checks that the Windows plugin exists, is non-empty
(`tests/tools/verify_package_layout.py`) and exports three names — `GetRenderEventFunc`,
`GetRenderEventAndDataFunc`, `ConfigureVulkanRenderEvent`
(`ci-engine.yml:99-126`). Nothing verifies the rest of the API, which is how
[G1/G2](#g1--impexporter_layeraddanimationkey-is-declared-and-called-but-does-not-exist) went unnoticed.

---

## 6. Gaps

Severity: **S1** broke a shipped code path, **S2** capability the binary had and the
wrapper could not reach, **S3** integration/packaging/documentation defect.

All S1 and S2 gaps are fixed (see the fix log below); the S3 defects that remain open
are kept in full detail after it.

| # | Severity | Gap | Status |
|---|---|---|---|
| [G1](#fix-log) | S1 | `ImmExporter_LayerAddAnimationKey` called by C#, absent from every binary and from native source | Fixed |
| [G2](#fix-log) | S1 | `ImmExporter_PaintSetMaxRepeatCount` — same | Fixed |
| [G3](#fix-log) | S2 | 21 exporter entry points existed only on Windows | Fixed |
| [G4](#fix-log) | S2 | `ImmExporter_CreateSpawnAreaLayer` exported, no C# binding, no authoring layer type | Fixed |
| [G5](#fix-log) | S2 | `GlobalWork` budget hardcoded to 9000 µs | Fixed |
| [G6](#fix-log) | S2 | viewport x/y/minDepth/maxDepth/forceViewport unreachable; export was Apple-only | Fixed |
| [G7](#fix-log) | S2 | `GetSpawnAreaPose`, `GetSpawnAreaNeedsUpdate`/`SetSpawnAreaNeedsUpdate` wrapped but never called | Fixed |
| [G8](#fix-log) | S2 | screenshot pointer/format/size returned by `GetSpawnAreaInfo`, dropped by the wrapper | Fixed |
| [G9](#fix-log) | S2 | chapter lengths, audio presence, cancel-loading, load time, perf counters, unload-all, pause/resume-at-tick not exposed | Fixed |
| G10 | S3 | `ImmUnityPlugin.dll` imports DLLs only the stroke-reader package ships | Open |
| [G11](#fix-log) | S2 | stroke-reader build id not reachable from C# | Fixed |
| [G12](#fix-log) | S2 | biggest-stroke query not reachable from C# | Fixed |
| G13 | S3 | managed code never calls `StrokeReader_End` | Open |
| G14 | S3 | `Debug()` unused | Open |
| G15 | S3 | `code/appImmStrokeReader/exe/ImmStrokeReader.dll` was 9 entry points behind | Fixed |
| G16 | S3 | CI does not compare P/Invokes against exports | Fixed |
| G17 | S3 | no reference doc for the entry points or the runtime flags | Fixed (this document) |
| G18 | S3 | `code/ImmStrokeReaderUPM/` is an empty package skeleton | Open |
| G19 | S3 | nothing compiles the Unity package C# on push | Local tool added; CI gate open |

### Fix log

Each entry names the change that closed the gap. Commits are on `main` in the order
listed.

- **G1 / G2 (S1).** `ImmExporter_LayerAddAnimationKey` and
  `ImmExporter_PaintSetMaxRepeatCount` are implemented in `appImmUnity/src/main.cpp` on
  top of the public `libImmExporter` API (`Layer::AddKey`, `Layer::SetMaxRepeatCount`),
  with null-handle, property-range and interpolation-range validation plus a paint-only
  guard for the repeat count. `code/appImmUnity/tests/exporter_bridge_smoke.py` exercises
  the positive and rejection paths and re-reads the exported file through
  `ImmStrokeReader` to confirm the repeat count and both key values survive
  serialization.
- **G3 (S2).** The exporter block and its includes are no longer Windows-only. The
  libopusenc 0.2.1 sources are vendored under `thirdparty/libopusenc-src` (the
  pre-existing `thirdparty/libopusenc` holds Windows binaries only) with a CMake target;
  `piWaveOPUS.cpp` and opus/opusenc are part of the Android, iOS and macOS core builds;
  `libImmExporter` has an Android gradle/CMake module and is linked into the iOS archive
  merge and the macOS bundle. `toImmersiveLayer.cpp` needed a `MAX` case in its animation
  property switch, which clang's `-Wswitch` rejects under `-Werror`.
  `ImmAuthoringRuntime.Capabilities` now reports the authoring features on every platform.
- **G4 (S2).** `ExportSequence.CreateSpawnAreaLayer` / `SetInitialSpawnArea` and an
  `ExportSpawnAreaLayer` wrapper (sphere or box volume, offset, radii, per-axis
  locomotion) expose the native entry points, and the new
  `ImmExporter_SetInitialSpawnArea` / `ImmExporter_SpawnAreaSetProperties` exports give the
  host explicit control; creating a spawn area now claims the sequence default only while
  none is set. On the authoring side `ImmAuthoringLayerType.SpawnArea` is a first-class
  layer with viewpoint properties, validation, compiler support (including default
  selection), importer support and structural comparison. The smoke test authors two
  viewpoints and verifies tracking level, radii, locomotion masks and default selection
  round-trip.
- **G5 (S2).** `GlobalWorkEx(enabled, budgetMicroseconds)` (negative budgets clamped to 0)
  and `ImmPlayerManager.GlobalWorkBudgetMicroseconds` replace the hardcoded 9000.
- **G6 (S2).** `SetCameraViewportEx(cameraID, x, y, width, height, minDepth, maxDepth,
  forceViewport)` and `ClearCameraViewport` work on every platform through a resolver that
  reproduces the previous viewport exactly when no override is set; the Vulkan
  render-buffer paths keep the bound buffer's size and only take origin, depth range and
  the force flag.
- **G7 (S2).** `ImmDocument` exposes `TryGetSpawnAreaPose`, `TryGetActiveSpawnAreaPose`,
  `GetSpawnAreaNeedsUpdate`, `SetSpawnAreaNeedsUpdate` and `ConsumeSpawnAreaNeedsUpdate`,
  and the world/view-target pose helpers now use the cheap pose query instead of the
  name-and-screenshot `GetSpawnAreaInfo` path.
- **G8 (S2).** `ImmDocument.TryGetSpawnAreaThumbnail` copies the native screenshot pixels
  into a `Texture2D` with the matching Unity format.
- **G9 (S2).** New entry points `GetChapterInfoEx` (chapter lengths in ticks plus
  `hasPlays`, releasing the temporary array `Player::GetChapterInfo` allocates),
  `GetDocumentHasAudio`, `CancelDocumentLoad`, `GetLoadTimeInMs`, `UnloadAll(sync)`,
  `PauseAt`/`ResumeAt` and `SetPerformanceMeasurementEnabled`/`GetPerformanceInfo` with a
  marshaled numeric `PerformanceInfo` subset, surfaced as `ImmDocument.TryGetChapterInfo`,
  `HasAudio`, `CancelLoading`, `PauseAt`/`ResumeAt` and
  `ImmPlayerManager.LastLoadTimeMs`, `UnloadAllDocuments`,
  `SetPerformanceMeasurementEnabled`, `GetPerformanceInfo`.
- **G11 / G12 (S2).** `ImmStrokeReader.GetBuildId()` and
  `StrokeReaderDocument.TryGetDrawingBiggestStroke` wrap the two entry points that had no
  binding; the smoke test asserts the build-id prefix and re-reads the drawing's
  biggest-stroke value (the largest element bounding-box extent the format stores per
  drawing — not a brush width, which the first draft of these docs claimed).
- **G15 (S3).** Rebuilding `appImmStrokeReader` refreshed the tracked
  `code/appImmStrokeReader/exe/ImmStrokeReader.dll`, which was nine entry points behind.
- **G17 (S3).** This document is the reference for the entry points, the ABI structs, the
  render-event contract and the runtime flags.

### G10 — imm-unity depends on the stroke-reader package for its Windows DLL dependencies

`ImmUnityPlugin.dll` imports `zlib1.dll`, `jpeg62.dll`, `libpng16.dll`, `ogg.dll`,
`vorbis.dll` (plus `Audio360.dll`, `opusenc.dll`, `vorbisenc.dll`, which it does ship).
The imm-unity package's `Plugins/x86_64` contains only the latter three plus
`Audio360.dll`; the missing five are shipped by the stroke-reader package. Installing
imm-unity without the stroke reader leaves a Windows player that cannot load the plugin.
The dependency is declared in `package.json` and in the README, but nothing verifies it —
and the two packages use different plugin folder conventions (`OSX/*.bundle` vs
`macOS/*.dylib`, `Android/libs/arm64-v8a` vs `Android/arm64-v8a`).

### G13 — `StrokeReader_End` is never called

Declared (`ImmStrokeReader.cs:38`) but never invoked, so the native document map and log
are never torn down from managed code (domain reload / play-mode exit leaves them to
process teardown). `StrokeReaderDocument` is `IDisposable` for individual documents, but
the static `ImmStrokeReader` binding class offers no shutdown path that calls
`StrokeReader_End`.

### G14 — `Debug()` is unused

`ImmNativePlugin.Debug()` (`main.cpp:1515` logs renderer initialisation state) has no
caller. Useful when diagnosing "plugin loaded but renderer absent" on device.

### G16 — no automated export-vs-DllImport check

Closed. `ci-engine.yml:99-126` asserts three export names via `dumpbin`, and
`tests/tools/verify_package_layout.py` checks existence and non-zero size, but neither
compared the C# `[DllImport]` set against a plugin's export table — which is how G1/G2 (a
declared entry point missing from every binary) survived.

`tests/tools/verify_unity_plugin_exports.py` performs the missing check offline for all four
platforms, and `build.yml` now runs it twice, both times after fresh binaries have been
staged into the package folders it reads:

- `package-unity-plugins`, which runs once per run after all four platform artifacts are
  synced into the packages — this gates the UPM zips on every trigger, including PRs and the
  daily schedule;
- `sync-binaries`, before its commit — this refuses to publish a binary refresh that would
  leave a declaration unexported.

Both invocations are non-strict, so the documented known gap (`ImmUnityRegisterRenderingPlugin`
on iOS, which Unity compiles from `Plugins/iOS/ImmUnityPluginRegister.mm`) is reported without
failing. They read the freshly built binaries inside the job workspace rather than the
committed ones, so a native change does not turn the pipeline red while the Android/iOS/macOS
binaries are still waiting for their binary sync.

### G18 — `ImmStrokeReaderUPM/` is a broken package skeleton

Contains `package.json` (version `0.1.0`, no `com.unity.nuget.newtonsoft-json`
dependency) and `README.md` advertising `Plugins/`, `Runtime/` and `Samples~/Examples`
that do not exist. Its manifest's `samples[0].path` points at a missing folder. It is not
the shipped package, but it sits at a plausible install path.

### G19 — nothing compiles the Unity package C# on push

No job on a push or pull request compiles the C# in `com.immersive-foundation.imm-unity`.
`build.yml` compiles the stroke-reader package's `ImmStrokeReader.cs` through the
SharpQuill adapter (`dotnet run --project .../SharpQuillAdapter.csproj`, which is how G1/G2
and a duplicate `GetBuildId` were caught), and everything else the Unity packages contain
reaches a compiler only in the engine-phase Unity jobs — which run on the daily schedule and
on `full`/`hardware`/`release` dispatches, not on an ordinary push.

That gap shipped a real error: commit `037e9a44` mapped a spawn-area screenshot to
`TextureFormat.RGBFloat`, which does not exist, so the entire package failed to compile
(CS0117) and stayed that way through several pushes. It was found by
`tests/tools/verify_unity_csharp.py`, which compiles the package runtimes with MSBuild
against the Editor's own managed assemblies using the `.csproj` files Unity writes into the
project. That tool is the local mitigation; it is not wired into CI because it needs both
MSBuild and a Unity installation on the runner, and the generated `.csproj` files are not
committed. Closing this properly means a Windows job that installs Unity and runs the
compile (or `unity test` in EditMode) on push.

### Related documentation drift (not plugin gaps)

- `README.md:163` states the macOS ImmUnity build "is currently disabled in CI";
  `build.yml:501` builds `--target ImmUnity` on `macos-14`.
- The committed `ImmUnityPlugin.bundle`'s `_CodeSignature/CodeResources` predates the
  committed binary (signature no longer covers it) — `sync-binaries` refreshes the bundle
  binary but not its `_CodeSignature`. The export sets themselves are current on all four
  platforms as of `32e99926`.

---

## 7. Appendix — full export lists

`ImmUnityPlugin` (97 on Windows, same API set on Android, iOS and macOS):

```
CancelDocumentLoad ClearCameraViewport ClearLayerTransformOverride ClearLayerVisibilityOverride
ConfigureVulkanRenderEvent Continue Debug End
GetActiveSpawnAreaId GetBoundingBox GetChapterCount GetChapterInfoEx
GetCurrentChapter GetDocumentHasAudio GetDocumentInfoEx GetDocumentState
GetInitialSpawnAreaId GetLayerCount GetLayerDiagnostics GetLayerInfoByIndex
GetLoadTimeInMs GetPerformanceInfo GetPlayTime GetPlayerInfo
GetRenderEventAndDataFunc GetRenderEventFunc GetSound GetSpawnAreaCount
GetSpawnAreaInfo GetSpawnAreaList GetSpawnAreaNeedsUpdate GetSpawnAreaPose
GetTime GlobalWork GlobalWorkEx Hide
ImmExporter_ComputeDrawingBounds ImmExporter_ComputeElementBounds ImmExporter_CreateDrawing ImmExporter_CreateGroupLayer
ImmExporter_CreatePaintLayer ImmExporter_CreateSequence ImmExporter_CreateSpawnAreaLayer ImmExporter_DestroyDrawing
ImmExporter_DestroyMemory ImmExporter_DestroySequence ImmExporter_DrawingGetElement ImmExporter_DrawingInit
ImmExporter_ElementInit ImmExporter_ElementSetPoint ImmExporter_ElementSetPoints ImmExporter_ExportToFile
ImmExporter_ExportToMemory ImmExporter_GetDrawingIndex ImmExporter_GetMemoryData ImmExporter_GetMemorySize
ImmExporter_LayerAddAnimationKey ImmExporter_PaintAddFrame ImmExporter_PaintSetMaxRepeatCount ImmExporter_SetInitialSpawnArea
ImmExporter_SpawnAreaSetProperties Init IsDocumentActive IsReadyForDocumentLoad
IsSequenceReady LoadFromFile LoadFromMemory Pause
PauseAt PrepareCamera Restart Resume
ResumeAt SetActiveSpawnAreaId SetCameraViewport SetCameraViewportEx
SetChapter SetDocumentToWorld SetLayerOpacity SetLayerTransform
SetLayerVisible SetMatrices SetPerformanceMeasurementEnabled SetRuntimeFlag
SetSound SetSpawnAreaNeedsUpdate SetTime SetVulkanCameraEyeRenderBuffers
SetVulkanCameraRenderBuffers SetVulkanDedicatedQueueAllowed Show SkipBack
SkipForward UnityPluginLoad UnityPluginUnload Unload
UnloadAll
```

`ImmStrokeReader` (38, identical on all four platforms):

```
StrokeReader_End StrokeReader_GetAuthoringDrawingCount StrokeReader_GetAuthoringFrameBuffer StrokeReader_GetAuthoringLayerAnimationInfo
StrokeReader_GetAuthoringLayerCount StrokeReader_GetAuthoringLayerInfo StrokeReader_GetAuthoringLayerTransform StrokeReader_GetAuthoringStrokeCount
StrokeReader_GetAuthoringStrokeInfo StrokeReader_GetAuthoringStrokePoints StrokeReader_GetBuildId StrokeReader_GetChapterCount
StrokeReader_GetChapterCountFromFile StrokeReader_GetCurrentChapter StrokeReader_GetDocumentCount StrokeReader_GetDocumentInfo
StrokeReader_GetDrawingBiggestStroke StrokeReader_GetDrawingCount StrokeReader_GetDrawingIndexForChapter StrokeReader_GetFrameBuffer
StrokeReader_GetLayerAnimationInfo StrokeReader_GetLayerAnimationKey StrokeReader_GetLayerAnimationKeyCount StrokeReader_GetLayerCount
StrokeReader_GetLayerInfo StrokeReader_GetLayerSpawnAreaInfo StrokeReader_GetLayerTransform StrokeReader_GetPictureInfo
StrokeReader_GetPicturePixelData StrokeReader_GetStrokeCount StrokeReader_GetStrokeInfo StrokeReader_GetStrokePoints
StrokeReader_Init StrokeReader_IsInitialized StrokeReader_LoadFromFile StrokeReader_LoadFromMemory
StrokeReader_SetChapter StrokeReader_Unload
```

