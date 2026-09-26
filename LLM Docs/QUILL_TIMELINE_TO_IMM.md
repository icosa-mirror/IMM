# Quill Timeline Concepts -> IMM Data Types

This is a mapping from Quill Timeline tutorial concepts to IMM data structures as implemented in this repo.

## Layers
- **Layer types** -> `ImmImporter::Layer::Type`
  - Group -> `Group`
  - Paint -> `Paint`
  - Picture -> `Picture`
  - Sound -> `Sound`
  - Model -> `Model`
  - Spawn Area -> `SpawnArea`
- **Hierarchy** -> `Layer` parent/child pointers (`GetParent`, `GetChild`, `mChildren`)
- **Visibility** -> `Layer::AnimProperty::Visibility` keys
- **Opacity** -> `Layer::AnimProperty::Opacity` keys
- **Transform** -> `Layer::AnimProperty::Transform` keys
- **Locked/Select/Stats/Flatten/Merge** -> editor/UI only (not serialized in IMM)

## Clips
- A clip is represented by a **visibility span** on a layer.
- **Clip start/end** -> `Visibility` keys (true at start, false at end).
- **Multiple clips per layer** -> multiple visibility spans on the same layer.
- **Clip-specific properties** -> additional Opacity/Transform keys inside that time span.

## Key frames
- Stored per-layer as **animation tracks** (`Layer::AnimProperty`).
- Each key = time + value + interpolation.
- Interpolation is **per key**, not per track.

## Stops / Plays
- Stored on the **root layer** as `AnimProperty::Action` keys.
- `AnimAction::Stop` and `AnimAction::Play` define chapter boundaries.
- The player counts these to derive chapter playback behavior.

## Sequence layers
- Implemented as **timeline layers** (group layers with `isTimeline = true`).
- Duration and looping live on the layer: `mDuration`, `mMaxRepeatCount`.
- Looping can be driven by `AnimProperty::Loop` keys (sets repeat count to 0/1).

## Paint layers
- **Paint layer** -> `LayerPaint`
- **Drawing** -> `Drawing` array inside `LayerPaint`
- **Stroke** -> `Element` inside a `Drawing`
- **Points** -> `Point` array inside an `Element`
- **Frame-by-frame animation** -> `LayerPaint` frame -> drawing mapping
- **Holds** -> multiple frames referencing the **same drawing**

## Picture layers
- `LayerPicture`
- **Content type** -> `LayerPicture::ContentType`:
  - Image2D
  - Image360EquirectMono / Stereo
  - Image360CubemapCrossMono / VstripMono
- **Viewer locked** -> `LayerPicture::GetIsViewerLocked()`
- **Formats** -> image assets (PNG/JPEG)

## Model layers
- `LayerModel` (geometry-only in this repo)
- `ShadingModel` exists but current renderer ignores it.
- Mesh assets are **not serialized** in IMM in this repo (import/export stubs).

## Spawn Areas
- `LayerSpawnArea`
- Stored as a layer with volume and locomotion flags.
- `AnimAction::MakeDefault` on a spawn area sets initial viewpoint.

## Playback controls
- Mapped to player API (play/pause/skip/restart).
- "Recording mode" is editor behavior; it produces `AddKey` calls.

## Timeline options (onion skin, snapping, range controls)
- Editor-only; not serialized in IMM.
