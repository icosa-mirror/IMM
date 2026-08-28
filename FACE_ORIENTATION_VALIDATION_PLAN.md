# Face-orientation validation plan

## Objective

Add a deterministic render that directly proves whether front faces are shown and back faces are culled. The diagnostic image and its machine-readable verdict must be included in the final CI validation report for human inspection.

## Fixture

The fixture must be an IMM document containing only:

1. A closed convex square bipyramid whose outward-facing exterior must remain visible.
2. A clearly asymmetric ribbon that proves the document and camera loaded correctly.
3. A single-sided open tunnel whose back-facing interior must remain hidden from the validation camera.
4. Flat diagnostic colours and a fixed camera.

It must not depend on textures, transparency, lighting, animation, post-processing, or the normal sample scene.

## Validation contract

1. Render the fixture from a deterministic viewpoint and resolution.
2. Assert that the convex exterior occupies its expected image region.
3. Assert that the asymmetric layout marker occupies its expected image region.
4. Assert that the open tunnel remains dark rather than exposing coloured back faces.
5. Save the capture, diagnostic overlay, numerical measurements, and pass/fail classification.
6. Add the capture and verdict to each lane report and the final consolidated validation report.

## Platform coverage

Use the same fixture through the actual host integration wherever that integration is supported:

1. Standalone: OpenGL, DirectX, Vulkan, GLES, and Metal.
2. Godot: Windows, Android, macOS, and iOS.
3. Unity: Windows, Android, macOS, and iOS.
4. XR: reuse the diagnostic when an appropriate hardware validation lane is available; lack of XR hardware must not prevent non-XR validation.

## Execution order

1. Add the fixture, classifier, report output, and static contract tests without changing renderer winding or visual thresholds.
2. Prove the diagnostic locally on the available Windows paths.
3. Commit the diagnostic independently.
4. Pull before pushing, then run one full CI validation pass.
5. Use the explicit diagnostic results to make targeted renderer fixes.
6. Retain the detailed sample's surface-detail comparison as secondary regression coverage.

## Decision rule

1. If the explicit diagnostic fails, fix the affected integration's face-orientation convention.
2. If it passes while the detailed sample still fails, investigate geometry generation, LOD, sampling, or backend-specific rendering instead of changing winding.
3. Do not weaken the existing surface-detail threshold merely to make current failures pass.
