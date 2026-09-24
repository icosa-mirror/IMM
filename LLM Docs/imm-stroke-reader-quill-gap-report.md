# IMM Stroke Reader vs Open Brush Quill Loader

This report inventories what the current IMM stroke reader plugin exposes, what Open Brush `Quill.cs` consumes, and what data is still missing to fully support non-paint layers and timeline features.

Paths referenced:
- Plugin C API: `code/appImmStrokeReader/src/main.cpp`
- Plugin store: `code/appImmStrokeReader/src/strokeStore.h`, `code/appImmStrokeReader/src/strokeStore.cpp`
- C# P/Invoke: `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ImmStrokeReader.cs`
- SharpQuill adapter: `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/SharpQuillCompat.cs`
- Open Brush loader: `open-brush-main/Assets/Scripts/Quill.cs`
- Importer types: `code/libImmImporter/src/document/*.h`

## What the plugin exposes today

Layer-level (per layer index):
- `StrokeReader_GetLayerInfo`: `id`, `type`, `numDrawings`, `name`
- (ABI break Feb 2026) also includes: `visible`, `opacity`, `pivot` transform
- `StrokeReader_GetLayerTransform`: local + world transform (rotation, scale, flip, translation)

Drawing-level (per layer + drawing index):
- `StrokeReader_GetDrawingCount`
- `StrokeReader_GetDrawingBiggestStroke`

Stroke-level (per drawing + stroke index):
- `StrokeReader_GetStrokeInfo`: `brushType`, `visibilityMode`, `numPoints`, bounding box
- `StrokeReader_GetStrokePoints`: position, normal, direction, color, alpha, width (decoded)

Notes:
- The stroke reader is fed by `IStrokeCollector` callbacks during IMM import.
- The collector only receives stroke data for paint layers; other layer types are not surfaced.

## What Open Brush `Quill.cs` currently uses

From SharpQuill adapter:
- Treats every layer as `LayerPaint`
- Pulls transform via `StrokeReader_GetLayerTransform`
- Pulls drawings/strokes + points and builds `Stroke` for rendering

From `Quill.cs`:
- Paint strokes rendered into a single Open Brush layer per top-level Quill layer
- Picture layers are supported only if present in the SharpQuill layer model (currently not exposed by plugin)
- Group layers are used for traversal and transform stacking (not exposed by plugin)
- Timeline/animation support is only used when loading Quill folders, not IMM (IMM returns paint-only)

## Missing data vs IMM importer model

The IMM importer (`libImmImporter`) defines these layer types and metadata that are currently not exposed by the stroke reader:

### Summary table (scope for parity)

| Item | Quill folders (SharpQuill) | IMM via plugin | Parity scope (Quill.cs) | Notes |
| --- | --- | --- | --- | --- |
| Picture layers | Supported | Supported | **Yes** | IMM plugin now exposes pixel data (Feb 2026) |
| Model layers | Unsupported | Unsupported | **No** | C++ viewer renders these, but Quill.cs does not; ignoring for now |
| Sound layers | Unsupported | Unsupported | **No** | Quill.cs does not handle sound layers |
| Spawn area layers | Unsupported | Unsupported | **No** | Quill.cs does not handle spawn areas |
| Instance layers | Unsupported | Unsupported | **No** | Not supported in C++ viewer; out of scope |
| Reference layers | Unsupported | Unsupported | **No** | Quill.cs does not handle references |
| Layer hierarchy (groups) | Supported (used for transform stacking; output flattened) | Flat (no groups) | **Yes** (world transforms must match) | Quill.cs does not preserve group structure |
| Layer visibility/opacity | Supported | Supported | **Yes** | ABI break Feb 2026 |
| Pivot transform | Supported | Supported | **Yes** | ABI break Feb 2026 |
| Paint animation frames | Supported | Unsupported | **Yes** | Quill.cs uses frames when loadAnimations |
| Timeline keys | Partially supported | Unsupported | **No** | Quill.cs does not use keys |

### 1. Layer types beyond Paint (status vs SharpQuill)

**Picture layers** (`LayerPicture`) (in scope for parity)
- `ContentType` (2D, 360 mono, 360 stereo, cubemap)
  - SharpQuill (Quill folders): **Supported**
  - SharpQuill (IMM via plugin): **Supported** (ABI break Feb 2026)
- Image asset bytes or import path (via `LoadAssetMemory` / asset IDs)
  - SharpQuill (Quill folders): **Supported** (path or qbin payload)
  - SharpQuill (IMM via plugin): **Supported** (raw RGBA pixels)
- `IsViewerLocked`
  - SharpQuill (Quill folders): **Supported** (layer data has flag)
  - SharpQuill (IMM via plugin): **Supported** (ABI break Feb 2026)
- BBox and image size
  - SharpQuill (Quill folders): **Supported**
  - SharpQuill (IMM via plugin): **Unsupported**

**Model layers** (`LayerModel`) (out of scope for parity)
- Mesh geometry (`piMesh`), bbox
  - SharpQuill (Quill folders): **Unsupported** (SharpQuill doesn’t surface model meshes)
  - SharpQuill (IMM via plugin): **Unsupported**
- Shading model (unlit/smooth)
  - SharpQuill (Quill folders): **Unsupported**
  - SharpQuill (IMM via plugin): **Unsupported**
- Render wireframe flag
  - SharpQuill (Quill folders): **Unsupported**
  - SharpQuill (IMM via plugin): **Unsupported**

**Sound layers** (`LayerSound`) (out of scope for parity)
- Audio data (wav/ogg/opus) + compression flag
  - SharpQuill (Quill folders): **Unsupported** (SharpQuill doesn’t surface sound payloads)
  - SharpQuill (IMM via plugin): **Unsupported**
- Looping, play/paused/offset, gain, volume
  - SharpQuill (Quill folders): **Unsupported**
  - SharpQuill (IMM via plugin): **Unsupported**
- Type (flat/ambisonic/positional)
  - SharpQuill (Quill folders): **Unsupported**
  - SharpQuill (IMM via plugin): **Unsupported**
- Attenuation + directional modifier parameters
  - SharpQuill (Quill folders): **Unsupported**
  - SharpQuill (IMM via plugin): **Unsupported**

**Spawn area layers** (`LayerSpawnArea`) (out of scope for parity)
- Tracking level
  - SharpQuill (Quill folders): **Unsupported**
  - SharpQuill (IMM via plugin): **Unsupported**
- Volume type (sphere/box) + dimensions
  - SharpQuill (Quill folders): **Unsupported**
  - SharpQuill (IMM via plugin): **Unsupported**
- Allowed translation axes
  - SharpQuill (Quill folders): **Unsupported**
  - SharpQuill (IMM via plugin): **Unsupported**
- Optional screenshot/asset data
  - SharpQuill (Quill folders): **Unsupported**
  - SharpQuill (IMM via plugin): **Unsupported**

**Instance layers** (`LayerInstance`) (out of scope for parity)
- Referenced layer name/id + instance target pointer
  - SharpQuill (Quill folders): **Unsupported**
  - SharpQuill (IMM via plugin): **Unsupported**

**Reference layers** (`Layer::Type::Reference`) (out of scope for parity)
- External file reference
  - SharpQuill (Quill folders): **Unsupported**
  - SharpQuill (IMM via plugin): **Unsupported**

### 2. Layer metadata not exposed (status vs SharpQuill)

The base layer (`Layer`) stores:
Layer data in IMM:
- Visibility / opacity (in scope for parity)
  - SharpQuill (Quill folders): **Supported** (read from Quill JSON)
  - SharpQuill (IMM via plugin): **Supported** (ABI break Feb 2026)
- Pivot transform (in scope for parity)
  - SharpQuill (Quill folders): **Supported**
  - SharpQuill (IMM via plugin): **Supported** (ABI break Feb 2026)
- Parent/child hierarchy (in scope for parity for transform stacking only)
  - SharpQuill (Quill folders): **Supported** (used to compute world transforms)
  - SharpQuill (IMM via plugin): **Flat** (no groups; relies on world transforms)
- Timeline state: `IsTimeline`, start/stop, duration, draw-in-time (out of scope for parity)
  - SharpQuill (Quill folders): **Partially supported** (SharpQuill has these, OB uses limited)
  - SharpQuill (IMM via plugin): **Unsupported**
- Animation keys for visibility/opacity/transform/action/loop/offset (out of scope for parity)
  - SharpQuill (Quill folders): **Partially supported** (SharpQuill parses; OB mostly ignores)
  - SharpQuill (IMM via plugin): **Unsupported**
- `GetTransformOverrideEnabled` / `GetTransformOverrideValue` (out of scope for parity)
  - SharpQuill (Quill folders): **Unsupported** (not surfaced in SharpQuill)
  - SharpQuill (IMM via plugin): **Unsupported**

These are not surfaced in the plugin API or adapter (IMM path):
- Layer hierarchy (parent/children) beyond transform stacking
- Timeline duration, frame rate, loop, frame list, or action keys
- Animation keyframes and interpolation

### 3. Drawings/frames (status vs SharpQuill)

Paint layers have animation metadata:
Paint animation metadata in IMM (in scope for parity):
- `GetNumFrames`, `GetFrameBuffer`, `GetFrameRate`, `GetMaxRepeatCount`, `GetPlaying`
  - SharpQuill (Quill folders): **Supported**
  - SharpQuill (IMM via plugin): **Unsupported**

Currently the plugin only exposes drawings + biggestStroke. Frames are synthesized as one frame per drawing in the SharpQuill adapter (IMM path only).

## What to expose next (plugin C API)

Priority order for Open Brush feature parity (aligned to what `Quill.cs` already supports):

1) **Layer type + hierarchy**
- Layer type already exists but not used to create non-paint layers.
- For now keep flattening but ensure world transforms and world visibility/opacity match Quill traversal.

2) **Picture layers**
- Done (Feb 2026): IMM plugin exposes picture type + pixel data; adapter builds `LayerPicture`.

3) **Paint animation metadata**
- Frame count, frame list, frame rate, max repeat, play/loop flags.

4) **Out of scope for parity (no action yet)**
- Sound, model, spawn, instance, reference layers

## Implications for Open Brush `Quill.cs`

Once the plugin provides the above, Open Brush should:
- Keep flattening to one OB layer per top-level Quill layer, but ensure IMM world transforms match Quill traversal.
- Use layer metadata: visibility/opacity and pivot transforms.
- Map picture layers to `ReferenceImage` widgets (and 360 pano support).
- Respect paint animation frames and max repeat count when `loadAnimations == true`.
- Sound/model/spawn/instance/reference layers remain ignored to match current `Quill.cs` behavior.

## Gaps between IMM and current SharpQuill adapter

The SharpQuill adapter (`SharpQuillCompat.cs`) currently:
- Forces every IMM layer into `LayerPaint`
- Uses visibility/opacity/pivot (ABI break Feb 2026)
- Still ignores non-paint layer types and hierarchy (flattened)
- Synthesizes frames (one per drawing) rather than reading actual frame metadata

## Notes / caveats

- Pivot transform is now exposed via `Layer::GetPivot()` (ABI break Feb 2026).
- Some IMM layer types (Reference, Effect) have no Open Brush analog; consider stub/ignore.

## Open Brush flattening behavior

`Quill.cs` always flattens output to one OB layer per top-level Quill layer, but it traverses groups to compute world transforms. As of Feb 2026 it also applies **world visibility/opacity** during flattening so hidden/transparent groups suppress or fade descendants.
