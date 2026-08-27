# IMM Godot XR sample

This project is intentionally separate from `ImmGodotSampleProject`: it enables OpenXR at Godot startup, while the desktop sample and its CI remain non-XR.

1. Start the OpenXR runtime and connect the headset on Windows, or prepare an OpenXR-capable Android device.
2. Import this directory as a Godot 4.6 project.
3. Run the project.

The repository includes Windows and Android ARM64 plugin binaries. To replace them with a local build, run `code/projects/windows/build-godot-extension.ps1` or `code/projects/android/build-godot-extension-android.ps1` with the required configuration. Both build scripts update this project and `ImmGodotSampleProject`.

The `ImmViewer` node and the `ImmViewerCompositorEffect` on `XRCamera3D` are authored in `scenes/XRSampleScene.tscn`. Select them in the editor to customize the document, renderer, transform, playback, logging, and compositor settings without changing `xr_sample_controller.gd`.

The sample loads `exampleImmFiles/sample1.imm`. It reports ready only after the compositor confirms two XR views and successful rendering of both eye layers.
