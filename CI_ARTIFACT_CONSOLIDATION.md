# CI Artifact Consolidation Notes

Context for the next agent: the GitHub Actions workflow currently uploads several artifacts whose names imply separate products, even when they are only packaging variants or intermediate build outputs. This became more visible while adding Vulkan/Metal/Godot coverage to `.github/workflows/build.yml`.

The most misleading case is the Windows standalone viewer. `ImmViewer-Windows`, `ImmViewerVulkan-Windows`, and `ImmViewer-Windows-VR` all package the same Windows executable, `appImmViewer_Release.exe`; they differ only by bundled settings. `ImmViewerVulkan-Windows` is not a distinct Vulkan binary.

Recommended cleanup order:

1. Merge the Windows viewer artifacts first.
   - Replace `ImmViewer-Windows`, `ImmViewerVulkan-Windows`, and `ImmViewer-Windows-VR` with one `ImmViewer-Windows` artifact.
   - Include the executable once, plus multiple settings files for default, Vulkan sample playback, and VR modes.
   - Update release download steps and docs so Vulkan launch instructions reference the Vulkan settings file inside the combined artifact.

2. Merge Android viewer APKs into one Android viewer artifact folder.
   - `ImmViewer-Android`, `ImmViewer-Android-Vulkan`, and `ImmViewer-Android-VR` may remain separate APK builds because Gradle properties change renderer/VR mode.
   - They can still be uploaded as one artifact, for example `ImmViewer-Android`, containing clearly named APKs such as default, Vulkan, and VR variants.
   - Update release packaging to normalize those APK names from the combined artifact.

3. Leave plugin platform artifacts as intermediate unless they are renamed clearly as internal build artifacts.
   - `ImmStrokeReaderPlugin-{Windows,Android,macOS,iOS}` and `ImmViewerPlugin-{Windows,Android,macOS,iOS}` are consumed by the UPM package job.
   - They could be merged into platform bundles, but separate artifacts make missing-platform failures explicit.
   - If they remain separate, consider naming or documenting them as package-job inputs, not user-facing downloads.

4. Keep final user-facing artifacts distinct by product.
   - Keep `ImmViewerPlugin-UPM`, `ImmStrokeReaderPlugin-UPM`, and `ImmGodotGDExtension` as separate final artifacts.
   - Keep smoke log artifacts such as `ImmGodotSmokeLogs-*` and `ImmViewerVulkanSmokeLogs-Windows` separate and failure-only.
   - Treat `ImmGodotGDExtension-{Windows,Android,macOS}-platform` as intermediate artifacts that are merged into the final `ImmGodotGDExtension` addon.

When changing the workflow, update `code/appImmGodotGDExtension/verify_local.py` token checks and rerun it. Also update release download/zip steps so GitHub releases still contain the intended user-facing packages.
