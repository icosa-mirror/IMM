# IMM to Quill Converter Handover

Date: 2026-02-11  
Project root: `C:\Users\andyb\Documents\IMM`

## Goal

Convert `.imm` files to Quill project format (`Quill.json` + `Quill.qbin`) using existing IMM import code and SharpQuill code, with a practical non-comprehensive first pass.

## High-Level Architecture

- **IMM decoding** is done in native C++ (`libImmImporter` + `appImmStrokeReader`) and exposed to Unity through C API.
- **Quill writing** is done in C# using vendored SharpQuill.
- **Bridge/converter** is `SharpQuillCompat` in the stroke-reader package.

Data path:

1. IMM file -> `StrokeReader_LoadFromFile` / `StrokeReader_LoadFromMemory`
2. Native importer decodes layers/drawings/strokes
3. C API exposes decoded data to C#
4. `SharpQuillCompat` builds `SharpQuill.Sequence`
5. `QuillSequenceWriter.Write(sequence, outputFolder)` writes Quill project

## What Is Already Done

### 1) SharpQuill vendored into this project

- Vendored location:
  - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ThirdParty/SharpQuill`
- Assembly/package dependencies updated for Newtonsoft.

### 2) Converter implementation exists

- Main converter:
  - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/SharpQuillCompat.cs`
- Exposed APIs:
  - `ReadImmAsSequence(string path, bool includePictures = false)`
  - `WriteImmAsQuillProject(string immPath, string outputFolder, bool includePictures = false)`

### 3) Unicode filename handling fix

- If `StrokeReader_LoadFromFile` fails (common with unicode/emoji paths), converter falls back to:
  - `File.ReadAllBytes(path)` + `StrokeReader_LoadFromMemory(...)`
- This is implemented in `TryLoadDocument(...)` in `SharpQuillCompat.cs`.

### 4) Paint stroke conversion (core geometry)

Implemented mappings in `ConvertPaintLayer(...)`:

- IMM stroke points -> Quill vertices
  - position: `pt.px/py/pz`
  - normal: `pt.nx/ny/nz`
  - tangent: `pt.dx/dy/dz`
  - color: `pt.r/g/b`
  - opacity: `pt.alpha`
  - width: `pt.width`
- Brush mapping:
  - 1 -> Ribbon
  - 2 -> Cylinder
  - 3 -> Ellipse
  - 4 -> Cube
  - default -> Cylinder

### 5) Static layer properties conversion

Implemented in `ApplyCommonLayerProperties(...)` and transform helpers:

- Visible, Opacity
- Layer local transform (rotation/scale/flip/translation)
- Pivot transform

### 6) Picture layer conversion

Implemented in `ConvertPictureLayer(...)`:

- Reads pixel blob from native API
- Maps content types to Quill picture types
- Creates `LayerPicture` with dimensions, alpha flag, and raw pixel bytes

### 7) Frame-by-frame paint animation support

This was a key fix.

Problem previously:

- Converter used one-frame-per-drawing fallback (`Frames = [0..N-1]`), which broke timing/holds.

What is now implemented:

- Native pipeline now passes frame mapping data from IMM:
  - new collector callbacks in `strokeCollector.h`:
    - `OnPaintLayerInfo(frameRate, numFrames, maxRepeatCount)`
    - `OnFrameBuffer(frameBuffer, numFrames)`
- Native store (`strokeStore`) stores:
  - `frameRate`, `numFrames`, `maxRepeatCount`, `frameBuffer`
- New native C API in `main.cpp`:
  - `StrokeReader_GetLayerAnimationInfo(...)`
  - `StrokeReader_GetFrameBuffer(...)`
- C# bindings added in `ImmStrokeReader.cs`.
- Converter now builds `LayerPaint.Frames` from original IMM frame buffer mapping.

Result:

- Hold frames and drawing reuse over timeline are preserved for paint-layer frame animation.

### 8) Unity editor commands added

File:

- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Editor/ImmToQuillConverterEditor.cs`

Commands:

- `IMM/Convert IMM To Quill...`
- `IMM/Convert Selected IMM To Quill`
- `IMM/Convert All Selected IMM To Quill`

Behavior:

- Output folder auto-selected as:
  - `~/Documents/Quill/<imm-file-name>/`
- Optional prompt to include picture layers.

## What Is Not Done Yet (For Full Conversion)

## Priority 1: Transform keyframe animation (position/rotation/scale)

This is the major missing feature and likely source of animation mismatch beyond frame animation.

IMM supports keyframed animation at layer level:

- `AnimProperty::Position`
- `AnimProperty::Rotation`
- `AnimProperty::Scale`
- `AnimProperty::Transform`

Defined in:

- `code/libImmImporter/src/document/layer.h`

Quill expects these in:

- `Layer.Animation.Keys.Transform` (`List<Keyframe<Transform>>`)

Current state:

- Converter only sets static `layer.Transform` and `layer.Pivot`.
- No transform keyframes are exported.

Required implementation steps:

1. Extend collector/native bridge to capture layer animation keyframes from imported IMM layer objects.
2. Add storage in `StrokeStore` for per-layer keyframe channels + interpolation + times.
3. Add native C API query methods for keyframe counts/data.
4. Add C# P/Invoke wrappers and structs.
5. In `SharpQuillCompat`, populate:
   - `layer.Animation.Keys.Transform`
   - and map interpolation enum values.
6. Ensure time conversion is correct (IMM ticks to Quill time units; both use 12600 ticks/sec basis, but verify serialization expectations in SharpQuill).

## Priority 2: Visibility/Opacity keyframes

Missing channels:

- Visibility -> `Animation.Keys.Visibility`
- Opacity -> `Animation.Keys.Opacity`

IMM has these key properties; converter currently exports only static visible/opacity values.

## Priority 3: Group/timeline hierarchy fidelity

Current converter behavior:

- Iterates layers from stroke reader and adds them directly under Quill root.

Missing:

- Preservation of original parent/child group hierarchy
- Timeline group semantics (`Animation.Timeline`, duration/start offset/max repeat at group level)

This affects animation evaluation in Quill even if individual paint layers are converted.

## Priority 4: Other IMM animation semantics

Potentially missing channels/features:

- DrawInTime
- Action keys (Play/Stop/Loop/MakeDefault)
- Offset keys
- Loop flags and timeline nesting behavior

These may be needed for parity on complex animated files.

## Known/Observed Issues

### A) open-brush-fast compatibility break from namespace refactor

What happened:

- open-brush-fast had code using old compat namespace alias (`ImmStrokeReader.SharpQuill`) + adapter.
- After switching to real SharpQuill types in package, that path broke.

What was updated:

- `C:\Users\andyb\Documents\open-brush-fast\Assets\Scripts\Quill.cs`
  - removed old alias usage
  - now uses `ImmStrokeReader.SharpQuillCompat.ReadImmAsSequence(path)` directly
  - removed old adapter block that converted between duplicate type systems

### B) Scale mismatch perception

Code findings:

- IMM exporter writes CoordSys units as meters.
- Importer reads units but does not apply a conversion factor.
- Converter currently does no explicit unit scaling, passes values through.

Potential remaining cause:

- Transform interpretation (layer transform vs stroke positions) may still need validation on specific files/toolchains.

## Files Most Relevant to Continue Work

Converter and Unity package:

- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/SharpQuillCompat.cs`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ImmStrokeReader.cs`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Editor/ImmToQuillConverterEditor.cs`

Native bridge:

- `code/appImmStrokeReader/src/main.cpp`
- `code/appImmStrokeReader/src/strokeStore.h`
- `code/appImmStrokeReader/src/strokeStore.cpp`

IMM importer internals:

- `code/libImmImporter/src/fromImmersive/strokeCollector.h`
- `code/libImmImporter/src/fromImmersive/fromImmersive.cpp`
- `code/libImmImporter/src/document/layer.h`
- `code/libImmImporter/src/document/layer.cpp`

SharpQuill animation model:

- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ThirdParty/SharpQuill/Animation.cs`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ThirdParty/SharpQuill/Keyframes.cs`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ThirdParty/SharpQuill/QuillSequenceWriter.cs`

## Suggested Next Implementation Plan (Concrete)

1. Add native extraction of `AnimProperty::Transform` keys per layer.
2. Add native extraction of `Visibility` + `Opacity` keys.
3. Expose keyframes over C API (counts + indexed get).
4. Add C# structs/enums for keyframes + interpolation.
5. Fill `layer.Animation.Keys.*` in `SharpQuillCompat`.
6. Preserve hierarchy (parent/children, group/timeline metadata).
7. Validate on 3 reference IMM files:
   - static painting
   - frame-animated paint only
   - timeline/group transform animation

## Current Logging Prefix

Converter/editor logs use:

- `[IMM2QUILL_20260209A]`

Use this to filter logs quickly.

## Final Status Snapshot

- Basic conversion: usable
- Paint frame animation: implemented
- Unicode file paths: handled via memory fallback
- Full animation parity with IMM: not yet complete
- Biggest missing piece: transform/visibility/opacity keyframes + hierarchy timeline semantics
