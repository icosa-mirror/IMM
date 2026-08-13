# iOS Product and Validation Plan

## Status

This is the active plan for closing the repository's iOS product and validation
gaps. The existing iOS workflow builds the Unity and Stroke Reader static
libraries and link-checks the Unity archive, but it does not build an
application, run Metal rendering, package Godot, or provide a standalone viewer.

Implementation proceeds from the smallest reusable foundation to the largest
new product surface:

1. Unity iOS player export and Xcode compilation.
2. Unity iOS Metal runtime and visual validation.
3. Godot iOS packaging, runtime, and visual validation.
4. Standalone iOS viewer, runtime, and visual validation.

The reverse order describes implementation complexity: standalone is hardest,
then Godot, then Unity visual validation, with Unity player compilation the
easiest gap.

Audio implementation work is being handled separately. This plan must preserve
and consume that work rather than introduce a competing audio implementation.

## Validation principles

1. A library compile or link smoke proves only native package compatibility.
2. A Unity or Godot application build proves packaging and linkage, not visual
   correctness.
3. A visual pass requires a captured image compared with a reviewed reference
   using tolerant spatial/content metrics and, for engine integrations, the
   depth-composition probes.
4. Missing runtime or visual evidence fails closed and is classified as
   `evidence_incomplete`; it must not be reported as a rendering failure.
5. Logs are fast-fail diagnostics. They cannot substitute for a passing image.
6. iOS Godot and Unity cells remain gray until their runtime visual evidence
   exists. A compile-only result must not turn either cell green or yellow.
7. The standalone iOS cell remains gray until an actual viewer target exists
   and passes visual validation.
8. The main report must always contain the iOS row with Standalone, Godot, and
   Unity cells. Each phase must update its existing cell rather than introduce a
   detached secondary matrix or omit the target from the main report.

## Phase 1: Unity iOS player export and compile

Status: cloud export and unsigned device Xcode build confirmed.

Focused run `31581617516` confirmed Unity export, native archive staging, and
generated-project structure. Xcode then failed while linking `UnityFramework`:
the combined IMM archive contains libpng and importer zlib calls, but the Unity
project did not link zlib. The iOS postprocessor now adds SDK `libz.tbd` to the
`UnityFramework` target. This is a real application-link dependency that the
earlier CMake smoke could not expose because that smoke linked `ZLIB::ZLIB`
directly.

Focused run `31582766456` confirmed the fix at commit `b5a6cef8`: the Unity
postprocessor added `libz.tbd`, Unity exported the Metal player, and Xcode
completed the unsigned arm64 device application build. This completes the
Phase 1 compile/package milestone; it does not constitute runtime visual
evidence, so the Unity iOS visual-matrix cell remains not tested.

Iteration note: commits containing `[CI IOS]` run the focused iOS workflow and
skip the full validation pipeline. The focused workflow rebuilds the iOS native
libraries, exports Unity, and compiles Xcode in one macOS job. Generated Xcode
output and derived data are not uploaded; only compact diagnostic logs,
contracts, and manifests are retained. Full `[CI VALIDATION]` remains the
cross-platform regression gate after the focused lane is stable.

### Implementation

1. Add a deterministic Unity Editor build method for `BuildTarget.iOS` using
   `Assets/Scenes/SampleScene.unity` and an explicit output directory.
2. Disable XR initialization for the ordinary iOS 2D sample build.
3. Require Metal and the existing ARM64 iOS native libraries.
4. Export an Xcode project with Unity 6.0 LTS from the current sample project.
5. Compile the generated application with `xcodebuild` without requiring code
   signing.
6. Confirm that the generated player links both `libImmUnityPlugin.a` and
   `libImmStrokeReader.a` and the required Apple frameworks.
7. Preserve the existing low-cost native archive/link smoke as an earlier
   failure point.
8. Upload the Unity Editor log, Xcode build log, generated-project contract,
   and a typed CI manifest.
9. Add a supported non-visual `application-build` matrix row while retaining
   the separate unsupported `non-vr/metal` row that controls the gray Unity iOS
   cell until Phase 2 supplies runtime depth evidence.

### Tests

1. Add a static contract test for the Unity build method and workflow wiring.
2. Fail if the scene, target, Metal selection, non-XR setting, output path, or
   Xcode no-sign compile step disappears.
3. Fail if the lane claims visual success or changes the iOS visual-matrix cell.

### Exit criterion

The cloud workflow exports the current Unity sample as an iOS Xcode project and
successfully compiles the application for an iOS target without signing. The
result is reported as application-build evidence, not visual evidence.

## Phase 2: Unity iOS Metal visual validation

Status: complete. Focused and aggregate cloud runtime, Metal rendering, visual
validation, depth composition, retained evidence, and report integration are
confirmed.

Focused run `31690240268` at commit `96d71023` built arm64 iOS Simulator native
archives, exported and linked the Unity 6 player, booted an installed iPhone
simulator on iOS 18.2, and ran the application twice through Metal. It retained
render-only, full-depth, and ordered-overlay captures. All three localized
tolerant visual contracts passed. Manual inspection confirmed recognizable IMM
content and the intended depth ordering: magenta and yellow probes are visible,
while cyan is absent from the character interior. Cyan portions outside the
character silhouette are intentionally visible and are not depth leakage. The
older broad in-player region diagnostic still reports that valid rim, so it is
retained as a warning rather than allowed to override the localized visual
contract documented in `VALIDATION_FALSE_FAILURE_CLEANUP_PLAN.md`.

Aggregate run `31691425995` at commit `607a8f36` then confirmed the reusable
iOS lane passes inside the full validation workflow, the aggregate report
classifies `unity/ios/non-vr/metal` as passed, the main Unity/iOS matrix cell is
green, and all three authoritative captures are retained in
`CIValidationEvidence`. The workflow itself was red because of independent
stale matrix-count, macOS wide-string logging, and Android render-phase video
timing checks; those did not change the passing iOS evidence and were corrected
before the final aggregate-green confirmation.

Final aggregate run `31698422979` at commit `5a4f118c` completed successfully.
The reusable Unity iOS Metal Simulator lane, application-build row, aggregate
Engine Evidence Report, and final Validation Evidence job all passed. The main
report records `unity/ios/application-build/metal` as passing non-visual build
evidence and `unity/ios/non-vr/metal` as passing visual evidence, with the Unity
iOS matrix cell green. The aggregate artifact retains the render-only,
full-depth, and ordered-overlay captures plus their supporting diagnostic
images and typed manifest. Manual review of all six retained iOS images
confirmed recognizable IMM content at the intended camera position and correct
probe depth/ordering. The manifest independently records rendering,
compositing, depth composition, and ordered overlay as successful.

### Implementation

1. Select a repeatable execution target:
   1. Prefer an Apple-silicon iOS Simulator if Unity, the native archives, and
      Metal rendering can all run there.
   2. Add simulator-compatible native slices or XCFramework packaging if the
      player otherwise works but the device-only ARM64 archives block it.
   3. Use a physical/cloud iOS device only if the simulator cannot provide a
      representative Metal result; record signing and service requirements
      explicitly.
2. Reuse `SampleScene`, `sample1.imm`, the fixed playback timestamp, camera
   viewpoint, render-only capture, full-depth probes, and ordered-overlay probes.
3. Capture the application-rendered frame rather than an Editor-only preview.
4. Compare render-only and composition captures against the committed tolerant
   contracts.
5. Add black/default-scene, displaced-camera, reverse-Z, missing-content, and
   incorrect-occlusion negative tests where existing fixtures do not already
   cover the lane classifier.
6. Add the Unity iOS Metal result to the aggregate evidence report and visual
   matrix only after it has authoritative runtime images.

### Exit criterion

The generated Unity iOS application runs with Metal, renders recognizable IMM
content, passes depth composition, and produces retained evidence in CI.

## Phase 3: Godot iOS packaging and visual validation

### Current status (2026-08-13)

1. Complete and proven in focused CI: the Godot iOS static GDExtension and
   XCFramework build, ordinary sample-project export, unsigned arm64 iPhoneOS
   application compilation, application bundle assembly, and IPA packaging.
2. The focused lane uses an exact native-build cache, skips the unrelated main
   build through `[CI IOS] [CI IOS GODOT]`, records the current Firebase iOS
   device catalogue, and reports the Testing API matrix result directly even
   when Cloud Tool Results is disabled.
3. Firebase Game Loop scheduling was proven on both iPhone 14 Pro / iOS 16.6
   and iPhone SE 3 / iOS 18.4 after removing Godot's Test-Lab-incompatible
   `iphone-ipad-minimum-performance-a12` metadata token from the validation IPA.
   The ordinary exported project and render code remain unchanged.
4. Physical execution is currently blocked before application startup. Firebase
   makes three infrastructure/install attempts on either device pool and writes
   no device log, video, or screenshot. Retained signing diagnostics show the
   uploaded IPA is ad-hoc signed, has no TeamIdentifier, and contains no
   `embedded.mobileprovision`.
5. The repository currently has no Apple signing certificate or provisioning
   profile secrets. A development- or ad-hoc-distribution-signed IPA is therefore
   the next required input. Until it is available, the Godot iOS visual/depth
   matrix cell must remain unpromoted and Phase 3 is not complete.

### Implementation

1. Build the Godot native integration and its dependencies for the required iOS
   architecture and package format.
2. Add the iOS library/framework mapping expected by the Godot extension and
   export system.
3. Add a reproducible iOS export preset for the existing sample project without
   changing the normal Run-button scene.
4. Export and compile an iOS application in CI without signing where possible.
5. Run the application on the selected simulator/device infrastructure.
6. Reuse the Godot Metal render, full-depth, ordered-overlay, Run-button, and
   crash/device-loss contracts.
7. Add the Godot iOS cell to the supported matrix only after application and
   visual evidence exist.

### Exit criterion

The ordinary Godot sample exports to iOS, runs through Metal, renders the IMM
scene with correct depth ordering, and passes the retained visual contracts.

## Phase 4: standalone iOS viewer and visual validation

### Implementation

1. Define the standalone iOS product behavior and minimum supported OS/device.
2. Extract reusable Metal-player logic from the macOS viewer without importing
   AppKit-specific application behavior.
3. Add a UIKit application shell, `CAMetalLayer` presentation, lifecycle and
   resize/orientation handling, asset/file loading, and input behavior.
4. Integrate the shared audio implementation and iOS interruption/lifecycle
   behavior after the audio workstream lands.
5. Add an Xcode/CMake application target, resources, bundle metadata, packaging,
   and signing-independent CI compilation.
6. Run it on the selected simulator/device target and capture the standard IMM
   render evidence.
7. Add release packaging only after runtime and visual validation pass.

### Exit criterion

The standalone iOS viewer builds as an application, loads `sample1.imm`, renders
the complete scene through Metal, survives ordinary lifecycle transitions, and
passes visual validation. Standalone does not require the engine depth-probe
classification used for Unity and Godot.

## Final completion audit

1. Verify all iOS application targets compile from a clean checkout.
2. Download and manually inspect every new iOS image before accepting its
   automated verdict.
3. Confirm Unity and Godot are green only with passing depth composition.
4. Confirm standalone is green with a correct render and does not inherit an
   inapplicable engine-depth requirement.
5. Confirm failures distinguish compilation, packaging, runtime, infrastructure,
   missing evidence, rendering, and composition.
6. Confirm Windows, Android, macOS, Web/WASM, and Quest Multi Pass validation
   remain unchanged.
