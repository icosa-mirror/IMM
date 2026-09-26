# Procedural IMM/Quill Generation — Implementation Plan

Goal: create Unity UPM packages that allow IMM/Quill scenes to be generated programmatically in C#, producing `.imm` files that can be played back in the IMM viewer or opened in Quill.

---

## Architecture overview

```
libImmExporter (C++)          appImmStrokeWriter (C++ DLL)
     ↓                                  ↓
Sequence / Layer / Drawing   ImmStrokeWriter.dll  ←── P/Invoke
/ Element / Point API                  ↓
                             com.immersive-foundation.imm-stroke-writer (UPM)
                             ImmStrokeWriterNative (raw P/Invoke)
                             ImmStrokeWriterDocument / ImmDrawing / ImmStroke (high-level)
```

The **read** side already exists:
```
libImmImporter (C++)  →  appImmStrokeReader  →  ImmStrokeReader.dll
                                                       ↓
                                         com.immersive-foundation.imm-stroke-reader (UPM)
```

Eventually (Phase 1.5) a shared-core package will tie the two together for round-trip workflows.

---

## Phase 1 — Windows, still scenes, all brush types ✅ COMPLETE

### What was built

**Native DLL: `code/appImmStrokeWriter/`**
- `appImmStrokeWriter.vcxproj` — Visual Studio project, links `libImmExporter` + `libImmCore`
- `src/main.cpp` — C API wrapping `libImmExporter`:
  - `ImmExporter_CreateSequence` / `DestroySequence` / `GetRootLayer`
  - `ImmExporter_CreatePaintLayer` / `CreateGroupLayer`
  - `ImmExporter_CreateDrawing` / `DestroyDrawing` / `DrawingInit` / `DrawingGetElement`
  - `ImmExporter_ElementInit` / `ElementSetPoint`
  - `ImmExporter_ComputeElementBounds` / `ComputeDrawingBounds`
  - `ImmExporter_PaintAddFrame`
  - `ImmExporter_ExportToFile`
  - `StrokeWriter_GetBuildId`
- Added to `code/projects/windows/imm.sln`
- Post-build event auto-copies DLL to the UPM package

**UPM package: `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-writer/`**

| File | Purpose |
|------|---------|
| `Runtime/ImmStrokeWriter.cs` | Raw P/Invoke declarations + marshalled structs (`ImmExporterTransform`, `ImmExporterPoint`) |
| `Runtime/ImmStrokeWriterDocument.cs` | High-level C# API: `ImmStrokeWriterDocument`, `ImmDrawing`, `ImmStroke` |
| `Editor/ImmStrokeWriterTestEditor.cs` | Editor test window at **IMM > Stroke Writer Test** |
| `Plugins/x86_64/ImmStrokeWriter.dll` | Built native DLL |

**High-level usage example:**
```csharp
using var doc = new ImmStrokeWriterDocument();
doc.Init(SequenceType.Still, Color.black, frameRate: 60);

var paint = doc.CreatePaintLayer(doc.RootLayer, "MyStrokes");
using var drawing = doc.CreateDrawing(paint);
drawing.Init(strokeCount: 1);

var stroke = drawing.GetStroke(0);
stroke.Init(pointCount: 3, BrushSectionType.Segment);
stroke.SetPoint(0, pos0, Vector3.up, -Vector3.forward, Color.red, width: 0.01f);
stroke.SetPoint(1, pos1, Vector3.up, -Vector3.forward, Color.red, width: 0.01f);
stroke.SetPoint(2, pos2, Vector3.up, -Vector3.forward, Color.red, width: 0.01f);

drawing.ComputeBounds();
doc.AddFrame(paint, drawing.Index);
doc.ExportToFile("/path/to/output.imm");
```

**Build command** (from repo root):
```bash
MSBuild code/projects/windows/imm.sln -t:appImmStrokeWriter -p:Configuration=Release -p:Platform=x64 -m
```

### Known limitations / deferred to later phases
- Windows only
- Flat layer hierarchy (one level of paint layers under root)
- No animation keyframes (only static single-frame scenes; `AddFrame` maps frame 0 → drawing 0)
- No picture, sound, or model layers
- No round-trip integration with `imm-stroke-reader`

---

## Phase 1.5 — Round-trip shared types

**Goal:** read an IMM file with `imm-stroke-reader`, modify strokes in C#, and write back out with `imm-stroke-writer` without manual type conversion.

### What needs to change

**New package: `com.immersive-foundation.imm-core`**
- Pure C#, no native dependencies
- Defines shared types that both reader and writer reference:
  - `ImmPoint` (merges `StrokePoint` from reader + `ImmExporterPoint` from writer)
  - `ImmTransform` (merges `StrokeLayerTransform` + `ImmExporterTransform`)
  - `ImmBrushType` (unifies brush type enums)
  - `ImmLayerInfo`

**Update `com.immersive-foundation.imm-stroke-reader`**
- Add `imm-core` as a dependency
- Replace inline struct definitions with references to `imm-core` types
- Add conversion helpers: `StrokePoint → ImmPoint`, `StrokeLayerTransform → ImmTransform`
- This is a **breaking change** for any code referencing `StrokePoint` directly

**Update `com.immersive-foundation.imm-stroke-writer`**
- Add `imm-core` as a dependency
- Replace `ImmExporterPoint` / `ImmExporterTransform` with `imm-core` types
- Note: `ImmExporterPoint` has `length` and `time` fields that `StrokePoint` doesn't — keep as extension fields

**Note on the reader's `length`/`time` gap:** The native reader exposes only 14 floats per point (no `length`/`time`). The writer requires 16. The imm-core `ImmPoint` should include all 16 fields; the reader fills `length=0, time=0` when constructing from native data.

---

## Phase 2 — Animation

**Goal:** generate animated `.imm` files where strokes appear, disappear, or move over time.

### Two animation mechanisms in IMM

**1. DrawInTime** — strokes draw themselves progressively. Each `Element` already has a `time` field per point (the parametric position along the draw timeline). This is already passable via `ImmExporterPoint.time` in Phase 1; it just needs documenting and testing.

**2. Frame sequences** — a `LayerPaint` maps discrete time ticks to drawings. Different drawings appear at different frames. This is already scaffolded via `ImmExporter_PaintAddFrame` but needs:
- Exposing the tick/framerate relationship clearly in C# (`piTick` = microseconds in the IMM format)
- Support for `durationTicks` and `maxRepeatCount` in `CreatePaintLayer`
- Helper API: `doc.CreateAnimatedPaintLayer(name, durationSeconds, frameRate)`

**3. Property animation keyframes** (advanced) — `Layer::AnimKey` with interpolation curves. Currently not in the C API at all. Requires:
- New C++ functions: `ImmExporter_AddAnimKey(layerHandle, property, tick, value, interpolation)`
- C# bindings for `AnimProperty` enum and `AnimValue` union
- This is the most complex part of Phase 2

### Steps for Phase 2

1. Add `durationTicks` / `maxRepeatCount` parameters to the C# `CreatePaintLayer` API (already in P/Invoke, just not surfaced in the high-level wrapper)
2. Add a `seconds → piTick` conversion helper (1 tick = 1 microsecond; 1 second = 1,000,000 ticks)
3. Document and test DrawInTime via the `time` field on points
4. Add C++ `ImmExporter_AddAnimKey` and C# bindings
5. Update the editor test window with an animation test case

---

## Phase 3 — Full hierarchy and picture layers

### Layer hierarchy

Currently `CreatePaintLayer` and `CreateGroupLayer` accept any layer handle as parent, so deep nesting is already possible via the C API. What's missing:

- C# convenience: `ImmGroupLayer` wrapper class with `Children` collection
- A scene-graph builder pattern: `doc.CreateGroupLayer(parent)` returning a typed object that can itself be used as a parent
- Transform propagation helpers (world → local conversion)

### Picture layers

`libImmExporter` has `LayerPicture` with full PNG/JPG support, but no C API wrapper exists. Requires:

1. New C++ functions in `appImmStrokeWriter/src/main.cpp`:
   - `ImmExporter_CreatePictureLayer(sequenceHandle, parentHandle, name, ...)`
   - `ImmExporter_SetPictureAsset(layerHandle, pixelData, width, height, format)`
2. C# bindings: `doc.CreatePictureLayer(parent, texture2D)`
3. Unity `Texture2D → raw pixel bytes` conversion helper

---

## Phase 4 — Cross-platform

**Goal:** `ImmStrokeWriter.dll` available on Android (arm64), macOS, and iOS.

### Current state

`libImmExporter` is compiled as a Windows-only static library (`.vcxproj`). The Android build system uses CMake (see `appImmStrokeReader/Projects/Android/CMakeLists.txt` as a reference).

### Steps

1. Create `code/appImmStrokeWriter/Projects/Android/CMakeLists.txt` — modelled on the reader's CMakeLists but linking `libImmExporter` instead of `libImmImporter`
2. Verify that all `libImmExporter` source compiles cleanly with clang/NDK (the exporter uses no Windows-specific APIs, but `piStr.h` wide-string functions may need attention)
3. Build `.so` and drop into `Plugins/Android/arm64-v8a/`
4. macOS/iOS: create CMake or Xcode project; output `.dylib` / `.a`
5. Remove `#if defined(WINDOWS)` guards from any platform-specific writer code

**Risk:** `libImmExporter`'s audio codec dependencies (Opus, Ogg/Vorbis) need Android/macOS builds. These are well-supported by vcpkg but need a separate arm64 build pipeline. The geometry-only path (no audio layers) has no codec dependency and could ship as a simpler Phase 4a.

---

## Drawing Test Scene ✅ COMPLETE

**Goal:** Interactive Unity scene where the user draws with the mouse and exports to .imm.

### Key discovery
`com.immersive-foundation.imm-unity` already contains `ImmPlayer.Exporter` — a higher-level wrapper (`ExportSequence`, `ExportPaintLayer`, `ExportDrawing`, `ExportElement`, `PaintPoint`) that wraps `ImmUnityPlugin.dll`'s exporter functions. The existing example scripts (`ImmExportExample.cs`, `ImmExportGroupsExample.cs`) use this API. The test scene should use **our new `ImmStrokeWriterDocument`** to exercise the new package, with a comment noting the alternative.

### Files to create
| File | Purpose |
|------|---------|
| `Assets/Scripts/ImmDrawingTest.cs` | Main MonoBehaviour: mouse drawing, IMGUI controls, export |
| `Assets/Editor/ImmDrawingTestSceneBuilder.cs` | `IMM > Create Drawing Test Scene` menu item |

### Design decisions
- **IMGUI (OnGUI)** for the panel — avoids Canvas/EventSystem wiring in the scene builder
- **Drawing plane:** `Camera.ScreenToWorldPoint` at fixed depth (Z = 5 m in front of camera)
- **Stroke storage:** `List<StrokeData>` (Points, Color, BrushType, Width) — collected in memory, exported all at once
- **LineRenderer** per stroke for Unity-side preview
- **Coordinate conversion on export:** negate Z (Unity left-handed → IMM right-handed)
- **Normal per point:** `cross(tangent_imm, viewDir_imm)` where `viewDir_imm = (0,0,-1)` — makes ribbon lie flat in drawing plane
- **Minimum 2 points** per stroke; distance threshold (~0.02 m) to skip redundant points

### UI panel (left column, IMGUI)
- 8 preset colour buttons (white, red, green, blue, yellow, cyan, magenta, orange)
- Brush type selection grid: Point / Segment / Circle / Ellipse / Square
- Width slider: 0.001 – 0.05
- **Clear All** button
- **Export to IMM…** button → `EditorUtility.SaveFilePanel` in editor; `persistentDataPath` in build
- Status message (fades after 3 s)
- — Procedural section —
- Shape dropdown: Spiral / Star / Lissajous / Circle Pattern
- **Generate** button

### Procedural shapes
| Shape | Description | Strokes |
|-------|-------------|---------|
| Spiral | Archimedean spiral, 2 revolutions | 1 |
| Star | Parametric `r = 0.5 + 0.5·cos(5θ)` | 1 |
| Lissajous | a=3, b=2, δ=π/4 | 1 |
| Circle Pattern | 6 circles in a ring | 6 (one per circle) |

### Scene builder
- `EditorSceneManager.NewScene(EmptyScene)`
- Camera at origin, looking +Z, FOV 60, dark grey background
- Empty GameObject `DrawingTest` with `ImmDrawingTest` component
- Save to `Assets/Scenes/ImmDrawingTest.unity`
- Log: "Press Play to start drawing"

---

## File inventory

### New files (all phases)

| File | Phase | Status |
|------|-------|--------|
| `code/appImmStrokeWriter/appImmStrokeWriter.vcxproj` | 1 | ✅ |
| `code/appImmStrokeWriter/src/main.cpp` | 1 | ✅ |
| `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-writer/package.json` | 1 | ✅ |
| `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-writer/Runtime/ImmStrokeWriter.cs` | 1 | ✅ |
| `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-writer/Runtime/ImmStrokeWriterDocument.cs` | 1 | ✅ |
| `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-writer/Editor/ImmStrokeWriterTestEditor.cs` | 1 | ✅ |
| `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-writer/Plugins/x86_64/ImmStrokeWriter.dll` | 1 | ✅ |
| `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-core/` (entire package) | 1.5 | ⬜ |
| `code/appImmStrokeWriter/Projects/Android/CMakeLists.txt` | 4 | ⬜ |

### Modified files

| File | Phase | Status |
|------|-------|--------|
| `code/projects/windows/imm.sln` | 1 | ✅ added `appImmStrokeWriter` |
| `code/projects/android/README.md` | 1 | ✅ added Unity plugin build commands |
| `BUILDING.md` | 1 | ✅ updated with writer DLL entry |
| `.github/workflows/build.yml` | 1 | ✅ writer artifact upload, packaging, release, upm publish |
| `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/` | 1.5 | ⬜ migrate to imm-core types |
| `code/appImmStrokeWriter/src/main.cpp` | 2 | ⬜ add `ImmExporter_AddAnimKey` |
| `code/appImmStrokeWriter/src/main.cpp` | 3 | ⬜ add picture layer C API |
