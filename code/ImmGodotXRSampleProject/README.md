# IMM Godot XR sample

This project is intentionally separate from `ImmGodotSampleProject`: it enables OpenXR at Godot startup, while the desktop sample and its CI remain non-XR.

1. Start the OpenXR runtime and connect the headset on Windows, or prepare an OpenXR-capable Android device.
2. Import this directory as a Godot 4.7 project.
3. Run the project.

The repository includes Windows and Android ARM64 plugin binaries. To replace them with a local build, run `code/projects/windows/build-godot-extension.ps1` or `code/projects/android/build-godot-extension-android.ps1` with the required configuration. Both build scripts update this project and `ImmGodotSampleProject`.

The `ImmViewer` node and the `ImmViewerCompositorEffect` on `XRCamera3D` are authored in `scenes/XRSampleScene.tscn`. Select them in the editor to customize the document, renderer, transform, playback, logging, and compositor settings without changing `xr_sample_controller.gd`.

The sample loads `exampleImmFiles/sample1.imm`. It reports ready only after the compositor confirms two XR views and successful rendering of both eye layers.

## Quest Android export

1. Install export templates matching your Godot editor and configure its Android SDK, Java SDK, and debug keystore settings. The checked-in preset uses these editor settings rather than machine-specific paths.
2. Install the official Godot OpenXR Vendors addon into `addons/godotopenxrvendors`. The headset build was tested with addon `5.1.0-stable` and Godot `4.7.2.stable.official.ed1daf0bf`.
3. Install the Android build template through Godot. The `Quest Debug` preset enables Gradle, ARM64, OpenXR, the Meta vendor plugin, and the Mobile renderer.
4. Copy `exampleImmFiles/sample1.imm` from the repository into this project before exporting. The preset includes IMM files; the controller extracts the packaged sample into writable application storage because the native loader requires a filesystem path.
5. Export the `Quest Debug` preset and install the APK on the headset.

The downloaded vendor addon, Android build template, copied sample, and build outputs remain local and are ignored by Git. They must be installed or copied on a fresh checkout before Android export.
