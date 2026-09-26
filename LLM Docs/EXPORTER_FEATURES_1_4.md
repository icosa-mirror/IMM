# Exporter Notes (Points 1-4)

## 1) Multiple drawings / frames (animated paint)
- A paint layer can contain multiple drawings.
- Each drawing is a collection of strokes (elements).
- Animation is a frame -> drawing mapping stored by the layer.
- For animated sequences, create N drawings and add N frames (each frame references a drawing index).
- Still sequences typically use one drawing with all strokes.

## 2) Brush section and visibility types
- `BrushSectionType` controls stroke cross-section shape:
  - Point, Segment, Circle, Ellipse, Square.
- `VisibilityType` controls how the stroke fades:
  - FadePow2 or Always.
- These are per-element (per stroke).

## 3) Per-layer opacity / visibility / transform
- Layer visibility and opacity are set on creation.
- Layer transform is a uniform-scale transform (translation + rotation + uniform scale).
- These apply to all strokes in the layer (or groups if the layer is a group).

## 4) Timeline vs still sequences
- Sequence type can be Still or Animated (Comic exists but is not wrapped).
- Still: one drawing / one frame is enough.
- Animated: set a frame rate and add multiple frames pointing to drawings.

## Missing in current wrappers
- Animation keys (Layer::AddKey) and advanced timeline features.
- Non-paint layers (Picture, Sound, Model, SpawnArea).
- Export to memory (only export to file is wrapped).

## Comic sequence type
- `Sequence::Type::Comic` is a document type intended for chaptered, timeline-driven playback.
- "Comic" is not just "Animated + chapters". It assumes chapter-style playback semantics
  driven by **Action keys** on the root layer and other timeline controls used by the player.
- Our Unity exporter wrapper does not expose any comic-specific metadata or actions yet, so
  setting Comic now would only tag the file without those semantics.

## Animation and keyframes (native, not wrapped yet)
### Conceptual hierarchy
1. A Layer has one animation track per property (AnimProperty).
2. Each track contains multiple keys (AnimKey).
3. Each key has a time, a value, and an interpolation type.
4. InterpolationType is chosen per key (not global to the track).

### AnimProperty (track type -> value type)
- Visibility -> bool
- Opacity -> float
- Position -> transform
- Rotation -> transform
- Scale -> transform
- DrawInTime -> double
- Action -> uint (AnimAction enum)
- Loop -> bool
- Offset -> uint
- Transform -> full transform

### What the special tracks mean
- **Action**: chapter/playback markers used by the player.
  - Used on the **root layer** to define chapter boundaries and playback behavior.
  - The player counts `Play` and `Stop` actions to compute chapters, and uses `Loop`
    to set a loop point for the whole document.
  - `MakeDefault` is used on SpawnArea layers to set the default spawn area.
- **Offset**: per-layer time offset applied when a visibility span starts.
  - When a Visibility key turns a layer on, the player looks for a same-time Offset key
    and uses it to shift the layer's local playback time.
- **Loop**: per-layer looping flag.
  - A Loop key sets max repeat count: `true` -> repeat forever (count = 0),
    `false` -> play once (count = 1).
  - For paint layers, this affects animated stroke playback (multiple frames).

### InterpolationType (per key, not global)
- None, Linear, Smoothstep, EaseIn, EaseOut, Spline, Auto

### AnimAction (values used by the Action track)
- Stop, Play, Loop, MakeDefault
