# Vulkan and OpenXR Adoption Plan

## Goal

Use Vulkan where it is the right backend on Windows and Android, and move VR
support toward OpenXR without coupling renderer migration and XR runtime
migration into one large change.

The plan deliberately separates three concerns:

1. Vulkan as a non-VR renderer.
2. OpenXR as the XR runtime abstraction.
3. Vulkan/OpenXR graphics interop.

Those areas share code at the final stereo image boundary, but they have
different failure modes. Keeping them staged gives each change a clear
validation target.

## Current State

### Standalone Windows

- The standalone player already has a renderer setting named `RenderingAPI`.
- `settings.cpp` accepts `OpenGL`, `DirectX`, `Metal`, and `Vulkan`.
- Standalone settings now include `XRRuntime`, with `Legacy` and `OpenXR`
  values.
- `mymain.cpp` maps the Vulkan setting to `piRenderer::API::Vulkan`.
- The default `code/appImmViewer/exe/settings.json` currently selects Vulkan and
  disables VR with `XRRuntime: Legacy`.
- `code/appImmViewer/exe/settings-opengl.json` preserves the OpenGL
  fallback/reference path.
- `code/appImmViewer/exe/settings-openxr-probe.json` selects Vulkan, enables
  VR, and requests `XRRuntime: OpenXR` for explicit startup-boundary testing.
- Non-VR Vulkan presentation has an explicit display-sRGB output path after the
  recent color-space fix.

### Android

- The Android non-VR path already has GLES/Vulkan selection logic.
- Android non-VR now defaults to Vulkan for normal builds, while the Gradle
  property remains available to select the startup renderer.
- Android can request `RenderingAPI` through intent extras before native
  renderer initialization.
- Android-created `Settings` objects explicitly use `XRRuntime: Legacy`.
- Android VR currently has a separate Oculus/GLES-oriented path.
- Vulkan should be validated first in the non-VR Android player before it is
  connected to XR swapchains.

### Existing VR

- Windows VR uses the existing `piVRHMD` abstraction.
- The Vive/OpenVR submit path currently submits textures as
  `vr::TextureType_OpenGL`.
- The Oculus path creates SDK swapchain textures and wraps those IDs through the
  renderer.
- Vulkan external texture wrapping is currently marked unsupported in the Vulkan
  renderer.

That means the standalone player can select Vulkan today, but Windows VR over
Vulkan is not just a settings change. The missing piece is Vulkan-aware XR
swapchain/image ownership and submission.

## Strategy

Do not combine Vulkan adoption and OpenXR adoption as a single task.

Instead, build toward the desired end state with staged vertical slices:

1. Make Vulkan reliable for flat rendering on Windows and Android.
2. Add OpenXR with the smallest useful renderer integration.
3. Add Vulkan/OpenXR interop once both sides are independently understood.

This avoids debugging renderer correctness, XR lifecycle, controller input,
swapchain ownership, synchronization, and color-space behavior all at once.

## Architecture Direction

### Renderer Boundary

The renderer should expose what it already owns:

- API type: OpenGL, GLES, DirectX, Metal, Vulkan.
- Texture creation and render target binding.
- Final resolve/output encoding intent.
- Native graphics handles only through explicit backend-specific structures.

Avoid treating a renderer texture as a generic integer ID for Vulkan. Vulkan
needs image handles, image views, formats, usage flags, layouts, queues, and
synchronization state. That data should be passed through typed structures, not
through `unsigned int` texture IDs.

### XR Boundary

OpenXR should own:

- Instance and system selection.
- Session lifecycle.
- Reference spaces.
- View configuration.
- Swapchain creation.
- Frame wait/begin/end.
- Action sets and input bindings.
- Runtime-specific graphics requirements.

The app should receive per-eye render targets from the XR layer, render into
them through the renderer, and return ownership at the frame boundary.

### Interop Boundary

The Vulkan/OpenXR interop layer should be explicit about:

- Vulkan instance extensions required by the OpenXR runtime.
- Vulkan device extensions required by the OpenXR runtime.
- Physical device selection constraints from OpenXR.
- Queue family selection.
- `XrGraphicsBindingVulkanKHR`.
- `XrSwapchainImageVulkanKHR` image enumeration.
- Image format selection.
- Image layout transitions before and after rendering.
- Synchronization between renderer work and `xrEndFrame`.

Do not hide this behind the existing `CreateTextureFromID(unsigned int)` API.
That API is too weak for Vulkan.

### Standalone Backend Selection

All standalone players should use the same backend-selection model:

1. Runtime settings request the desired renderer.
2. Build configuration declares which renderers are compiled into the package.
3. Startup resolves the request against available compiled backends.
4. If the requested backend is unavailable, behavior is explicit:
   - strict mode fails with a clear diagnostic.
   - fallback mode selects the configured fallback and logs the decision.

The requested renderer should be represented by the same setting concept on all
standalone platforms. Windows already has `RenderingAPI` in `settings.json`.
Android should move toward the same model instead of using a separate build per
renderer as the primary selection mechanism.

Android build flags should become capability/default flags rather than the
normal user selection path. For example, an Android non-VR package can compile
both Vulkan and GLES, default to Vulkan at runtime, and keep GLES available as a
fallback. Special-purpose packages can still compile only one backend when size,
distribution, store policy, or device support requires it, but those packages
should still use the same runtime setting names so behavior stays explainable.

The shared policy should be:

- Non-VR default: Vulkan on Windows and Android.
- Non-VR fallback: OpenGL on Windows, GLES on Android.
- Runtime selector name: `RenderingAPI` on every standalone player.
- XR runtime selector name: `XRRuntime` where VR startup is possible.
- VR default: the existing known-good legacy backend until OpenXR/Vulkan is
  validated on target hardware.
- OpenXR/Vulkan: selected explicitly until the full XR path is ready to become
  default.

## Phase 1: Vulkan Non-VR Baseline

### Windows Standalone

1. Keep `RenderingAPI: Vulkan` as the default non-VR setting.
2. Validate `sample1.imm` against the OpenGL reference path.
3. Confirm:
   - Brightness matches the OpenGL reference.
   - Image orientation is correct.
   - FPS is not limited by CPU readback.
   - OIT/MSAA sample count and resolve behavior are unchanged.
   - Resize/fullscreen behavior is stable.
4. Add or maintain a smoke script that can run without VR hardware.
5. Record known unsupported Vulkan features in logs with stable prefixes.
6. Keep an explicit OpenGL settings file or documented override for regression
   comparison and fallback.

### Android Non-VR

1. Build and run the Android non-VR Vulkan path with the default Gradle
   renderer selection.
2. Validate the same sample content used on Windows where practical.
3. Confirm:
   - Swapchain creation succeeds on target hardware.
   - Display color-space behavior is correct.
   - Touch/window lifecycle survives pause/resume.
   - GLES fallback remains available.
4. Continue converging Android on the shared standalone backend-selection model:
   - request the renderer through runtime settings or intent extras.
   - keep build flags for startup defaults and capability trimming.
   - move toward compiling Vulkan and GLES into the normal non-VR package where
     practical.
5. Keep GLES available as the runtime fallback for non-VR builds.
6. Keep Android VR defaulting to the current GLES/Oculus path until OpenXR
   Vulkan is ready.
7. Keep Android VR out of scope for this phase.

### Exit Criteria

Phase 1 is complete when Windows and Android non-VR Vulkan visibly render known
samples with acceptable appearance in the actual target presentation path,
independent of OpenXR, and Vulkan has been promoted to the runtime default for
non-VR standalone players with OpenGL/GLES fallback paths still available. Log
markers alone do not satisfy this exit criterion.

## Phase 2: OpenXR Skeleton

### Purpose

Create an OpenXR runtime path without immediately requiring Vulkan rendering.
The first target is a minimal, inspectable OpenXR loop that proves runtime
lifecycle and view acquisition.

### Work Items

1. Vendor or discover the native OpenXR SDK components needed by standalone
   builds:
   - `openxr.h`
   - platform headers such as `openxr_platform.h`
   - `openxr_loader.lib` or an explicit dynamic-loader path
   - runtime loader DLL discovery for local probes
2. Keep `code/appImmViewer/scripts/check-openxr-deps.ps1` passing before adding
   OpenXR code to the default standalone build.
3. Keep `code/appImmViewer/scripts/probe-openxr-runtime.ps1` as the
   non-rendering runtime probe. Its intended scope is loader, extension,
   instance, and system discovery; its current verified scope is loader and
   extension enumeration only.
4. Add an OpenXR backend next to the existing VR backend, not as a replacement
   on day one.
5. Initialize:
   - `xrCreateInstance`
   - system selection
   - view configuration
   - reference space
   - session lifecycle
6. Add basic frame loop handling:
   - `xrWaitFrame`
   - `xrBeginFrame`
   - view location
   - projection extraction
   - `xrEndFrame`
7. Add simple logging with a unique prefix for OpenXR startup and frame-state
   diagnostics.
8. Add runtime selection/configuration so OpenXR can be enabled explicitly.
9. Keep existing OpenVR/Oculus paths available until OpenXR reaches feature
   parity or a deliberate cutover decision is made.

### First Graphics Binding

Pick the first graphics binding based on the least risky validation path:

- If Windows OpenXR can be brought up fastest with DirectX, use DirectX for the
  first OpenXR lifecycle slice.
- If Android OpenXR is the priority and the available runtime expects Vulkan,
  make Android the first OpenXR/Vulkan slice but keep it narrow.

The decision should be made after checking target runtime support, not assumed
from renderer preference alone.

### Exit Criteria

Phase 2 is complete when the app can start an OpenXR session, acquire poses,
compute per-eye views, and submit a simple frame on one platform/runtime.
The current codebase has not reached Phase 2 completion on Windows or Android.
Windows can discover the loader and enumerate extensions but currently fails
`xrCreateInstance`; Android now has an explicit IMM OpenXR startup-probe APK
path, but current runtime launches are blocked before native startup by Quest
OS focus state.

## Phase 3: OpenXR Input and App Integration

### Work Items

1. Map existing controller concepts onto OpenXR action sets.
2. Add action bindings for the target controller profiles.
3. Feed poses, buttons, triggers, sticks, and haptics into the existing app
   input layer.
4. Preserve left-handed and haptics settings where possible.
5. Add file-based or accessible logging for platforms where console output is
   not reliable.
6. Validate UI interaction and playback controls in headset.

### Exit Criteria

Phase 3 is complete when the OpenXR path can drive the existing VR interaction
model well enough to play and inspect sample content.

## Phase 4: Vulkan/OpenXR Interop

### Windows

1. Query OpenXR Vulkan graphics requirements.
2. Ensure the Vulkan renderer creates or adopts a Vulkan instance/device
   compatible with the OpenXR runtime.
3. Add a typed Vulkan external texture wrapper for OpenXR swapchain images.
4. Render directly into `XrSwapchainImageVulkanKHR` images or into internal
   render targets followed by an explicit resolve/copy into the swapchain image.
5. Add image layout transitions required for color attachment rendering and XR
   submission.
6. Ensure synchronization is complete before `xrEndFrame`.
7. Validate color-space behavior with the same display-encoding rules used by
   the non-VR Vulkan path, adjusted for OpenXR runtime expectations.

### Android

1. Use the OpenXR Android loader/runtime path.
2. Query Android Vulkan requirements from OpenXR.
3. Ensure the renderer and OpenXR use compatible Vulkan instance/device state.
4. Validate lifecycle behavior:
   - Android app pause/resume
   - session loss
   - surface recreation
   - headset removal/reentry where applicable
5. Keep GLES/Oculus VR fallback until OpenXR Vulkan is stable enough to replace
   it.

### Render Target Choice

There are two viable approaches:

1. Render directly to OpenXR swapchain images.
2. Render to existing internal OIT/MSAA targets, then resolve/copy into OpenXR
   swapchain images.

The second approach is safer initially because the renderer's OIT/MSAA behavior
already depends on its internal target structure. It keeps the central
transparency implementation intact and limits XR-specific behavior to the final
handoff.

Direct-to-swapchain rendering can be considered later if profiling shows the
extra resolve/copy is a real bottleneck.

### Exit Criteria

Phase 4 is complete when Vulkan OpenXR can render stereo sample content with:

- Correct brightness.
- Correct orientation.
- Stable head tracking.
- Correct controller input.
- No change to OIT/MSAA appearance.
- Acceptable frame timing on target hardware.

## Phase 5: Cutover and Cleanup

1. Decide whether OpenXR replaces or coexists with existing OpenVR/Oculus paths.
2. Remove dead runtime-specific code only after the OpenXR path covers required
   devices and workflows.
3. Convert settings to make backend choices explicit and consistent across
   standalone platforms:
   - XR runtime: none, legacy, OpenXR.
   - Graphics API: OpenGL/GLES, DirectX, Vulkan.
   - Fallback policy: strict or automatic fallback.
4. Make build configuration describe compiled backend capabilities, not the
   ordinary runtime renderer choice.
5. Update documentation and sample configs so non-VR Vulkan is the default path
   on Windows and Android, while legacy renderer configs remain available for
   comparison and support.
6. Add CI/build coverage for enabled combinations.

## Validation Matrix

### Required Non-VR Checks

| Platform | Renderer | Sample | Expected Result |
| --- | --- | --- | --- |
| Windows | OpenGL | `sample1.imm` | Reference appearance |
| Windows | Vulkan | `sample1.imm` | Matches OpenGL appearance |
| Android | GLES | representative sample | Reference mobile appearance |
| Android | Vulkan | representative sample | Matches GLES within expected platform differences |

### Required XR Checks

| Platform | Runtime | Renderer | Expected Result |
| --- | --- | --- | --- |
| Windows | existing VR path | current renderer | Existing behavior preserved |
| Windows | OpenXR | first chosen binding | Session, poses, stereo submit |
| Windows | OpenXR | Vulkan | Correct stereo Vulkan output |
| Android | existing VR path | GLES | Existing behavior preserved |
| Android | OpenXR | Vulkan | Correct stereo Vulkan output |

## Current Validation Evidence

Last updated: 2026-06-05.

Local validation was refreshed on 2026-06-05 after the backend-selection and
OpenXR-probe changes.

Validated:

- Windows standalone non-VR Vulkan sample playback.
- Android standalone non-VR Vulkan sample playback in a compositor capture on
  Quest 3S.
- Android standalone non-VR GLES fallback build.
- Android legacy VR/Oculus build.

Not validated:

- Windows OpenXR instance creation, session creation, poses, or frame submit
  against a real OpenXR runtime.
- Android IMM OpenXR instance creation, session creation, poses, or frame
  submit.
- Vulkan/OpenXR swapchain interop on any platform.

Validation rule: builds, log markers, package installation, process state, and
OpenXR service discovery are diagnostic proxies only. A runtime path is not
validated until the expected IMM sample content is visibly present in the
headset or in a headset/compositor capture that clearly shows the same content.

### Completed Locally

- Windows Release standalone build passes. Refreshed on 2026-06-05 with:
  `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe code/projects/windows/imm.sln /t:appImmViewer /p:Configuration=Release /p:Platform=x64`.
- Windows Release Vulkan `sample1.imm` smoke passes:
  `code/appImmViewer/scripts/run-vulkan-sample1-smoke.ps1 -Configuration Release -DurationSeconds 60 -PresentSeconds 5 -KeepArtifacts`.
- The Vulkan smoke reports:
  - `Vulkan sample1 smoke passed`
  - `nonblack=921148`
  - `nearVisible=555162`
  - `maxRGB=255,255,255`
  - `Live present: sRGB GPU Vulkan presentation logged`
- Windows Release DirectX `sample1.imm` baseline capture still passes:
  `code/appImmViewer/scripts/capture_windows_directx_baseline.ps1 -Configuration Release -TimeoutSeconds 90`.
  It wrote:
  `build/baseline-captures/windows-directx-static.ppm`.
  The captured debug log reports `nonZero=921600`, `drawCalls=38`,
  `paintDrawCalls=37`, `pictureDrawCalls=1`, and
  `picture360EquirectDrawCalls=1`.
- Launching the Windows standalone player with the default `settings.json`
  selects Vulkan and non-VR according to `code/appImmViewer/exe/debug.txt`:
  - `Rendering Backened: Vulkan`
  - `XR Runtime: Legacy`
  - `Rendering in VR: no`
- Android non-VR default build passes:
  `./gradlew :appImmViewer:assembleDebug -PimmNonVr=ON -PimmBuildDir=build_vulkan`.
  The unified validation runner refreshed this on 2026-06-05 in
  `build/validation/vulkan-openxr-20260605-174447/android-vulkan-build.txt`;
  Gradle reported `BUILD SUCCESSFUL`.
- Android non-VR and VR builds now use separate manifests selected by
  `-PimmNonVr`. The non-VR manifest does not declare Oculus `vr_only`, the
  Oculus VR category, or required VR features. The VR manifest retains the
  Oculus/VrApi metadata for the legacy VR player.
  The unified validation runner checks this directly in
  `build/validation/vulkan-openxr-20260605-175506/android-manifest-split.txt`:
  the non-VR manifest is verified to omit `com.oculus.intent.category.VR`,
  `com.oculus.vr.application.mode`, `android.hardware.vr.headtracking`,
  `android.hardware.vr.high_performance`, and `android.software.vr.mode`; the
  VR manifest is verified to retain those VR/Oculus tokens.
- The unified validation runner now has a static renderer/XR config gate. The
  latest lightweight run recorded this in
  `build/validation/vulkan-openxr-20260605-175952/static-renderer-xr-config.txt`.
  It verifies:
  - `settings.json`: `RenderingAPI: Vulkan`, `XRRuntime: Legacy`,
    `EnableVR: false`.
  - `settings-vulkan-smoke.json`: `RenderingAPI: Vulkan`,
    `XRRuntime: Legacy`, `EnableVR: false`.
  - `settings-openxr-probe.json`: `RenderingAPI: Vulkan`,
    `XRRuntime: OpenXR`, `EnableVR: true`.
  - Android Gradle defaults: `immNonVr` defaults to `ON`; non-VR defaults to
    Vulkan; VR defaults to GLES; CMake receives `IMM_ANDROID_NON_VR` and
    `IMM_ANDROID_RENDERER_API`; manifest selection uses `src/nonVr` for
    `immNonVr=ON` and `src/vr` otherwise.
- The generated Android Gradle/CMake build model for that build contains:
  `-DIMM_ANDROID_NON_VR=ON` and `-DIMM_ANDROID_RENDERER_API=Vulkan`.
- The native compile commands for the Android non-VR Vulkan build contain:
  `-DIMM_ANDROID_RENDERER_VULKAN=1`.
- Android non-VR GLES fallback build passes:
  `./gradlew :appImmViewer:assembleDebug -PimmNonVr=ON -PimmRendererApi=GLES -PimmBuildDir=build_gles_fallback`.
  Refreshed on 2026-06-05 by the unified validation runner in
  `build/validation/vulkan-openxr-20260605-174447/android-gles-build.txt`;
  Gradle reported `BUILD SUCCESSFUL`.
- Android non-VR GLES fallback runtime visual regression is not currently
  validated after the latest changes. A refreshed attempt built and installed
  the GLES fallback APK, selected `IMM Android renderer API: GLES`, and began
  loading `sample1.imm`, but Android sent pause/term-window commands almost
  immediately. The resulting compositor capture was black:
  `code/projects/android/logs/android-gles-regression-capture1/imm_gles_regression_vw.png`.
  Its log and window dump are:
  `code/projects/android/logs/android-gles-regression-capture1/logcat.txt` and
  `code/projects/android/logs/android-gles-regression-capture1/volumetric_window.txt`.
  A second refreshed run produced the same black 4078-byte capture:
  `code/projects/android/logs/android-gles-regression-capture2/imm_gles_regression_vw.png`.
  That run reached `IMM Android renderer API: GLES`, called `loadPath` for
  `sample1.imm`, and returned `IMMAVAL loadPath result=1`, but the activity was
  paused at `2026-06-05 17:04:15.877`, received `APP_CMD_INIT_WINDOW` at
  `17:04:15.898`, then received `NativeWindowDestroyed` at `17:04:15.905` and
  `APP_CMD_TERM_WINDOW` at `17:04:15.966`. No `Loaded in CPU` or `Loaded in GPU`
  marker appeared in that run. Its window dump shows the IMM panel as visible
  but not focused/available, while the successful Android Vulkan run remained
  focused and visible through CPU load, GPU load, draw submission, and present.
  A repeatable smoke wrapper was added at
  `code/projects/android/run-android-renderer-smoke.ps1`, with GLES and Vulkan
  wrappers at `run-android-gles-smoke.ps1` and `run-android-vulkan-smoke.ps1`.
  A clean GLES rerun through that wrapper on 2026-06-05 selected GLES and
  initialized Adreno OpenGL ES, but still failed before `Loaded in CPU`:
  `code/projects/android/logs/android-gles-smoke-current2/logcat.txt`.
  The clean run shows `onPause` at `17:10:06.710`, `APP_CMD_INIT_WINDOW` at
  `17:10:06.726`, `NativeWindowDestroyed` at `17:10:06.739`, `IMM Android
  renderer API: GLES` at `17:10:06.740`, `IMMAVAL loadPath result=1` at
  `17:10:06.807`, and `APP_CMD_TERM_WINDOW` at `17:10:06.807`. The process was
  still alive afterward, but activity state included the IMM activity as
  stopped/last-paused rather than a stable rendering window. This is current
  evidence of an Android non-VR GLES runtime lifecycle regression.
  The same wrapper was then run against the existing Android Vulkan APK:
  `code/projects/android/logs/android-vulkan-smoke-wrapper-check/logcat.txt`.
  That current Vulkan rerun selected Vulkan, created the Android Vulkan surface,
  initialized the owned Vulkan device, and returned `IMMAVAL loadPath result=1`,
  but it also received `onPause`, `NativeWindowDestroyed`, and
  `APP_CMD_TERM_WINDOW` before any `Loaded in CPU` marker. Therefore the latest
  automated Android reruns point to a non-VR Android launch/window lifecycle
  issue affecting both renderer selections in the current device/session state.
  They do not invalidate the earlier user-confirmed Android Vulkan visual
  capture, but they do leave Android non-VR automated smoke in a failed state.
  The wrapper was adjusted so the default launch matches the manual
  `adb shell am start -n org.linuxfoundation.imm.player/.MainActivity` path and
  only sends the `RenderingAPI` intent extra when `-UseIntentRendererExtra` is
  requested. A no-extra Vulkan rerun still failed before CPU load:
  `code/projects/android/logs/android-vulkan-smoke-no-extra/logcat.txt`. Its
  activity dump showed `isSleeping=true`, `mCurrentFocus=null`, and the display
  off, while the earlier successful Vulkan capture had `isSleeping=false`.
  An adb wake attempt changed power wakefulness to awake, but the next launch
  was blocked by Quest OS state instead of reaching IMM startup:
  `code/projects/android/logs/android-vulkan-smoke-adb-wakeup/logcat.txt`
  contains `Launch is blocked because: a Reprojected OS dialog is currently
  showing`, and the activity dump shows VR lockscreen/Guardian focus. The smoke
  script now records `power_before.txt`, `activity_before.txt`,
  `power_after.txt`, and `activity_after.txt`, and reports this Quest
  lockscreen/Guardian/reprojected-dialog condition explicitly. Verified blocker
  message:
  `code/projects/android/logs/android-vulkan-smoke-blocker-check/logcat.txt`.
  A fresh check at 2026-06-05 17:23 showed the Quest attached over ADB and
  awake, but `dumpsys activity activities` still had
  `com.oculus.os.vrlockscreen/.SensorLockActivity` and
  `com.oculus.guardian/...GuardianDialogActivity` resumed. Current Vulkan and
  GLES wrapper runs therefore stop with the explicit Quest OS focus blocker
  before treating missing renderer markers as a renderer failure:
  `code/projects/android/logs/android-vulkan-smoke-current-blocked/logcat.txt`
  and
  `code/projects/android/logs/android-gles-smoke-current-blocked/logcat.txt`.
  This is a regression gap to rerun after the headset is unlocked and Guardian
  dialogs are cleared; it is not evidence that GLES fallback runtime rendering
  passed, and it is not evidence that the current Vulkan renderer regressed.
- Android legacy VR/Oculus build still passes:
  `./gradlew :appImmViewer:assembleDebug -PimmNonVr=OFF -PimmBuildDir=build_vr`.
  Refreshed on 2026-06-05 by the unified validation runner in
  `build/validation/vulkan-openxr-20260605-174447/android-legacy-vr-build.txt`;
  Gradle reported `BUILD SUCCESSFUL`.
- An earlier Android non-VR Vulkan runtime marker smoke passed on the attached
  Quest 3S after uninstalling the pre-existing differently signed
  `org.linuxfoundation.imm.player` package:
  `code/projects/android/run-android-vulkan-smoke.ps1 -Adb <Unity adb>`.
  The smoke installs `appImmViewer-debug.apk`, launches the player, captures
  `logs/android-vulkan-smoke/logcat.txt`, and requires these markers:
  - `IMM Android renderer API: Vulkan`
  - `Vulkan renderer created Android surface`
  - `Vulkan renderer initialized with owned device`
  - `Loaded in CPU`
  - `Loaded in GPU`
  - `Vulkan renderer submitted picture draw commands`
  - `Vulkan renderer submitted static paint draw commands`
  This smoke is not visual validation by itself, and current automated Android
  reruns are blocked by Quest display/OS focus state as documented above.
  Earlier Quest screencaps while
  `org.linuxfoundation.imm.player` was running produced:
  - `quest_imm_player_screencap.png`: dark stereo view, no recognizable
    `sample1.imm` content.
  - `quest_imm_player_screencap_after_permissions.png`: dark stereo view after
    storage permissions were granted, no recognizable `sample1.imm` content.
  - `quest_imm_player_screencap_nonvr_manifest.png`: passthrough/room view
    after removing VR-only manifest metadata from the non-VR build, no
    recognizable `sample1.imm` content.
  - `quest_imm_player_screencap_opaque_window.png`: passthrough/room view after
    requesting an opaque `WINDOW_FORMAT_RGBX_8888` Android native window for
    Vulkan, no recognizable `sample1.imm` content.
  - `quest_imm_player_screencap_gles_control.png`: GLES non-VR control run with
    the same non-VR manifest, also passthrough/room view with no recognizable
    `sample1.imm` content.
  - `quest_imm_player_screencap_user_visible_random_shapes.png`: headset
    capture taken after the user reported that the headset-visible app content
    was random colored shapes rather than `sample1.imm`.
  - `quest_imm_vw_capture_user_visible_random_shapes.png`: compositor
    volumetric-window capture of the IMM 2D panel showing blocky
    high-saturation geometry, not the expected `sample1.imm` view.
  Android activity/window dumps showed `org.linuxfoundation.imm.player`
  resumed, focused, and hosted as a 1280x800 2D panel. SurfaceFlinger showed an
  XR quad layer for the IMM panel with frames latching, and app logs showed
  `sample1.imm` loading plus Vulkan/GLES draw/present markers. Those were
  diagnostic proxies only and did not validate Vulkan visible content.
- A later GLES non-VR control capture using the same Android app, manifest,
  sample, spawn, and compositor-capture path produced correct `sample1.imm`
  content:
  `code/projects/android/logs/android-gles-visual-control/imm_gles_control_vw.png`.
  That earlier control validated the Android non-VR app setup and compositor
  capture path at that point in the investigation. It is not a current GLES
  runtime pass after the Vulkan fixes; the latest GLES fallback runtime attempts
  are blocked by the pause/term-window lifecycle described above.
- Android Vulkan diagnostic captures after fixing the Android `Settings`
  lifetime and aligning the non-VR render target MSAA count with the existing
  8-sample OIT shader contract still showed incorrect blocky colored geometry:
  - `code/projects/android/logs/android-vulkan-visual-fixed-settings-2/imm_fixed_settings_vw.png`
  - `code/projects/android/logs/android-vulkan-visual-direct-blit-rebuilt/imm_direct_blit_rebuilt_vw.png`
  - `code/projects/android/logs/android-vulkan-visual-msaa8-direct-blit/imm_msaa8_direct_blit_vw.png`
  The direct-blit variants were diagnostic only and did not validate Vulkan
  content.
- Android Vulkan visible-content validation now passes on Quest 3S after fixing
  the Android packed static-paint vertex shader variants, Vulkan static/picture
  descriptor bindings, and Android Vulkan swapchain pre-transform selection.
  The current validated compositor capture is:
  `code/projects/android/logs/android-vulkan-swapchain-identity-capture2/imm_swapchain_identity_vw.png`.
  That capture uses the IMM app volumetric-window token and clearly shows
  `sample1.imm` content: the robot is upright on the branch with the same
  expected composition as the GLES control capture. The user visually confirmed
  this capture as correct. Its log evidence is:
  `code/projects/android/logs/android-vulkan-swapchain-identity-capture2/logcat.txt`.
  The log includes:
  - `IMMAVAL swapchain transform current=8 supported=0x1ff selected=1`
  - `Loaded in CPU`
  - `Loaded in GPU`
  - `Vulkan renderer submitted static paint draw commands`
  - `Vulkan renderer submitted picture draw commands`
  - `IMMAVAL present select direct=1 srgb=1 needsResolve=1 sampleCount=8
    resolveImage=1 resolveView=1 format=122 size=1280x800 gpuPaintDraws=37`
  The app was force-stopped after the capture and
  `code/projects/android/logs/android-vulkan-swapchain-identity-capture2/pidof_after.txt`
  is empty.
- Android OpenXR was not validated. Only OS/component discovery was performed
  on an attached Quest 3S using Unity-bundled adb:
  `C:\Program Files\Unity\Hub\Editor\2022.3.62f2\Editor\Data\PlaybackEngines\AndroidPlayer\SDK\platform-tools\adb.exe`.
  The device reports that some OpenXR-related system components exist and are
  running:
  - model: `Quest 3S`
  - manufacturer: `Oculus`
  - Android SDK: `34`
  - OpenXR broker package: `horizonos.openxr.runtimebroker`
  - broker version: `versionCode=34`, `versionName=14`
  - broker ABI: `arm64-v8a`
  - services/processes: `xrservice`, `xrspd`, and
    `horizonos.openxr.runtimebroker` are running.
  - runtime properties: `init.svc.xrservice=running`,
    `init.svc.xrspd=running`, `ovr.xrspd.status=4`,
    `ovr.xrspd.ffs.ready=1`.
  - recent `logcat` includes device OpenXR activity from system components such
    as `OpenXR_ClientState` and `OpenXR_Anchor`.
  This does not prove that an application can create an OpenXR instance,
  create a session, acquire views, or submit frames on this device.
- The unified validation runner now records the Android OpenXR component
  discovery as its own gate, independent of Android app launch/focus state.
  The latest run passed this gate in
  `build/validation/vulkan-openxr-20260605-182052/android-openxr-runtime-components.txt`.
  It found the Quest 3S over ADB, package
  `horizonos.openxr.runtimebroker`, `versionCode=34`, `versionName=14`,
  `primaryCpuAbi=arm64-v8a`, `xrservice` running, `xrspd` running,
  `ovr.xrspd.status=4`, `ovr.xrspd.ffs.ready=1`, and a running
  `horizonos.openxr.runtimebroker` process. This is component-presence
  evidence only, not IMM OpenXR session or frame-submit validation.
- An explicit Android OpenXR startup-probe APK path now exists. It is selected
  with `-PimmNonVr=ON -PimmRendererApi=Vulkan -PimmXrRuntime=OpenXR` and
  compiles `IMM_ANDROID_XR_RUNTIME_OPENXR` into the non-VR NativeActivity path.
  The native probe in `code/appImmViewer/src/android/cpp/NonVrApp.cpp` uses
  the Android activity VM/object to call `xrInitializeLoaderKHR`, enumerates
  instance extensions, creates an OpenXR instance with
  `XR_KHR_android_create_instance`, queries the HMD system and primary stereo
  view configuration, and tears the instance down. It logs with the stable
  prefix `IMM_ANDROID_OPENXR_PROBE`.
- The explicit Android OpenXR probe APK build passed on 2026-06-05. The
  unified validation runner recorded this in
  `build/validation/vulkan-openxr-20260605-182052/android-openxr-probe-build.txt`;
  Gradle reported `BUILD SUCCESSFUL in 15s`. The same run also rebuilt the
  Android Vulkan non-VR, GLES non-VR fallback, and legacy VR/Oculus APK
  variants successfully.
- `code/projects/android/run-android-openxr-probe-smoke.ps1` now installs and
  launches that probe APK, requires the `IMM_ANDROID_OPENXR_PROBE` loader,
  instance, HMD-system, stereo-view, and teardown markers, and classifies Quest
  OS focus blockers before marker checks. A current launch attempt at
  `code/projects/android/logs/android-openxr-probe-current-blocker/` installed
  the APK and issued `am start`, but Android did not reach native startup. The
  captured log reports `Launch is blocked because: a Reprojected OS dialog is
  currently showing`, and the activity dump shows
  `com.oculus.os.vrlockscreen/.SensorLockActivity` plus
  `com.oculus.guardian/com.oculus.vrguardianservice.guardiandialog.GuardianDialogActivity`
  resumed. This is a headset OS focus blocker, not an OpenXR probe result.
- A later attempted launch after an ADB wake/menu/home sequence is recorded in
  `code/projects/android/logs/android-openxr-probe-adb-wake-attempt-20260605-1836/`.
  The APK install succeeded, but the probe still did not reach native
  `IMM_ANDROID_OPENXR_PROBE` startup. `power_after.txt` shows the headset was
  awake (`mWakefulness=Awake`, `mHalInteractiveModeEnabled=true`,
  `mHoldingDisplaySuspendBlocker=true`), while `activity_after.txt` shows
  `com.oculus.guardian/...GuardianDialogActivity` resumed and
  `com.oculus.os.vrlockscreen/.SensorLockActivity` owning current focus. This
  narrows the Android runtime blocker: ADB can wake the device, but cannot clear
  the VR lockscreen/Guardian focus state needed for app launch.
- Android now has a second OpenXR startup-probe APK shape for Quest launch
  policy testing. `-PimmNonVr=ON -PimmRendererApi=Vulkan
  -PimmXrRuntime=OpenXR -PimmManifest=vr` builds the lightweight
  `NonVrApp.cpp` OpenXR startup probe while selecting the VR manifest with
  `com.oculus.intent.category.VR` and `vr_only` metadata. The unified runner
  recorded the build in
  `build/validation/vulkan-openxr-20260605-184554/android-openxr-vrmanifest-probe-build.txt`,
  and Gradle reported `BUILD SUCCESSFUL`. The same report includes
  `android-openxr-vrmanifest-probe-manifest.txt`, which verifies the merged APK
  manifest contains `com.oculus.intent.category.VR`,
  `com.oculus.vr.application.mode`, `vr_only`,
  `android.hardware.vr.headtracking`, `android.hardware.vr.high_performance`,
  `android.software.vr.mode`, `android.app.lib_name`, and
  `org.linuxfoundation.imm.player.MainActivity`.
- The VR-manifest OpenXR probe APK was also installed and launched directly;
  evidence is in
  `code/projects/android/logs/android-openxr-probe-vrmanifest-attempt-20260605-1842/`.
  The APK install succeeded, but the probe still did not reach
  `IMM_ANDROID_OPENXR_PROBE`. `activity_after.txt` shows
  `com.oculus.vrshell/.systemdialog.launchcheck.LaunchCheckControllerRequiredDialogActivity`
  resumed, `com.oculus.guardian/...GuardianDialogActivity` present, and
  `com.oculus.os.vrlockscreen/.SensorLockActivity` owning current focus. This
  proves the blocker is not only the original non-VR manifest; the headset also
  needs the lockscreen/Guardian/controller-required launch state cleared.
- Direct shell queries to the Android OpenXR runtime broker content providers:
  - `content://org.khronos.openxr.runtime_broker/openxr/1/abi/arm64-v8a/runtimes/active/0`
  - `content://org.khronos.openxr.system_runtime_broker/openxr/1/abi/arm64-v8a/runtimes/active/0`
  returned `No result found` from `adb shell content query`. Treat this as a
  shell-context discovery result only; a real OpenXR app should use the Android
  loader path and app/activity context.
- There is now an IMM Android OpenXR startup-probe test APK and smoke script,
  but its runtime markers have not yet executed because the current headset OS
  state blocks app launch before native startup. The current Android VR player
  remains the Oculus/VRAPI/GLES path, and the validated Android Vulkan sample
  APK is still non-VR.
- Standalone settings parse and log `XRRuntime`.
- Launching the Windows standalone player with
  `code/appImmViewer/exe/settings-openxr-probe.json` logs:
  - `XR Runtime: OpenXR`
  - `Rendering in VR: yes`
  - `IMM_OPENXR_STANDALONE loader=C:\Program Files
    (x86)\Steam\steamapps\common\SteamVR\bin\win64\openxr_loader.dll`
  - `IMM_OPENXR_STANDALONE enumerateExtensionsResult=0 count=48`
  - `IMM_OPENXR_STANDALONE enumerateExtensionsFillResult=0 count=48`
  - extensions including `XR_KHR_vulkan_enable` and
    `XR_KHR_vulkan_enable2`
  - `IMM_OPENXR_STANDALONE createInstanceResult=-2 resultName=XR_ERROR_RUNTIME_FAILURE instance=0`
  - `OpenXR standalone startup probe failed; the OpenXR VR backend is not
    implemented yet`
- The explicit OpenXR standalone setting therefore fails at the intended
  startup boundary instead of silently using the legacy VR backend. It currently
  reaches the active runtime and enumerates instance extensions, but it does not
  currently create an OpenXR instance through the SteamVR runtime.
- Windows has an active OpenXR runtime registered:
  `C:\Program Files (x86)\Steam\steamapps\common\SteamVR\steamxr_win64.json`.
- The active runtime provides a loader DLL:
  `C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\openxr_loader.dll`.
- `code/projects/windows/bootstrap-openxr-sdk.ps1` cloned the pinned Khronos
  `OpenXR-SDK` tag `release-1.1.60` into `thirdparty/openxr-sdk`.
- `code/appImmViewer/scripts/check-openxr-deps.ps1` now passes using
  `thirdparty/openxr-sdk\include\openxr\openxr.h` and the SteamVR loader DLL.
  Refreshed on 2026-06-05 after the Vulkan milestone validation. The script now
  treats the loader DLL as required and `openxr_loader.lib` as optional for the
  dynamic-loader path.
- The OpenXR dependency path is ready for a dynamic-loader skeleton that does
  not link against `openxr_loader.lib`.
- A hardware-independent OpenXR validation seam now exists. The Windows
  standalone app-side probe accepts `IMM_OPENXR_LOADER_DLL`, allowing tests to
  load a controlled DLL instead of the active system runtime. The fake loader
  source is `code/appImmViewer/scripts/fake_openxr_loader.cpp`, built by
  `code/appImmViewer/scripts/build-fake-openxr-loader.ps1` into
  `build/openxr-fake-loader/openxr_loader.dll`.
- The fake-loader path was validated on 2026-06-05 after rebuilding the Windows
  Release viewer. `code/appImmViewer/scripts/probe-openxr-runtime.ps1
  -LoaderDll build/openxr-fake-loader/openxr_loader.dll` completed:
  `xrCreateInstance`, `xrGetSystem`, `xrGetSystemProperties`,
  `xrEnumerateViewConfigurations`, two primary stereo views, and
  `xrDestroyInstance`, all with `XR_SUCCESS`. The fake runtime reports Vulkan
  extensions `XR_KHR_vulkan_enable` and `XR_KHR_vulkan_enable2`.
- The rebuilt app-side probe was also run with
  `IMM_OPENXR_LOADER_DLL=build/openxr-fake-loader/openxr_loader.dll` and
  `settings-openxr-probe.json`. It selected Vulkan, requested
  `XRRuntime: OpenXR`, loaded the fake loader, logged
  `IMM_OPENXR_STANDALONE createInstanceResult=0 resultName=XR_SUCCESS`, found
  system id `66`, queried Vulkan instance extension requirements, queried
  Vulkan device extension requirements, queried Vulkan graphics API version
  requirements, enumerated two stereo views, created a no-render session,
  enumerated one fake swapchain format, created a fake Vulkan swapchain,
  enumerated a fake `XrSwapchainImageVulkanKHR` image handle
  `0x13579bdf`, began the session, completed one
  `xrWaitFrame`/`xrBeginFrame` loop, acquired/waited/released the fake
  swapchain image, called `xrEndFrame` with one projection layer containing two
  views, ended and destroyed the session, destroyed the instance, and then
  stopped at the expected boundary:
  `OpenXR standalone startup probe passed; the OpenXR VR backend is not
  implemented yet`.
- The fake-loader OpenXR/Vulkan interop boundary was strengthened on
  2026-06-05. The latest app-side fake-loader evidence is
  `build/validation/vulkan-openxr-20260605-181407/windows-openxr-fake-loader-app-probe.txt`.
  It records:
  - `getVulkanInstanceExtensionsProcResult=0 resultName=XR_SUCCESS available=1`
  - `vulkanInstanceExtensions=VK_KHR_surface VK_KHR_win32_surface`
  - `getVulkanDeviceExtensionsProcResult=0 resultName=XR_SUCCESS available=1`
  - `vulkanDeviceExtensions=VK_KHR_swapchain`
  - `getVulkanGraphicsRequirementsProcResult=0 resultName=XR_SUCCESS available=1`
  - `getVulkanGraphicsRequirementsResult=0 resultName=XR_SUCCESS minApi=0x400000 maxApi=0x403000`
  - `vulkanGraphicsBinding type=1000025000 instance=0x1111222233334444 physicalDevice=0x2222333344445555 device=0x3333444455556666 queueFamily=7 queueIndex=1`
  - `createSessionResult=0 resultName=XR_SUCCESS`
  These calls are obtained through `xrGetInstanceProcAddr`, matching the
  OpenXR extension loading model. This proves the app-side startup boundary now
  asks the runtime for the Vulkan requirements it must satisfy before real
  Vulkan/OpenXR swapchain interop can be correct, and it passes a typed
  `XrGraphicsBindingVulkanKHR`-shaped structure into session creation. The fake
  loader now validates those deterministic Vulkan handles before allowing
  `xrCreateSession` to succeed. It still does not prove a real runtime graphics
  binding or real compositor swapchain submission.
- The Vulkan renderer external-image boundary is now covered by validation.
  `build/validation/vulkan-openxr-20260605-182820/vulkan-external-image-frame-boundary.txt`
  records that `piRendererVulkan::BeginExternalImageFrame` now has an
  OpenXR-shaped overload taking a Vulkan image, explicit Vulkan format, width,
  height, and array-layer count. That path creates and owns a Vulkan image view
  internally, wraps the external color image with
  `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`, creates an internal
  `imm_external_image_depth` depth texture, builds a render target around the
  external color image, and tears the frame down without destroying externally
  owned Vulkan images. Renderer-created image views on external images are
  still destroyed by the renderer. The older image-plus-image-view overload is
  preserved for callers that already own a suitable view. The same gate confirms
  the legacy
  `CreateTextureFromID(unsigned int)` path remains explicitly unsupported, so
  OpenXR swapchain work stays on the typed Vulkan image/image-view boundary
  rather than the old integer texture-id API. This still does not validate
  rendering into a real `XrSwapchainImageVulkanKHR`; it protects the renderer
  handoff surface needed for that work.
- The fake OpenXR/Vulkan swapchain probe now validates and records the renderer
  handoff tuple. In
  `build/validation/vulkan-openxr-20260605-183543/windows-openxr-fake-loader-app-probe.txt`,
  the fake loader accepts `xrCreateSwapchain` only when the app requests color
  attachment usage, format `44`, `1600x1600`, sample count `1`, face count `1`,
  array size `2`, and mip count `1`. The app then enumerates the fake
  `XrSwapchainImageVulkanKHR` image and logs
  `rendererExternalImageFrameCandidate image=0x13579bdf vkFormat=44 width=1600 height=1600 arrayLayers=2`.
  That is the exact data shape consumed by the OpenXR-shaped
  `BeginExternalImageFrame(image, vkFormat, width, height, arrayLayers)` renderer
  entry point. This is still controlled fake-runtime evidence, not a real
  compositor image or real frame submission.
- `build/validation/vulkan-openxr-20260605-183543/windows-openxr-fake-loader-swapchain-contract.txt`
  adds a runtime negative/positive contract check for the fake loader. It calls
  `xrCreateSwapchain` directly, verifies a wrong format returns
  `XR_ERROR_VALIDATION_FAILURE` (`badFormatResult=-1`), verifies a wrong array
  size returns `XR_ERROR_VALIDATION_FAILURE` (`badArraySizeResult=-1`), and
  verifies the valid contract returns `XR_SUCCESS` with fake swapchain handle
  `0x2468ace0`.
- The validation runner has these useful current reports:
  - `build/validation/vulkan-openxr-20260605-174244/validation-status.txt`
    includes Windows Vulkan non-VR smoke, Windows DirectX baseline, fake-loader
    build/probes, and the real Windows OpenXR runtime blocker.
  - `build/validation/vulkan-openxr-20260605-174447/validation-status.txt`
    adds Android Vulkan non-VR, GLES non-VR fallback, and legacy VR/Oculus APK
    build gates. That run skipped Android runtime because the Quest OS focus
    blocker is still external to the APK build.
  - `build/validation/vulkan-openxr-20260605-175203/validation-status.txt`
    adds lightweight hardware-state probes without APK install/launch and uses
    child PowerShell process capture so the standalone OpenXR probe evidence
    files contain the actual probe logs. The runner also resets
    `$LASTEXITCODE` per captured check so native command results do not leak
    between gates. It classifies Windows SteamVR HMD state as blocked and
    Android Quest OS state as blocked while keeping the fake-loader probes
    passing.
  - `build/validation/vulkan-openxr-20260605-175506/validation-status.txt`
    adds the Android manifest split gate. It passes the non-VR/VR manifest
    checks while preserving the same real-runtime Windows and Quest OS blockers.
  - `build/validation/vulkan-openxr-20260605-175821/validation-status.txt`
    adds the static renderer/XR config gate and refreshes the real-runtime
    blocker evidence. It passes Windows OpenXR dependency discovery, static
    Windows/Android renderer/XR config, fake-loader build/probes, app-side
    fake OpenXR session/frame/swapchain ordering, and Android manifest split.
    It still classifies the real Windows OpenXR runtime and Quest OS state as
    blocked.
  - `build/validation/vulkan-openxr-20260605-175952/validation-status.txt`
    adds Android OpenXR runtime component discovery. It passes the same static,
    fake-loader, app-side fake OpenXR, Android OpenXR component, and Android
    manifest checks while keeping the real Windows OpenXR runtime and Quest OS
    focus state classified as blocked.
  - `build/validation/vulkan-openxr-20260605-180444/validation-status.txt`
    adds the Android OpenXR probe APK build gate. It passes Windows OpenXR
    dependency discovery, static Windows/Android renderer/XR config, fake
    OpenXR loader probes, app-side fake OpenXR session/frame/swapchain
    ordering, Android OpenXR component discovery, Android manifest split,
    Android Vulkan non-VR build, Android GLES fallback build, Android legacy
    VR/Oculus build, and Android OpenXR probe APK build. Runtime launches were
    skipped in that report; a separate direct OpenXR probe smoke attempt is
    recorded under `code/projects/android/logs/android-openxr-probe-current-blocker/`.
  - `build/validation/vulkan-openxr-20260605-181137/validation-status.txt`
    adds stricter Windows app-side fake-loader validation for OpenXR/Vulkan
    requirements. It passes the fake-loader build/probes and requires
    `xrGetInstanceProcAddr`-loaded Vulkan instance extensions, device
    extensions, and graphics requirements before fake session and swapchain
    creation. The same report refreshes the Windows no-HMD blocker and the
    Quest lockscreen/Guardian blocker.
  - `build/validation/vulkan-openxr-20260605-181407/validation-status.txt`
    further tightens the fake interop gate by requiring an
    `XrGraphicsBindingVulkanKHR`-shaped session-create `next` chain before fake
    session creation. The fake loader validates deterministic Vulkan instance,
    physical-device, device, queue-family, and queue-index values.
  - `build/validation/vulkan-openxr-20260605-181842/validation-status.txt`
    adds the Vulkan renderer external-image frame boundary gate to the broader
    fake-loader/status pass. It passes Windows OpenXR dependency discovery,
    static renderer/XR config, Vulkan external-image boundary validation, fake
    OpenXR loader build/probes, app-side fake OpenXR/Vulkan session and
    swapchain ordering, Android OpenXR component discovery, and Android
    manifest split. It still classifies the real Windows OpenXR runtime,
    SteamVR HMD state, and Quest OS focus state as blocked.
  - `build/validation/vulkan-openxr-20260605-182052/validation-status.txt`
    refreshes the Android build gates after the external-image validation
    change. It passes Windows OpenXR dependency discovery, static renderer/XR
    config, Vulkan external-image boundary validation, fake OpenXR loader
    build/probes, app-side fake OpenXR/Vulkan session and swapchain ordering,
    real Windows/Quest blocker classification, Android OpenXR component
    discovery, Android manifest split, Android Vulkan non-VR build, Android
    GLES fallback build, Android legacy VR/Oculus build, and Android OpenXR
    probe APK build.
  - `build/validation/vulkan-openxr-20260605-182346/validation-status.txt`
    refreshes the local Windows visual evidence after fixing child-process
    output capture for the Windows smoke scripts. It passes Windows Vulkan
    `sample1.imm` non-VR smoke with
    `Pixels: nonblack=921148 nearVisible=555162 maxRGB=255,255,255` and
    `Live present: sRGB GPU Vulkan presentation logged`, and passes the
    Windows DirectX baseline capture with `IMM GL validation` reporting
    `nonZero=921600`, `drawCalls=38`, `paintDrawCalls=37`, and
    `pictureDrawCalls=1`.
  - `build/validation/vulkan-openxr-20260605-182820/validation-status.txt`
    validates the OpenXR-shaped Vulkan renderer external-image boundary after
    adding the image-only overload. It passes Windows OpenXR dependency
    discovery, static renderer/XR config, Vulkan external-image boundary
    validation, fake OpenXR loader build/probes, app-side fake OpenXR/Vulkan
    requirement and swapchain ordering, Android OpenXR component discovery, and
    Android manifest split. It still classifies real Windows OpenXR, SteamVR
    HMD state, and Quest OS focus state as blocked.
  - `build/validation/vulkan-openxr-20260605-182846/validation-status.txt`
    refreshes Android build coverage after the renderer API change. Android
    Vulkan non-VR, GLES fallback, legacy VR/Oculus, and OpenXR probe APK builds
    all report `BUILD SUCCESSFUL`.
  - `build/validation/vulkan-openxr-20260605-183328/validation-status.txt`
    strengthens the fake OpenXR/Vulkan swapchain gate. It passes Windows OpenXR
    dependency discovery, static renderer/XR config, the OpenXR-shaped Vulkan
    external-image boundary, fake loader build/probes, app-side Vulkan
    requirement queries, fake `XrGraphicsBindingVulkanKHR` session creation,
    strict fake swapchain creation, fake `XrSwapchainImageVulkanKHR`
    enumeration, renderer external-image-frame candidate logging, Android
    OpenXR component discovery, and Android manifest split. It still classifies
    real Windows OpenXR, SteamVR HMD state, and Quest OS state as blocked.
  - `build/validation/vulkan-openxr-20260605-183543/validation-status.txt`
    adds the direct fake-loader swapchain contract test. It passes the static
    OpenXR-shaped renderer boundary check, fake loader build, standalone fake
    OpenXR probe, direct negative/positive `xrCreateSwapchain` contract test,
    and app-side fake OpenXR/Vulkan renderer-handoff candidate probe.
  - `build/validation/vulkan-openxr-20260605-184042/validation-status.txt`
    adds the Android OpenXR VR-manifest probe APK build. It passes static
    renderer/XR config, Vulkan external-image boundary validation, Android
    Vulkan non-VR build, Android GLES fallback build, Android legacy VR/Oculus
    build, Android OpenXR non-VR-manifest probe build, and Android OpenXR
    VR-manifest probe build.
  - `build/validation/vulkan-openxr-20260605-184401/validation-status.txt`
    verifies the unified runner now reports both OpenXR runtime smoke variants:
    the original OpenXR probe runtime smoke and the OpenXR VR-manifest probe
    runtime smoke.
  - `build/validation/vulkan-openxr-20260605-184554/validation-status.txt`
    adds merged-manifest verification for the Android OpenXR VR-manifest probe
    APK. It passes the VR-manifest probe build and confirms the generated
    manifest contains the VR category, Oculus VR mode metadata, VR hardware
    feature declarations, native library metadata, and `MainActivity`.
  - `build/validation/vulkan-openxr-20260605-184954/validation-status.txt`
    adds an alternate Windows real-runtime probe using
    `XR_RUNTIME_JSON=C:\Program Files\Meta Horizon\Support\oculus-runtime\oculus_openxr_64.json`
    without changing the active OpenXR runtime registry. The active SteamVR
    runtime still fails `xrCreateInstance` with `XR_ERROR_RUNTIME_FAILURE`, but
    the Meta runtime override enumerates 99 extensions, creates an OpenXR
    instance successfully, reports no HMD system with
    `getHmdSystemResult=-35 systemId=0`, and destroys the instance cleanly.
  - `build/validation/vulkan-openxr-20260605-185433/validation-status.txt`
    refreshes the focused OpenXR evidence after the result-name logging update.
    The fake-loader/app-side OpenXR Vulkan handoff probe still passes, active
    SteamVR still fails with `XR_ERROR_RUNTIME_FAILURE`, and the Meta runtime
    override now records the no-HMD result as
    `getHmdSystemResult=-35 resultName=XR_ERROR_FORM_FACTOR_UNAVAILABLE
    systemId=0`.
  - `logs/android-openxr-vrmanifest-handtracking-attempt-20260605-1900/`
    tested the VR-manifest OpenXR probe after adding Quest hand-tracking
    support declarations to the VR manifest. The merged manifest contains
    `com.oculus.permission.HAND_TRACKING`, `oculus.software.handtracking`,
    `com.oculus.handtracking.version=V2.0`, and
    `com.oculus.handtracking.frequency=HIGH`. This changed the fresh Android
    failure signature: logcat no longer shows a new
    `RequiresControllersLaunchInterceptor` for the IMM launch; instead,
    `SystemUXController` accepts the VR-category launch and then reports
    `Launch is blocked because: a Reprojected OS dialog is currently showing`.
  - `logs/android-openxr-vrmanifest-postlaunch-unblock-attempt-20260605-1902/`
    adds a post-launch ADB keyevent unblock attempt. It still does not reach
    `IMM_ANDROID_OPENXR_PROBE` native startup. The relevant logcat lines show
    `START ... cat=[com.oculus.intent.category.VR] ... result code=0`, then
    the same reprojected OS dialog cache, followed by
    `VolumetricWindowManagerServiceImpl: Timeout while requesting window
    placement` for `org.linuxfoundation.imm.player/.MainActivity`.
  - `build/validation/vulkan-openxr-20260605-190008/validation-status.txt`
    rebuilds and verifies the Android APK matrix after the VR manifest
    hand-tracking declarations. Android Vulkan non-VR, GLES fallback, legacy
    VR/Oculus, OpenXR probe, OpenXR VR-manifest probe, and merged VR-manifest
    verification all pass. The only VR-manifest runtime issue is still headset
    launch state before native startup.
  - `build/validation/vulkan-openxr-20260605-190255/validation-status.txt`
    verifies the aggregate classifier now treats the current Android
    VR-manifest runtime state as `BLOCKED` on `reprojected Quest OS dialog`
    rather than as a probe failure.
- `code/appImmViewer/scripts/probe-openxr-runtime.ps1` can load that DLL and
  enumerate OpenXR instance extensions. Earlier local runs created an instance,
  selected an HMD system, queried system properties, and enumerated primary
  stereo views, but the current active SteamVR runtime now returns
  `createInstanceResult=-2` from `xrCreateInstance`. The standalone app-side
  `IMM_OPENXR_STANDALONE` probe reports the same current result.
  The script was refreshed on 2026-06-05 to use `Add-Type -CompilerOptions
  "/unsafe"` on this PowerShell host; the latest rerun at 2026-06-05 17:23
  enumerated 48 extensions and still failed `xrCreateInstance` with
  `XR_ERROR_RUNTIME_FAILURE`.
- The rebuilt Windows Release standalone app-side OpenXR probe was rerun on
  2026-06-05 with `settings-openxr-probe.json`. It selected Vulkan, requested
  `XRRuntime: OpenXR`, enumerated 48 extensions through the SteamVR loader, and
  logged `IMM_OPENXR_STANDALONE createInstanceResult=-2
  resultName=XR_ERROR_RUNTIME_FAILURE instance=0`.
- SteamVR logs identify the current Windows OpenXR blocker as hardware/runtime
  state rather than a loader-discovery failure. The latest runtime probe at
  2026-06-05 17:23:27 is within one minute of the local clock check at
  2026-06-05 17:23:46. `C:\Program Files
  (x86)\Steam\logs\xrclient_pwsh.txt` reports `Received connect response
  VRInitError_Driver_WirelessHmdNotConnectedAndNoSteam. Giving up.` for the
  refreshed probe. `C:\Program Files (x86)\Steam\logs\vrserver.txt` reports
  `No connected devices found` and `Refusing connect ... because
  VRInitError_Driver_WirelessHmdNotConnectedAndNoSteam`.
- A later hardware-state probe at local clock `2026-06-05 18:33:38 +01:00`,
  recorded in
  `build/validation/vulkan-openxr-20260605-183328/windows-steamvr-hmd-state.txt`,
  shows the same current blocker. `xrclient_pwsh.txt` reports at
  `Fri Jun 05 2026 18:33:32.602`:
  `Received connect response VRInitError_Driver_WirelessHmdNotConnectedAndNoSteam`.
  `vrserver.txt` reports `No connected devices found` and
  `Refusing connect ... because
  VRInitError_Driver_WirelessHmdNotConnectedAndNoSteam`. The process snapshot
  showed Oculus/Virtual Desktop services but no connected SteamVR HMD state.
- The runtime reports graphics extensions including:
  - `XR_KHR_vulkan_enable`
  - `XR_KHR_vulkan_enable2`
  - `XR_KHR_D3D11_enable`
  - `XR_KHR_D3D12_enable`
  - `XR_KHR_opengl_enable`
- The earlier successful runtime probe reported:
  - `systemName=Vive OpenXR: Vive SRanipal`
  - `viewConfigurationType=2`
  - two primary stereo views
  - recommended stereo eye size `2112x2304`
  - max swapchain size `8192x8192`

### Dependency-Gated

- `openxr_loader.lib` is not present in the repository or installed SDK search
  paths checked by `code/appImmViewer/scripts/check-openxr-deps.ps1`, but this
  is optional for the first dynamic-loader skeleton.
- `code/projects/windows/bootstrap-openxr-sdk.ps1 -BuildLoader` currently hangs
  during CMake/Visual Studio compiler detection on this machine before producing
  `openxr_loader.lib`. Keep the normal bootstrap path header-only until that
  toolchain issue is diagnosed.

### Hardware-Gated

- Real-runtime OpenXR session creation, frame loop, pose acquisition, and frame
  submission remain unvalidated.
- Fake-loader validation proves IMM loader override, dynamic export loading,
  instance creation, HMD system query, system property query, view configuration
  query, stereo-view query, OpenXR Vulkan instance-extension requirement query,
  Vulkan device-extension requirement query, Vulkan graphics API requirement
  query, typed Vulkan graphics-binding handoff into session creation,
  no-render session creation, session begin/end, one frame
  wait/begin/end cycle, fake Vulkan swapchain format enumeration, swapchain
  creation, `XrSwapchainImageVulkanKHR` image enumeration, acquire/wait/release
  ordering, projection-layer submission with two views, and teardown behavior.
  It does not validate a real OpenXR runtime session, real tracking, compositor
  submission, Android OpenXR loader behavior, or real Vulkan/OpenXR swapchain
  interop.
- IMM app-side Android OpenXR validation is no longer completely
  implementation-gated at startup: an explicit Android OpenXR startup-probe APK
  now builds. It is still runtime-gated because the current Quest OS focus state
  prevents the APK from reaching native startup, and it still does not implement
  a full Android OpenXR render/session backend or Vulkan swapchain interop.
- Headset validation for existing VR and Vulkan/OpenXR remains gated until
  those runtime paths are implemented and/or selected by a runnable test APK.
- Android runtime validation is currently gated by headset OS state, not by APK
  build state. Earlier direct OpenXR probe launches proved the non-VR manifest
  and initial VR manifest were blocked before native startup. The hand-tracking
  VR-manifest attempt at local clock `2026-06-05 18:57 +01:00` narrowed that:
  the fresh controller-required launch interceptor is avoided, but the app is
  still blocked by the visible VR lockscreen/Guardian/reprojected OS dialog
  state and times out during Horizon OS volumetric-window placement before
  `IMM_ANDROID_OPENXR_PROBE` can run. The blocker is therefore current headset
  launch state, not APK build, ADB attachment, device wakefulness, the non-VR
  manifest, or a missing hand-tracking manifest declaration.

## Risk Register

### Vulkan Renderer Gaps

The Vulkan renderer still has unsupported feature paths. Any OpenXR Vulkan work
must start from the features actually used by the player, not from assuming full
renderer parity.

Mitigation: keep Phase 1 focused on real sample playback and log unsupported
feature usage with searchable prefixes.

### VR Texture Submission

Existing VR code passes texture identities in forms that are valid for GL/DX
style paths but insufficient for Vulkan.

Mitigation: introduce typed backend-specific swapchain image wrappers instead of
expanding the old integer-ID API.

### OIT/MSAA Regression

The renderer's MSAA behavior is central to OIT appearance.

Mitigation: keep OIT/MSAA render targets internal at first, and perform the XR
handoff after the existing resolve boundary. Do not change sample counts,
accumulation formats, or blending semantics as part of OpenXR adoption.

### Color-Space Regression

Window presentation and XR compositor submission may expect different output
encoding contracts.

Mitigation: keep output encoding explicit at the final handoff. Validate
brightness against OpenGL/GLES references and headset output separately.

### Debugging Too Many Variables

Combining Vulkan and OpenXR from the start makes failures ambiguous.

Mitigation: use small vertical slices and keep known-good fallbacks available
until each layer has been validated.

## Suggested First Milestone

The first milestone should be:

1. Windows non-VR Vulkan smoke remains passing and visual output remains
   correct.
2. Android non-VR Vulkan shows correct `sample1.imm` content in the headset or
   in an equivalent compositor capture on target hardware.
3. Windows and Android non-VR runtime defaults are Vulkan.
4. Windows and Android standalone players use the same renderer request model:
   `RenderingAPI` chooses the runtime backend, while build flags describe the
   compiled backend capability set and default.
5. OpenGL/GLES fallback settings remain available for comparison and support.
6. Android VR remains on its current GLES/Oculus path.
7. OpenXR remains explicitly out of the Vulkan-default milestone except for
   documenting what is not yet implemented.

Current status: this is still an OpenXR, Vulkan, Android, and Windows adoption
task, not just a non-VR Vulkan renderer task. On 2026-06-05 the validated
slices are: Windows Vulkan `sample1.imm` non-VR smoke, Android Vulkan
`sample1.imm` visible content in the Quest 3S compositor capture, Android GLES
fallback build, Android legacy VR/Oculus build, Windows DirectX baseline
capture, and the Windows app-side OpenXR startup/session/frame-loop/fake
Vulkan-swapchain probe against a controlled fake loader, including fake
OpenXR/Vulkan graphics-requirement and extension queries. The renderer now has
an OpenXR-shaped Vulkan external-image frame entry point that accepts the
`VkImage`/format/dimensions/layer-count data provided by OpenXR swapchain image
enumeration and creates the required Vulkan image view internally. The
unvalidated
slices remain core to the task: the active SteamVR Windows OpenXR runtime
currently fails at `xrCreateInstance` because SteamVR reports no connected HMD;
the installed Meta OpenXR runtime can create and destroy an OpenXR instance via
`XR_RUNTIME_JSON` override but reports no HMD system; the IMM Android OpenXR
startup-probe APK build exists but runtime launch is blocked before native
startup by Quest OS focus state; real Vulkan/OpenXR swapchain interop has not
been validated against a real runtime/compositor; current Android non-VR
automated reruns for both GLES and Vulkan are currently blocked by Quest OS
focus state (display asleep/off, VR lockscreen, Guardian, or reprojected OS
dialog) before CPU load; and existing Windows/Android VR runtime behavior is
not headset-validated in this pass.

After that milestone, the next OpenXR milestone is:

1. Keep the app-side OpenXR session/frame-loop/fake-swapchain probe passing
   against the controlled fake loader.
2. Prove `xrCreateInstance` against a real runtime when headset/runtime state is
   available.
3. Prove `xrGetSystem` against a real runtime.
4. Prove session creation against a real runtime with a valid graphics binding.
5. Prove frame wait/begin/end without rendering.
6. Submit a trivial stereo frame.
7. Only then choose the first real OpenXR graphics binding based on target
   runtime support and the least risky path to a visible stereo frame.
