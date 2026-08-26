# IMM Godot Windows XR sample

This project is intentionally separate from `ImmGodotSampleProject`: it enables OpenXR at Godot startup, while the desktop sample and its CI remain non-XR.

1. Start the PC OpenXR runtime and connect the headset.
2. Import this directory as a Godot 4.6 project.
3. Run the project.

The repository includes the Windows plugin binaries required by the editor. To replace them with a local build, run `code/projects/windows/build-godot-extension.ps1 -Configuration Debug` for Editor play or `-Configuration Release` for an exported release build. The build script updates both this project and `ImmGodotSampleProject`.

The `ImmViewer` node and the `ImmViewerCompositorEffect` on `XRCamera3D` are authored in `scenes/XRSampleScene.tscn`. Select them in the editor to customize the document, renderer, transform, playback, logging, and compositor settings without changing `xr_sample_controller.gd`.

The sample loads `exampleImmFiles/sample1.imm`. It reports ready only after the compositor confirms two XR views and successful rendering of both eye layers.
