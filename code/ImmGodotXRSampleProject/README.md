# IMM Godot Windows XR sample

This project is intentionally separate from `ImmGodotSampleProject`: it enables OpenXR at Godot startup, while the desktop sample and its CI remain non-XR.

1. Start the PC OpenXR runtime and connect the headset.
2. Build the Windows Godot extension from the repository root with `code/projects/windows/build-godot-extension.ps1 -Configuration Release`.
3. Import this directory as a Godot 4.6 project.
4. Run the project.

The sample loads `exampleImmFiles/sample1.imm`. It reports ready only after the compositor confirms two XR views and successful rendering of both eye layers.
