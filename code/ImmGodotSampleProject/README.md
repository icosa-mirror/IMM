# IMM Godot Sample Project

This project mirrors the intent of the Unity sample project while the native Godot integration is still being built.

## Current status

- The scene and controls are in place.
- The `ImmViewerNode` script mirrors the runtime API shape the future GDExtension will expose.
- The native `appImmGodot` DLL exists in the C++ solution, but it is not wired into Godot yet.
- A `.gdextension` manifest is present under `addons/imm_viewer/`, ready for the future extension binary.

## Open in Godot

1. Open `code/ImmGodotSampleProject` in Godot 4.x.
2. Use the Compatibility renderer path.
3. Run `scenes/SampleScene.tscn`.

## Sample controls

- `L`: load document
- `U`: unload document
- `Space`: pause/resume
- `R`: restart
- `]`: next chapter
- `[`: previous chapter
- `'`: next spawn area
- `;`: previous spawn area
- `W/A/S/D/Q/E`: move camera

## Document path

The default document path is set to `../../../exampleImmFiles/sample1.imm`, which points at the repository sample from this project directory.

## Next integration step

Replace the script-backed `ImmViewerNode` with a GDExtension-backed implementation that calls into `ImmGodotPlugin`.
