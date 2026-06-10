# Automated Testing Matrix Plan

This plan defines a GitHub Actions testing matrix for IMM across standalone players, Unity packages, Godot GDExtension packages, Windows, Android, iOS, macOS, and VR/non-VR validation. It is grounded in the current repository state as of 2026-06-10:

- Existing workflow: `.github/workflows/build.yml`
- Existing fixture: `exampleImmFiles/sample1.imm`
- Existing Windows baseline capture path: `code/appImmViewer/scripts/capture_windows_directx_baseline.ps1`
- Existing Windows Vulkan standalone smoke: `code/appImmViewer/scripts/run-vulkan-sample1-smoke.ps1`
- Existing Windows Godot smoke helpers: `code/projects/windows/run-godot-smoke.ps1` and `code/projects/windows/run-godot-vulkan-visual-baseline-smoke.ps1`
- Existing Android renderer/OpenXR/Godot smoke helpers: `code/projects/android/run-android-*-smoke.ps1`
- Existing macOS Metal standalone validation: `validateAppImmViewerMetalPlayback`
- Existing Godot local verifier: `code/appImmGodotGDExtension/verify_local.py`

The target state is a CI system where every pull request receives deterministic build, API, packaging, and non-device validation on GitHub-hosted runners, while GPU/device/VR validation also runs from GitHub Actions on labeled self-hosted runners.

## Goals

1. Build every supported artifact on every supported platform.
2. Validate standalone, Unity, and Godot integrations against the same baseline source of truth.
3. Cover both non-VR and VR modes where the platform supports them.
4. Separate deterministic hosted-runner gates from hardware-dependent gates without moving hardware testing outside GitHub CI.
5. Publish artifacts, logs, manifests, captures, and comparison reports in a consistent layout for every matrix leg.
6. Make failures classifiable: build failure, packaging failure, API regression, content parsing regression, visual regression, audio regression, runtime launch failure, or VR/device infrastructure failure.

## Baseline Source Of Truth

Use a two-layer baseline, because no single renderer is a complete truth source for every validation type.

### 1. Canonical Content Baseline

Create an engine-independent baseline generated from IMM parsing, not rendering:

- Location: `tests/baselines/content/`
- Generator: `tests/tools/generate_imm_baseline.py`
- Verifier: `tests/tools/verify_imm_baseline.py`
- Initial fixture: `exampleImmFiles/sample1.imm`

Each fixture gets a JSON manifest containing:

- IMM file hash and byte size
- document metadata
- layer count and layer types
- stroke/mesh/picture/sound counts
- bounding box
- spawn-area list and active spawn-area data
- chapter/timeline duration data
- audio object metadata and decode expectations
- renderer-independent invariants such as finite transforms, non-empty geometry, and valid resource references

This becomes the baseline for standalone parser/player loading, Unity C# wrapper API results, and Godot node API results.

### 2. Reference Render Baseline

Keep visual baselines separate from content baselines:

- Location: `tests/baselines/render/`
- Initial reference: Windows standalone DirectX capture from `capture_windows_directx_baseline.ps1`
- Secondary reference: macOS standalone Metal capture from `validateAppImmViewerMetalPlayback`
- Comparison tool: existing `compare-ppm-captures.ps1`, extended or wrapped for PNG/PPM normalization

Reference captures should not require exact full-frame hashes unless the renderer is deterministic enough to support that. Use structured metrics:

- dimensions
- non-background pixel count
- visible content bounding rectangle
- luma/color distribution
- known blank-frame rejection
- old-known-bad hash rejection
- renderer-specific diagnostic markers
- optional perceptual image delta threshold for stable render paths

The render baseline is used to validate standalone Vulkan, Godot Vulkan/Metal visual paths, and device screenshots where exact pixel parity is not realistic.

## Runner Tiers

| Tier | Runner type | Purpose | Required for PR gate |
|---|---|---|---|
| Hosted deterministic | `windows-latest`, `ubuntu-latest`, `macos-latest` | Build, package, content baseline, API checks, headless/script smoke, non-device app bundle checks | Yes |
| Hosted Apple build | `macos-latest` | macOS viewer, iOS static libraries/packages, iOS simulator build checks where possible | Yes |
| Self-hosted GPU | GitHub Actions runner with discrete/integrated GPU and display session | Windows Vulkan, Godot Vulkan, macOS Metal visual smoke if hosted GPU is insufficient | Required before merge to protected branches once stable |
| Self-hosted Android device | Runner with Android SDK, ADB, Quest/Android phone attached | Android non-VR GLES/Vulkan launch, Android Godot Vulkan launch, Android OpenXR probe, Quest VR launch | Nightly initially, then required for release |
| Self-hosted Apple device | macOS runner with Xcode and attached iOS device if runtime validation is added | iOS package/device smoke beyond static build | Nightly/release |
| Self-hosted VR | Runner with supported headset and runtime session | Windows PC VR and Quest VR validation | Nightly/release until stable enough for protected branch gate |

All tiers are still GitHub CI because self-hosted runners are invoked by GitHub Actions jobs and report checks on the same workflow.

Self-hosted jobs run `tests/tools/preflight_runner.py` before the expensive smoke steps. The preflight writes `preflight.json` into the job artifact, checks required commands such as `adb`, `msbuild`, `cmake`, `glslangValidator`, and `spirv-val`, checks required environment variables such as `GODOT_EXE` or `UNITY_EXE`, and records `adb devices -l` for Android/Quest lanes. This keeps infrastructure failures distinct from product regressions.

## Product Matrix

| Product surface | Windows | Android | iOS | macOS |
|---|---|---|---|---|
| Standalone non-VR | Build and launch DirectX/OpenGL/Vulkan; compare Vulkan to DirectX baseline | Build APK; launch GLES and Vulkan on device/emulator; compare runtime markers and screenshot metrics | Not currently a standalone target in repo; track as unsupported unless a viewer target is added | Build `.app`; run Metal validation and capture metrics |
| Standalone VR | Build/package OpenGL VR settings; PC VR runtime smoke on self-hosted headset | Build Quest VR APK; launch on Quest; validate OpenXR/session markers | Not currently a standalone target in repo | Track as unsupported unless a macOS XR target exists |
| Unity non-VR | Build native plugin DLL and UPM package; run Unity batchmode sample tests on self-hosted Unity runner | Build `.so`; validate package import and Android player build | Build static library; validate package import and iOS player build | Build bundle/static plugin; validate package import and macOS player build |
| Unity VR | Unity OpenXR sample scene smoke on self-hosted Windows VR runner | Unity Quest/OpenXR build and device smoke | Unity iOS XR only if supported by product roadmap | Track as unsupported unless a macOS XR target exists |
| Godot non-VR | Build GDExtension; run preflight/script smoke on hosted; run headed Vulkan visual smoke on GPU runner | Build Android GDExtension; export APK; run Vulkan visual smoke on device | Track as unsupported until iOS Godot extension packaging exists | Build GDExtension; run Metal visual smoke and capture |
| Godot VR | Godot OpenXR scene smoke on self-hosted Windows VR runner after non-VR renderer parity is stable | Godot OpenXR/Quest smoke after Android Godot non-VR is stable | Track as unsupported until iOS Godot XR target exists | Track as unsupported unless Godot macOS XR target exists |

## Validation Matrix

Use these validation classes consistently across products.

| Class | Baseline | Hosted CI | Hardware CI |
|---|---|---|---|
| Build | Compiler/linker success and expected output files | Required | Required where hardware jobs build locally |
| Package layout | Expected artifact paths, manifests, native library names, dependency manifests | Required | Required |
| Content parse | `tests/baselines/content/*.json` | Required | Required |
| Public API parity | Unity/Godot wrapper output equals canonical content baseline | Required where engine CLI is available | Required |
| Non-VR launch | Process starts, loads fixture, reaches ready state | Partial: hosted where no GPU/device is needed | Required |
| VR launch | Runtime initializes XR/OpenXR/Oculus path and loads fixture | No, except static probe | Required |
| Visual output | Structured render metrics against render baseline | Partial: macOS Metal hosted, Windows DirectX capture hosted | Required for Vulkan/Godot/device paths |
| Audio | Decode count, backend selected, play accepted, teardown clean | Required where existing smoke supports it | Required |
| Repeated lifecycle | load/unload/reload, app relaunch, resource teardown | Required where existing smoke supports it | Required |

## Proposed Workflow Structure

Split the current monolithic `Build` workflow into reusable and targeted workflows over time. The existing `.github/workflows/build.yml` can stay as the release/package workflow until the matrix jobs are stable.

### 1. `ci-core.yml`

Runs on every PR and selected pushes.

Jobs:

- `baseline-content`
  - Generate canonical JSON from `exampleImmFiles/sample1.imm`
  - Compare to committed `tests/baselines/content/sample1.json`
  - Upload generated manifest on failure

- `windows-build`
  - Existing `build-windows` compile path
  - Capture Windows DirectX reference frame
  - Run content baseline CLI verifier against standalone output
  - Build Godot GDExtension
  - Run Godot local verifier and preflight smoke
  - Upload `ImmViewer-Windows`, `Internal-*`, and baseline captures

- `android-build`
  - Existing Android Gradle builds
  - Build non-VR GLES/Vulkan APKs and VR APK
  - Build Android Godot GDExtension
  - Verify APK names, ABI contents, manifest mode flags
  - Upload Android artifacts

- `macos-build`
  - Existing macOS CMake build
  - Run `validateAppImmViewerMetalPlayback`
  - Build macOS Godot GDExtension
  - Run Godot `verify_local.py`
  - Upload macOS viewer, Metal capture, and plugin artifacts

- `ios-build`
  - Existing iOS CMake build/static library packaging
  - Add package-layout verification for Unity iOS plugin outputs
  - Upload iOS plugin artifacts

- `package-unity`
  - Consume platform plugin artifacts
  - Assemble UPM packages
  - Run package manifest verification

- `package-godot`
  - Consume Windows/Android/macOS Godot platform artifacts
  - Assemble addon
  - Run `.gdextension` manifest verification

### 2. `ci-gpu.yml`

Runs on `workflow_dispatch`, nightly schedule, and protected branch pushes. Also runs on PRs when a label such as `gpu-ci` is present.

Jobs:

- `windows-standalone-vulkan`
  - Runner labels: `self-hosted`, `windows`, `gpu`, `vulkan`
  - Command: `code/appImmViewer/scripts/run-vulkan-sample1-smoke.ps1 -Configuration Release -SkipBuild -KeepArtifacts -ReferencePath build/baseline-captures/windows-directx-playerframe60.ppm`
  - Upload Vulkan capture, logs, and compare report

- `windows-godot-vulkan`
  - Runner labels: `self-hosted`, `windows`, `gpu`, `godot`, `vulkan`
  - Command: `code/projects/windows/run-godot-smoke.ps1 -Configuration Release -RequireExtension -RendererApi 5 -Headed`
  - Optional visual-baseline command: `code/projects/windows/run-godot-vulkan-visual-baseline-smoke.ps1`

- `macos-godot-metal`
  - Runner labels: `self-hosted`, `macos`, `gpu`, `metal`, `godot`
  - Run Godot Metal visual smoke scene
  - Compare PNG metrics to standalone Metal and Windows DirectX reference metrics

### 3. `ci-device.yml`

Runs on nightly, release branches, and manual dispatch. It can become required for releases once device stability is proven.

Jobs:

- `android-standalone-gles`
  - Runner labels: `self-hosted`, `android-device`
  - Command: `code/projects/android/run-android-gles-smoke.ps1`

- `android-standalone-vulkan`
  - Runner labels: `self-hosted`, `android-device`, `vulkan`
  - Command: `code/projects/android/run-android-vulkan-smoke.ps1`

- `android-openxr-probe`
  - Runner labels: `self-hosted`, `quest`, `openxr`
  - Command: `code/projects/android/run-android-openxr-probe-smoke.ps1`

- `android-godot-vulkan`
  - Runner labels: `self-hosted`, `android-device`, `godot`, `vulkan`
  - Command: `code/projects/android/run-godot-android-vulkan-smoke.ps1`

- `android-quest-vr`
  - Runner labels: `self-hosted`, `quest`, `vr`
  - Install `ImmViewer-Android-OpenGL-VR.apk`
  - Launch VR activity
  - Require OpenXR/Oculus initialization, document load, stereo frame submission, no runtime blocker markers

- `ios-device-smoke`
  - Runner labels: `self-hosted`, `macos`, `ios-device`
  - Initially package/import validation only
  - Add launch smoke only after an iOS runtime harness exists

### 4. `ci-engine.yml`

Runs engine import/API compatibility checks.

Jobs:

- `unity-package-import`
  - Runner labels: `self-hosted`, `unity`
  - Import `ImmPlayerPlugin-Unity` and `ImmStrokeReaderPlugin-Unity` into a minimal Unity project
  - Run batchmode EditMode tests
  - Verify native libraries are present for Windows, Android, macOS, iOS
  - Verify wrapper API output against canonical content baseline

- `unity-windows-playmode`
  - Runner labels: `self-hosted`, `windows`, `unity`, `gpu`
  - Run sample scene in batchmode or player
  - Validate document load, background color, layer/spawn-area/chapter API, non-VR render markers

- `unity-openxr-vr`
  - Runner labels: `self-hosted`, `windows`, `unity`, `vr`
  - Run OpenXR sample scene against attached headset
  - Validate stereo mode, per-eye matrices, document load, frame submission

- `godot-package-import`
  - Hosted where possible, self-hosted where native rendering is required
  - Import `ImmPlayerPlugin-Godot` into a minimal Godot project
  - Run `verify_local.py`
  - Run script-stub and native smoke
  - Verify `ImmViewerNode` API output against canonical content baseline

## Initial GitHub Actions Matrix

The first implementation can use a compact matrix without creating every final workflow immediately.

```yaml
strategy:
  fail-fast: false
  matrix:
    include:
      - product: standalone
        platform: windows
        mode: non-vr
        renderer: directx
        runner: windows-latest
        tier: hosted
      - product: standalone
        platform: windows
        mode: non-vr
        renderer: vulkan
        runner: [self-hosted, windows, gpu, vulkan]
        tier: gpu
      - product: standalone
        platform: android
        mode: non-vr
        renderer: gles
        runner: [self-hosted, android-device]
        tier: device
      - product: standalone
        platform: android
        mode: non-vr
        renderer: vulkan
        runner: [self-hosted, android-device, vulkan]
        tier: device
      - product: standalone
        platform: android
        mode: vr
        renderer: openxr
        runner: [self-hosted, quest, vr]
        tier: device
      - product: standalone
        platform: macos
        mode: non-vr
        renderer: metal
        runner: macos-latest
        tier: hosted
      - product: unity
        platform: all
        mode: package
        renderer: native
        runner: ubuntu-latest
        tier: hosted
      - product: godot
        platform: windows
        mode: non-vr
        renderer: preflight
        runner: windows-latest
        tier: hosted
      - product: godot
        platform: windows
        mode: non-vr
        renderer: vulkan
        runner: [self-hosted, windows, gpu, vulkan]
        tier: gpu
      - product: godot
        platform: android
        mode: non-vr
        renderer: vulkan
        runner: [self-hosted, android-device, godot, vulkan]
        tier: device
      - product: godot
        platform: macos
        mode: non-vr
        renderer: metal
        runner: macos-latest
        tier: hosted
      - product: ios
        platform: ios
        mode: package
        renderer: native
        runner: macos-latest
        tier: hosted
```

GitHub Actions does not allow `runs-on` values to be selected directly from a mixed string/list matrix without care. Implement this either with separate jobs per runner class or with normalized JSON runner labels and `fromJSON(matrix.runs_on)`.

## Required New Test Artifacts

Add these files/directories:

- `tests/baselines/content/sample1.json`
- `tests/baselines/render/windows-directx-sample1.json`
- `tests/baselines/render/macos-metal-sample1.json`
- `tests/tools/generate_imm_baseline.py`
- `tests/tools/verify_imm_baseline.py`
- `tests/tools/compare_render_metrics.py`
- `tests/tools/collect_ci_artifacts.py`
- `tests/unity/` minimal Unity test project or package import harness
- `tests/godot/` minimal Godot package import harness if the existing `code/ImmGodotSampleProject` is too broad for package-import checks

Do not commit large binary captures directly unless they stay small and stable. Prefer committed JSON metrics plus CI-uploaded capture images. If full reference images are needed, use Git LFS or release assets with checksum pinning.

## CI Artifact Layout

Every matrix leg should upload the same logical structure:

```text
artifacts/
  manifest.json
  logs/
  captures/
  metrics/
  packages/
  compare/
```

`manifest.json` should include:

- git SHA
- workflow/job/matrix identifiers
- product/platform/mode/renderer
- runner labels and OS
- tool versions: compiler, CMake, Gradle, Android SDK/NDK, Xcode, Unity, Godot, Vulkan SDK
- fixture hashes
- artifact hashes
- pass/fail classification

`tests/tools/collect_ci_artifacts.py` writes `artifact-summary.json` files that summarize uploaded directories, embedded manifests, preflight diagnostics, and metrics JSON. Hosted core and package artifacts include these summaries so release audits can inspect a run without unpacking every artifact by hand.

## Gating Policy

Machine-readable support status lives in `tests/matrix_status.json`. CI verifies that every planned product/platform/mode has either a supported gate, a deferred reason, an unsupported reason, or an explicit waiver. Treat this file as the release audit source for rows that cannot yet run on hosted CI.

Workflow wiring is checked by `tests/tools/verify_workflow_matrix.py`. It fails CI if required matrix jobs, self-hosted labels, preflight steps, package verifiers, artifact manifests, or release verification steps drift out of the workflow YAML.

### Pull Requests

Required:

- hosted builds for Windows, Android, macOS, iOS
- content baseline verification
- package layout verification
- Godot local verifier/preflight
- macOS Metal standalone validation
- Windows DirectX baseline capture

Optional or label-triggered:

- Windows Vulkan GPU smoke
- Godot Vulkan GPU smoke
- Android device smoke
- Unity editor/package import

### Protected Branches

Required after stabilization:

- all PR gates
- Windows Vulkan standalone smoke on self-hosted GPU
- Windows Godot Vulkan smoke on self-hosted GPU
- Android GLES/Vulkan non-VR smoke on device
- macOS/Godot Metal visual smoke

### Nightly

Required:

- full hosted matrix
- all GPU matrix legs
- all Android device matrix legs
- Unity import and PlayMode checks
- VR/OpenXR/Quest checks
- baseline drift report

### Release

Required:

- full nightly matrix green on the release SHA
- release artifacts verified from downloaded GitHub Actions artifacts, not from local build outputs
- package install/import smoke for Unity and Godot
- Android APK install smoke for each released APK
- Windows and macOS viewer launch smoke from release zips

## Implementation Phases

### Phase 1: Baseline Infrastructure

1. Add `tests/baselines/content/sample1.json`.
2. Add generator/verifier scripts.
3. Add CI job that fails when the generated baseline differs from the committed baseline.
4. Add render metrics JSON for existing Windows DirectX and macOS Metal captures.
5. Standardize artifact manifest generation for existing workflow jobs.

Exit criteria:

- PR CI proves `sample1.imm` parser/player invariants without launching Unity or Godot.
- Windows and macOS render captures produce comparable metrics artifacts.

### Phase 2: Hosted Matrix Cleanup

1. Split build/package/test concerns in `.github/workflows/build.yml` or add new reusable workflows.
2. Promote existing Windows, Android, macOS, and iOS builds into named matrix legs.
3. Add package layout verification after Unity and Godot packaging.
4. Add failure-only logs for every smoke job.
5. Keep release packaging dependent on the same artifacts tested by CI.

Exit criteria:

- A PR shows explicit check names for each platform and package surface.
- Artifact names map directly to user-facing products and intermediate package inputs.

### Phase 3: Unity Validation

1. Create a minimal Unity import test harness.
2. Import generated UPM packages, not source folders.
3. Run EditMode tests for wrapper API and package layout.
4. Add PlayMode/player smoke for Windows non-VR.
5. Add Android player build validation.
6. Add OpenXR/VR Unity smoke on self-hosted VR hardware.

Exit criteria:

- Unity package artifacts are proven installable and callable on every supported plugin platform.
- VR Unity checks run in GitHub Actions on self-hosted hardware.

### Phase 4: Godot Validation

1. Continue using `verify_local.py` as the fast hosted gate.
2. Add package import test against the assembled `ImmPlayerPlugin-Godot` artifact.
3. Add Windows Vulkan visual smoke on self-hosted GPU.
4. Add Android Godot Vulkan device smoke.
5. Add macOS Godot Metal visual smoke to hosted or self-hosted CI depending on runner reliability.
6. Add Godot OpenXR/VR smoke after non-VR renderer parity is stable.

Exit criteria:

- The shipped Godot addon is proven installable and runnable from its packaged artifact.
- Visual smoke captures are compared against render metrics on each renderer path.

### Phase 5: Android And VR Device CI

1. Register self-hosted Android/Quest runners with stable labels.
2. Add device health preflight: `adb devices`, battery/power state, display awake state, package uninstall cleanup.
3. Run GLES, Vulkan, OpenXR probe, and Quest VR APKs.
4. Pull logs/screenshots into the standard artifact layout.
5. Classify infrastructure blockers separately from product failures.

Exit criteria:

- Android and Quest validation runs automatically from GitHub Actions.
- Failures preserve enough logs to diagnose without user intervention.

### Phase 6: Release Enforcement

1. Require a green full matrix before release jobs run.
2. Download release artifacts and rerun layout/smoke checks from downloaded assets.
3. Include baseline and validation summaries in release notes.
4. Block releases if any supported product/platform/mode lacks either a passing gate or an explicit unsupported/waived status.

Exit criteria:

- A release cannot be created from unvalidated local outputs or partially checked artifacts.

## Current Coverage Gap Summary

| Requirement | Current state | Gap |
|---|---|---|
| Standalone Windows non-VR | Builds, DirectX baseline capture, gated Vulkan smoke variable exists | Need formal baseline metrics and self-hosted GPU requirement |
| Standalone Windows VR | Packaged settings exist | Need PC VR runtime smoke |
| Standalone Android non-VR | Builds GLES/Vulkan APKs; smoke scripts exist | Need GitHub self-hosted device runner |
| Standalone Android VR | Builds Quest-style APK | Need Quest runtime smoke and stereo/OpenXR markers |
| Standalone iOS | No standalone viewer target identified | Mark unsupported or add target |
| Standalone macOS non-VR | Metal app build and validation exist | Need formal render metrics baseline |
| Standalone macOS VR | No target identified | Mark unsupported |
| Unity packages | Multi-platform package assembly exists | Need Unity import/API tests from packaged artifacts |
| Unity VR | Plugin/runtime likely capable on supported platforms | Need explicit OpenXR scene and hardware CI |
| Godot package | GDExtension packaging exists | Need package import test and hardware visual gates |
| Godot VR | Not yet established | Defer until non-VR Godot parity is stable |
| Baseline source of truth | Existing sample and render captures exist | Need committed canonical JSON and render metrics |

## Definition Of Done

This plan is implemented when:

1. GitHub Actions exposes check results for every supported product/platform/mode row.
2. Every supported row has build, package, content baseline, and runtime validation appropriate to its platform.
3. VR rows run through GitHub Actions on self-hosted headset/device runners.
4. Unsupported rows are explicitly listed with a reason and owner decision.
5. Baseline updates are reviewed as source changes, not silently regenerated in CI.
6. Release artifacts are validated after download from GitHub Actions.
7. CI artifacts include enough logs, captures, metrics, and manifests to diagnose failures without asking for console output.
