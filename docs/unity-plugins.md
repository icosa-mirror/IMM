# IMM Unity plugins

Reference for the two native Unity plugins in this repository, the C# packages that wrap
them, and the gaps between what the binaries export and what the C# wrappers support.

Written against `fc9665e1` (2026-09-17). Every binary figure below was read from the
committed binaries themselves, not from the build files.

How to re-check after any native or wrapper change:

```powershell
python tests/tools/verify_unity_plugin_exports.py            # every platform
python tests/tools/verify_unity_plugin_exports.py --platform windows --json out.json
```

The tool reads the `[DllImport]` entry points out of the C# packages and the export
tables out of the shipped binaries (PE, ELF, Mach-O and static archives, no external
tools required). It exits 1 on drift.

---

## 1. Plugin inventory

| Plugin | Unity package | Platform | Binary | Format | Exports (all symbols) | Plugin API | Size | sha256 (16) |
|---|---|---|---|---|---|---|---|---|
| ImmUnityPlugin | `com.immersive-foundation.imm-unity` | Windows x86_64 | `Plugins/x86_64/ImmUnityPlugin.dll` | PE | 81 | 81 | 4.42 MB | `e8fbc8fe9df642f2` |
| ImmUnityPlugin | " | Android arm64-v8a | `Plugins/Android/libs/arm64-v8a/libImmUnityPlugin.so` | ELF | 3582 | 60 | 12.90 MB | `6e265dd0d4d3bdab` |
| ImmUnityPlugin | " | iOS (static) | `Plugins/iOS/libImmUnityPlugin.a` | ar | 2759 | 60 | 4.10 MB | `a7385d0d07bdfa62` |
| ImmUnityPlugin | " | macOS | `Plugins/OSX/ImmUnityPlugin.bundle/Contents/MacOS/ImmUnityPlugin` | Mach-O | 2302 | 60 | 3.05 MB | `780bf54c7c5af444` |
| ImmStrokeReader | `com.immersive-foundation.imm-stroke-reader` | Windows x86_64 | `Plugins/x86_64/ImmStrokeReader.dll` | PE | 37 | 37 | 341 KB | `2c332f70b0a66627` |
| ImmStrokeReader | " | Android arm64-v8a | `Plugins/Android/arm64-v8a/libImmStrokeReader.so` | ELF | 2067 | 37 | 7.16 MB | `d5c7b41b9b052d2d` |
| ImmStrokeReader | " | iOS (static) | `Plugins/iOS/libImmStrokeReader.a` | ar | 123 | 37 | 84 KB | `76a2d24de61ad65e` |
| ImmStrokeReader | " | macOS | `Plugins/macOS/libImmStrokeReader.dylib` | Mach-O | 1458 | 37 | 1.63 MB | `833bc3f3aac718d8` |

"Plugin API" counts the plugin's own `extern "C"` entry points present in that binary.
The larger "all symbols" figure is the whole default-visibility symbol table: on Android,
iOS and macOS the plugins are built without an export map or `-fvisibility=hidden`, so
every C++ symbol of the statically linked `libImmCore` / `libImmImporter` / `libImmPlayer`
is exported as well. Only the `extern "C"` names are a supported interface.

Windows is the only fully-featured platform: the 21 `ImmExporter_*` entry points exist
only there (see [G3](#g3--exporter-api-exists-only-in-the-windows-binary)).

Local build outputs under the `exe/` folders (not shipped; only the stroke reader DLL here
is tracked in git):

| Path | Exports | Note |
|---|---|---|
| `code/appImmUnity/exe/ImmUnityPlugin.dll` | 81 | current; identical export set to the shipped package DLL |
| `code/appImmUnity/exe/Release/ImmUnityPlugin.dll` | 60 | leftover from an earlier output layout; both configs now write the flat `exe\` path |
| `code/appImmStrokeReader/exe/ImmStrokeReader.dll` | 28 | **tracked and stale** — predates the nine `StrokeReader_GetAuthoring*` entry points |

Windows runtime dependencies (from the PE import tables):

- `ImmUnityPlugin.dll`: `Audio360.dll`, `opusenc.dll`, `zlib1.dll`, `jpeg62.dll`, `libpng16.dll`, `ogg.dll`, `vorbis.dll`, `vorbisenc.dll`, plus system `d3d11`/`opengl32`/`DSOUND`.
- `ImmStrokeReader.dll`: `zlib1.dll`, `jpeg62.dll`, `libpng16.dll`, `ogg.dll`, `vorbis.dll`.

Each package ships only its own subset, so `imm-unity` alone cannot load on Windows —
the `zlib1`/`jpeg62`/`libpng16`/`ogg`/`vorbis` DLLs come from the stroke-reader package.
This is why the READMEs require installing the stroke reader first (see
[G10](#g10--imm-unity-depends-on-the-stroke-reader-package-for-its-windows-dll-dependencies)).

---

## 2. ImmUnityPlugin

Source: `code/appImmUnity/src/main.cpp` (2791 lines) plus the shared engine bridge
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

### 2.2 Native API (81 entry points)

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
| `ImmExporter_CreateSpawnAreaLayer` | **not declared** | — |
| `ImmExporter_CreateDrawing` / `DestroyDrawing` / `GetDrawingIndex` | Export | `ImmExporter`, `ImmAuthoringCompiler` |
| `ImmExporter_DrawingInit` / `DrawingGetElement` | Export | `ImmExporter`, `ImmAuthoringCompiler` |
| `ImmExporter_ElementInit` / `ElementSetPoint` / `ElementSetPoints` | Export | `ImmExporter`, `ImmAuthoringCompiler` |
| `ImmExporter_ComputeElementBounds` / `ComputeDrawingBounds` | Export | `ImmExporter`, `ImmAuthoringCompiler` |
| `ImmExporter_PaintAddFrame` | Export | `ImmExporter`, `ImmAuthoringCompiler` |
| `ImmExporter_LayerAddAnimationKey` | Export | `ImmAuthoringCompiler` — **no native symbol** |
| `ImmExporter_PaintSetMaxRepeatCount` | Export | `ImmAuthoringCompiler` — **no native symbol** |
| `ImmExporter_ExportToFile` / `ExportToMemory` | Export | `ImmExporter` |
| `ImmExporter_GetMemoryData` / `GetMemorySize` / `DestroyMemory` | Export | `ImmExporter` |

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
- **`GlobalWork`** always passes a 9000 µs work budget to the engine bridge — the bridge
  accepts a budget parameter, the export does not (see [G5](#g5--per-frame-work-budget-is-hardcoded)).
- **`SetCameraViewport`** only stores width/height and only on `__APPLE__`
  (`main.cpp:1602`); the bridge's `ViewportInfo` has x, y, minDepth, maxDepth and
  forceViewport, none of which is reachable (see [G6](#g6--camera-viewport-is-widthheight-only)).

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
file into a plain in-memory stroke/layer graph. All 37 entry points exist on all four
platforms.

### 3.1 Native API (37 entry points)

All are declared in `Runtime/ImmStrokeReader.cs` except where noted.

| Group | Entry points |
|---|---|
| Build/init | `StrokeReader_GetBuildId` (**not declared**), `Init`, `IsInitialized`, `End` (—), `GetDocumentCount` |
| Load | `LoadFromFile`, `LoadFromMemory`, `Unload` |
| Document | `GetDocumentInfo` |
| Layers | `GetLayerCount`, `GetLayerInfo`, `GetLayerTransform` |
| Layers (authoring view) | `GetAuthoringLayerCount`, `GetAuthoringLayerInfo`, `GetAuthoringLayerTransform` |
| Animation | `GetLayerAnimationKeyCount`, `GetLayerAnimationKey`, `GetLayerAnimationInfo`, `GetAuthoringLayerAnimationInfo` |
| Drawings / frames | `GetDrawingCount`, `GetDrawingIndexForChapter`, `GetFrameBuffer`, `GetAuthoringDrawingCount`, `GetAuthoringFrameBuffer` |
| Strokes | `GetStrokeCount`, `GetStrokeInfo`, `GetStrokePoints`, `GetAuthoringStrokeCount`, `GetAuthoringStrokeInfo`, `GetAuthoringStrokePoints`, `GetDrawingBiggestStroke` (**not declared**) |
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

Severity: **S1** breaks a shipped code path, **S2** capability the binary has and the
wrapper cannot reach, **S3** integration/packaging/documentation defect.

| # | Severity | Gap | Area |
|---|---|---|---|
| [G1](#g1--impexporter_layeraddanimationkey-is-declared-and-called-but-does-not-exist) | S1 | `ImmExporter_LayerAddAnimationKey` called by C#, absent from every binary and from native source | exporter |
| [G2](#g2--impexporter_paintsetmaxrepeatcount-same) | S1 | `ImmExporter_PaintSetMaxRepeatCount` — same | exporter |
| [G3](#g3--exporter-api-exists-only-in-the-windows-binary) | S2 | 21 exporter entry points exist only on Windows; C# is compiled on every platform | exporter |
| [G4](#g4--impexporter_createspawnarealayer-is-not-wrapped) | S2 | `ImmExporter_CreateSpawnAreaLayer` exported, no C# binding at all | exporter |
| [G5](#g5--per-frame-work-budget-is-hardcoded) | S2 | `GlobalWork` budget hardcoded to 9000 µs | playback |
| [G6](#g6--camera-viewport-is-widthheight-only) | S2 | viewport x/y/minDepth/maxDepth/forceViewport unreachable; export is Apple-only | rendering |
| [G7](#g7--no-native-frame-pose--needs-update-consumer) | S2 | `GetSpawnAreaPose`, `GetSpawnAreaNeedsUpdate`/`SetSpawnAreaNeedsUpdate` wrapped but never called | spawn areas |
| [G8](#g8--spawn-area-screenshot-is-returned-but-never-consumed) | S2 | screenshot pointer/format/size returned by `GetSpawnAreaInfo`, dropped by the managed wrapper | spawn areas |
| [G9](#g9--player-capabilities-with-no-unity-entry-point) | S2 | chapter lengths, audio presence, cancel-loading, load time, perf counters, unload-all, pause/resume-at-tick not exposed | playback |
| [G10](#g10--imm-unity-depends-on-the-stroke-reader-package-for-its-windows-dll-dependencies) | S3 | `ImmUnityPlugin.dll` imports DLLs only the stroke-reader package ships | packaging |
| [G11](#g11--strokereader_getbuildid-is-not-wrapped) | S2 | stroke-reader build id not reachable from C# | stroke reader |
| [G12](#g12--strokereader_getdrawingbiggeststroke-is-not-wrapped) | S2 | biggest-stroke query not reachable from C# | stroke reader |
| [G13](#g13--strokereader_end-is-never-called) | S3 | managed code never shuts the stroke reader down | lifecycle |
| [G14](#g14--debug-is-unused) | S3 | `Debug()` unused | diagnostics |
| [G15](#g15--stale-tracked-stroke-reader-dll) | S3 | `code/appImmStrokeReader/exe/ImmStrokeReader.dll` is 9 entry points behind | packaging |
| [G16](#g16--no-automated-export-vs-dllimport-check) | S3 | CI does not compare P/Invokes against exports | process |
| [G17](#g17--undocumented-native-surface-and-flags) | S3 | no reference doc for the 81/37 entry points or the 25 runtime flags | docs |
| [G18](#g18--immstrokereaderupm-is-a-broken-package-skeleton) | S3 | `code/ImmStrokeReaderUPM/` looks installable but is empty | packaging |

### G1 — `ImmExporter_LayerAddAnimationKey` is declared and called but does not exist

- Declared: `Runtime/ImmExporter.cs:560`.
- Called: `Runtime/Authoring/ImmAuthoringCompiler.cs:347`, once per animation key of every
  layer during `ExportToMemory`/`ExportToFile`.
- Absent from: the 81-name export table of the shipped Windows DLL, both local build
  outputs, all other platform binaries, and every native source file at this revision
  (`git grep LayerAddAnimationKey HEAD -- *.cpp *.h` → no match).
- Introduced by `72959afe` ("Add supported paint import and round trip") in C# only;
  binaries were refreshed afterwards in `300f8fdc` without the symbol appearing.

Impact: any runtime-authoring export that reaches an animation key throws
`EntryPointNotFoundException` instead of returning a structured
`ImmAuthoringErrorCode`. Nothing in CI catches it (`--platform windows` in the verifier
reports it).

Fix options: implement `ImmExporter_LayerAddAnimationKey` in `main.cpp`. The native work is
bounded — `libImmExporter::Layer` already exposes the public setter
`AddKey(piTick, AnimProperty, const AnimValue&, InterpolationType)`
(`code/libImmExporter/src/document/layer.h:153`) and the shipped `ImmExporter_CreatePaintLayer` /
`CreateGroupLayer` already take a `maxRepeatCount` argument. Alternatively delete both C#
declarations and their calls and return `ImmAuthoringErrorCode.Unsupported`.

### G2 — `ImmExporter_PaintSetMaxRepeatCount` same

- Declared: `Runtime/ImmExporter.cs:610`; called: `ImmAuthoringCompiler.cs:387` for every
  paint layer, **before** the first drawing is created — so this fires earlier than G1 and
  affects even simple paint documents.
- Same evidence as G1: never defined natively, not exported, not detected by CI.
- Fix: `libImmExporter::Layer::SetMaxRepeatCount` is public
  (`code/libImmExporter/src/document/layer.h:146`) and `ImmExporter_CreatePaintLayer`
  already accepts `maxRepeatCount`, so the export is a thin forwarder — or the call can be
  dropped, since the repeat count is already supplied at layer creation.

### G3 — exporter API exists only in the Windows binary

The whole exporter block in `code/appImmUnity/src/main.cpp` is inside
`#if defined(WINDOWS)` (lines 2347–2791), and `libImmExporter` is referenced only by
`appImmUnity.vcxproj` and `imm.sln`. Result: 60 of 81 API entry points on Android, iOS and
macOS.

The C# side knows this (`ImmAuthoringRuntime.Capabilities` reports authoring support for
Windows x64 only, `ImmAuthoringRuntime.cs:48-69`) but `ImmExporter.cs` has no
`#if` guards, so the types are callable and fail at first P/Invoke on device. The verifier
tracks this as a documented platform gap (`KNOWN_PLATFORM_GAPS`).

Suggested: make `ImmAuthoringRuntime.Capabilities` the enforced gate (throw a managed
`NotSupportedException`/capability error before any exporter P/Invoke) so device builds
fail predictably instead of with `DllNotFoundException`/`EntryPointNotFoundException`.

### G4 — `ImmExporter_CreateSpawnAreaLayer` is not wrapped

Exported (`main.cpp:2514`), present in the shipped Windows DLL, but no C# declaration
anywhere. Consequence: `ImmAuthoringDocument` cannot create spawn-area layers, so an IMM
file authored at runtime can never carry viewpoints — the runtime authoring graph only
produces group and paint layers. Fix: add the binding plus an
`ImmAuthoringLayerType.SpawnArea` path (transform + floorLevel), or document that runtime
authoring is playback-only.

### G5 — per-frame work budget is hardcoded

`GlobalWork(int enabled)` calls `mBridge.GlobalWork(enabled == 1, 9000)` (`main.cpp:1566`).
The bridge signature is `GlobalWork(bool, int budgetMicroseconds = 9000)`
(`imm_engine_bridge.h:62`), and `Player::GlobalWork(bool, uint32_t microsecondsBudget)`
accepts the budget. No C# API or flag can change it, so hosts cannot trade frame time
against load progress on device.

### G6 — camera viewport is width/height only

`SetCameraViewport(int cameraID, int width, int height)` (`main.cpp:1602`) stores two
ints, and only under `#if defined(__APPLE__)`; on Windows/Android the call is a no-op.
The bridge's `ViewportInfo` carries `x`, `y`, `minDepth`, `maxDepth` and `forceViewport`
(`imm_engine_bridge.h:43-52`), none of which is reachable. Sub-rectangle rendering,
depth-range control and forced viewports therefore cannot be requested from Unity, and
managed code gets no error when the call does nothing.

### G7 — no native frame-pose / needs-update consumer

`GetSpawnAreaPose` and `GetSpawnAreaNeedsUpdate` are declared in
`ImmNativePlugin.cs:296`/`:287` with comments describing their intended use (cheap
per-frame pose, timeline "make default" re-anchor signal), but nothing in the repository
calls either. `ImmDocument` instead re-derives poses through the heavier
`GetSpawnAreaInfo` path (`ImmDocument.cs:445-497`), and the needs-update signal is never
consumed — so a Quill MakeDefault keyframe cannot re-anchor a rig through the shipped C#
API. `SetSpawnAreaNeedsUpdate` has no caller either (the only match is its own comment).

### G8 — spawn-area screenshot is returned but never consumed

`GetSpawnAreaInfo` fills `SerializedSpawnArea.screenshot` (format, width, height, pixel
pointer — `main.cpp:2249-2263`) and C# mirrors the struct, but `grep -r screenshot` over
both packages matches only the struct definition: `SpawnAreaInfo` (the managed projection)
deliberately omits it (`ImmDocument.cs:461-471`) and no code turns the pixels into a
`Texture2D`. Spawn-area thumbnails are therefore unavailable to Unity even though the
plugin delivers them.

### G9 — player capabilities with no Unity entry point

These exist on `ImmPlayer::Player` (`code/libImmPlayer/src/player.h`) and are reachable
inside the plugin through `ImmEngineBridge::GetPlayer()` (`imm_engine_bridge.h:75`), but no
entry point exposes them, so Unity cannot use them. The standalone viewer calls several
directly (`code/appImmViewer/src/viewer.cpp`: `GetLoadTimeInMs`, `CancelLoading`,
`UnloadAllSync`), which is the clearest evidence that they are product-relevant:

| Capability | Native API | Why it matters |
|---|---|---|
| Chapter lengths + `hasPlays` | `GetChapterInfo(numChapters, chapterLengths, hasPlays, id)` | chapter UI can only show an index, not duration or whether markers are real plays |
| Audio presence | `GetHasAudio(id)` | `GetSound` returns volume only; cannot tell "silent" from "muted" |
| Cancel an in-flight load | `CancelLoading(id)` | long loads can only be waited out or unloaded |
| Load time measurement | `GetLoadTimeInMs()` | no load telemetry |
| Performance counters | `EnablePerformanceMeasurement(bool)` + `PerformanceInfo` | draw calls, triangles and culled counts unavailable to Unity profiling |
| Unload everything | `UnloadAll()`, `UnloadAllSync()` | multi-document hosts must iterate ids |
| Pause/resume at a tick | `Pause(id, stopTicks)`, `Resume(id, startTicks)` | frame-accurate stop/start not expressible |

### G10 — imm-unity depends on the stroke-reader package for its Windows DLL dependencies

`ImmUnityPlugin.dll` imports `zlib1.dll`, `jpeg62.dll`, `libpng16.dll`, `ogg.dll`,
`vorbis.dll` (plus `Audio360.dll`, `opusenc.dll`, `vorbisenc.dll`, which it does ship).
The imm-unity package's `Plugins/x86_64` contains only the latter three plus
`Audio360.dll`; the missing five are shipped by the stroke-reader package. Installing
imm-unity without the stroke reader leaves a Windows player that cannot load the plugin.
The dependency is declared in `package.json` and in the README, but nothing verifies it —
and the two packages use different plugin folder conventions (`OSX/*.bundle` vs
`macOS/*.dylib`, `Android/libs/arm64-v8a` vs `Android/arm64-v8a`).

### G11 — `StrokeReader_GetBuildId` is not wrapped

Exported on all four platforms (`main.cpp:37`), absent from `ImmStrokeReader.cs` (36 of 37
entry points declared). Managed code therefore cannot verify which native stroke reader
build is loaded — exactly what the function exists for.

### G12 — `StrokeReader_GetDrawingBiggestStroke` is not wrapped

Exported on all four platforms (`main.cpp:438`, backed by
`StrokeStore::GetDrawingBiggestStroke`), absent from the C# wrapper. It is the query used
by the web decoder's paint-geometry builder
(`code/projects/web/decoder/src/imm_web_scene.cpp`, `buildPaintGeometry`), so Unity cannot
reproduce the same geometry-sizing decisions from the managed API.

### G13 — `StrokeReader_End` is never called

Declared (`ImmStrokeReader.cs:38`) but never invoked, so the native document map and log
are never torn down from managed code (domain reload / play-mode exit leaves them to
process teardown). `StrokeReaderDocument` is `IDisposable` for individual documents, but
the static `ImmStrokeReader` binding class offers no shutdown path that calls
`StrokeReader_End`.

### G14 — `Debug()` is unused

`ImmNativePlugin.Debug()` (`main.cpp:1515` logs renderer initialisation state) has no
caller. Useful when diagnosing "plugin loaded but renderer absent" on device.

### G15 — stale tracked stroke reader DLL

`code/appImmStrokeReader/exe/ImmStrokeReader.dll` is tracked and clean but predates the
nine `StrokeReader_GetAuthoring*` entry points (28 of the current 37 exports); the package
copy has all 37. Anyone pointing a test harness at the `exe\` path gets an incomplete API.
The file is not covered by `.gitignore` (only its `.pdb`/`.exp`/`.lib` siblings are).

### G16 — no automated export-vs-DllImport check

`ci-engine.yml:99-126` asserts three export names via `dumpbin`;
`tests/tools/verify_package_layout.py` checks existence and non-zero size. Nothing links
the C# `[DllImport]` set to the binary's export table, which is why G1/G2 survived CI.
`tests/tools/verify_unity_plugin_exports.py` now does that check offline for all four
platforms; wiring it into the "Verify Unity native plugin exports" step (plus the stroke
reader binary) would close the loop.

### G17 — undocumented native surface and flags

The 81 + 37 entry points, the ABI structs, the render-event protocol and the runtime flags
listed in [§2.5](#25-runtime-flags) had no reference outside the sources and the package
READMEs; this document is the first consolidated list. The flags in particular are
discoverable only by reading `main.cpp`, and they are the only way to reach several
diagnostic and Vulkan/Metal workarounds.

### G18 — `ImmStrokeReaderUPM/` is a broken package skeleton

Contains `package.json` (version `0.1.0`, no `com.unity.nuget.newtonsoft-json`
dependency) and `README.md` advertising `Plugins/`, `Runtime/` and `Samples~/Examples`
that do not exist. Its manifest's `samples[0].path` points at a missing folder. It is not
the shipped package, but it sits at a plausible install path.

### Related documentation drift (not plugin gaps)

- `README.md:163` states the macOS ImmUnity build "is currently disabled in CI";
  `build.yml:501` builds `--target ImmUnity` on `macos-14`.
- The committed `ImmUnityPlugin.bundle`'s `_CodeSignature/CodeResources` predates the
  committed binary (signature no longer covers it), and the macOS bundle/dylib are one
  binary-sync commit behind the Windows/Android/iOS copies.

---

## 7. Appendix — full export lists

`ImmUnityPlugin` (81, identical on Windows; 60 on Android/iOS/macOS with all
`ImmExporter_*` absent):

```
ClearLayerTransformOverride ClearLayerVisibilityOverride ConfigureVulkanRenderEvent Continue
Debug End GetActiveSpawnAreaId GetBoundingBox GetChapterCount GetCurrentChapter
GetDocumentInfoEx GetDocumentState GetInitialSpawnAreaId GetLayerCount GetLayerDiagnostics
GetLayerInfoByIndex GetPlayTime GetPlayerInfo GetRenderEventAndDataFunc GetRenderEventFunc
GetSound GetSpawnAreaCount GetSpawnAreaInfo GetSpawnAreaList GetSpawnAreaNeedsUpdate
GetSpawnAreaPose GetTime GlobalWork Hide
ImmExporter_ComputeDrawingBounds ImmExporter_ComputeElementBounds ImmExporter_CreateDrawing
ImmExporter_CreateGroupLayer ImmExporter_CreatePaintLayer ImmExporter_CreateSequence
ImmExporter_CreateSpawnAreaLayer ImmExporter_DestroyDrawing ImmExporter_DestroyMemory
ImmExporter_DestroySequence ImmExporter_DrawingGetElement ImmExporter_DrawingInit
ImmExporter_ElementInit ImmExporter_ElementSetPoint ImmExporter_ElementSetPoints
ImmExporter_ExportToFile ImmExporter_ExportToMemory ImmExporter_GetDrawingIndex
ImmExporter_GetMemoryData ImmExporter_GetMemorySize ImmExporter_PaintAddFrame
Init IsDocumentActive IsReadyForDocumentLoad IsSequenceReady LoadFromFile LoadFromMemory
Pause PrepareCamera Restart Resume SetActiveSpawnAreaId SetCameraViewport SetChapter
SetDocumentToWorld SetLayerOpacity SetLayerTransform SetLayerVisible SetMatrices
SetRuntimeFlag SetSound SetSpawnAreaNeedsUpdate SetTime SetVulkanCameraEyeRenderBuffers
SetVulkanCameraRenderBuffers SetVulkanDedicatedQueueAllowed Show SkipBack SkipForward
UnityPluginLoad UnityPluginUnload Unload
```

`ImmStrokeReader` (37, identical on all four platforms):

```
StrokeReader_End StrokeReader_GetAuthoringDrawingCount StrokeReader_GetAuthoringFrameBuffer
StrokeReader_GetAuthoringLayerAnimationInfo StrokeReader_GetAuthoringLayerCount
StrokeReader_GetAuthoringLayerInfo StrokeReader_GetAuthoringLayerTransform
StrokeReader_GetAuthoringStrokeCount StrokeReader_GetAuthoringStrokeInfo
StrokeReader_GetAuthoringStrokePoints StrokeReader_GetBuildId StrokeReader_GetChapterCount
StrokeReader_GetChapterCountFromFile StrokeReader_GetCurrentChapter
StrokeReader_GetDocumentCount StrokeReader_GetDocumentInfo
StrokeReader_GetDrawingBiggestStroke StrokeReader_GetDrawingCount
StrokeReader_GetDrawingIndexForChapter StrokeReader_GetFrameBuffer
StrokeReader_GetLayerAnimationInfo StrokeReader_GetLayerAnimationKey
StrokeReader_GetLayerAnimationKeyCount StrokeReader_GetLayerCount StrokeReader_GetLayerInfo
StrokeReader_GetLayerTransform StrokeReader_GetPictureInfo StrokeReader_GetPicturePixelData
StrokeReader_GetStrokeCount StrokeReader_GetStrokeInfo StrokeReader_GetStrokePoints
StrokeReader_Init StrokeReader_IsInitialized StrokeReader_LoadFromFile
StrokeReader_LoadFromMemory StrokeReader_SetChapter StrokeReader_Unload
```
