# Unity XR Priority Plan

## Purpose

Unity XR is the higher-priority path for this codebase because Unity already owns the OpenXR runtime integration: session lifecycle, swapchains, frame timing, eye poses, input profiles, and headset presentation. The native IMM Unity plugin should focus on rendering correctly inside Unity's render targets and command-buffer/plugin-event flow.

This plan covers three steps:

1. Keep Unity Windows DX11 OpenXR as the known-good path.
2. Add and validate Unity Android OpenXR, starting with the currently configured GLES3 path.
3. Consider Unity Vulkan only after the Unity OpenXR paths are stable and there is a concrete reason to need Vulkan.

## Current Baseline

### Confirmed

- Unity project includes `com.unity.xr.openxr` version `1.14.3`.
- Unity project includes `com.unity.xr.management` version `4.5.4`.
- OpenXR loader asset exists at `code/ImmUnitySampleProject/Assets/XR/Loaders/OpenXRLoader.asset`.
- XR general settings include OpenXR providers for Standalone and Android.
- Unity Windows OpenXR was validated through the sample project using DX11.
- Unity Windows DX11 rendering is upright and nonblack after the native depth convention fix and Unity projection convention fix.
- The Unity native plugin now treats DX11 command-buffer rendering as texture-style projection to avoid vertically inverted Game View/XR output.

### Not Confirmed

- Unity Android OpenXR rendering on device.
- Unity Android native plugin loading and rendering under OpenXR.
- Unity Android GLES3 XR render-target orientation/depth/color behavior.
- Unity Android Vulkan OpenXR.
- Unity Windows Vulkan OpenXR.

### Important Distinction

Standalone OpenXR/Vulkan work does not directly implement Unity OpenXR/Vulkan support. In Unity, Unity owns OpenXR and presentation. The IMM plugin only needs to render correctly into the render targets Unity gives it.

## Step 1: Preserve Unity Windows DX11 OpenXR As Known-Good

### Goal

Keep a stable Windows Unity XR baseline while other XR targets are developed.

### Scope

- Windows Standalone / Editor
- Unity OpenXR
- Direct3D11
- `SampleSceneVR.unity`
- IMM Unity package native plugin path

### Required Behaviors

- Scene loads without script/native plugin errors.
- OpenXR initializes with real runtime when available, or Unity Mock Runtime for automation.
- IMM content renders nonblack.
- Output is upright.
- Eye placement is correct.
- No projection/depth regression.
- Non-XR `SampleScene.unity` remains correct.

### Actions

1. Keep Windows graphics API on DX11 for the Unity sample unless explicitly testing another backend.
2. Keep `SampleScene.unity` as the default non-XR sanity scene.
3. Keep `SampleSceneVR.unity` as the OpenXR validation scene.
4. Maintain a small Editor validation script or MCP workflow that records:
   - `SystemInfo.graphicsDeviceType`
   - XR enabled/running state
   - XR display name
   - stereo mode
   - command buffer count
   - render target size
5. Preserve screenshot capture for visual evidence:
   - non-XR DX11 upright capture
   - XR mock/real runtime capture where possible
6. When changing native renderer code, always run Windows Unity DX11 non-XR first, then Unity OpenXR.

### Acceptance Criteria

- Windows non-XR sample renders upright and nonblack.
- Windows OpenXR sample renders upright and nonblack.
- Unity logs contain no current native plugin or shader errors relevant to the test timestamp.
- Any expected OpenXR runtime failure is clearly distinguished from plugin rendering failure.

### Regression Triggers

Run this baseline after changes to:

- native renderer projection/depth/color handling
- `imm_engine_bridge.cpp`
- Unity `ImmPlayerManager.cs`
- native plugin event/render-target handling
- platform-specific renderer code
- Unity XR settings
- Unity graphics API settings

## Step 2: Add And Validate Unity Android OpenXR With GLES3 First

### Goal

Make Unity Android OpenXR render IMM content correctly on headset using the currently configured Android graphics API path before introducing Vulkan.

### Why GLES3 First

The Unity sample project currently serializes Android graphics API as GLES3. Starting with the existing configuration reduces variables:

- Unity OpenXR session and Android lifecycle are new enough.
- Native Android plugin loading is a separate risk.
- Headset deployment/log collection is a separate risk.
- Vulkan adds render-target layout, format, and backend differences on top.

### Scope

- Unity Android build target
- Unity OpenXR loader
- Android GLES3 graphics API
- Quest/Pico-style Android headset runtime depending on available device
- IMM Unity package Android native plugin path

### Required Preflight

1. Confirm Android OpenXR provider is assigned in `XRGeneralSettingsPerBuildTarget.asset`.
2. Confirm Android graphics API is GLES3.
3. Confirm Android OpenXR feature set is suitable for target device:
   - Oculus/Meta controller profiles if Quest is target
   - Meta/Oculus Quest support feature if required by runtime/package version
4. Confirm Android native plugin binaries are present in the Unity package/import settings.
5. Confirm sample `.imm` file is included in build or accessible at runtime.
6. Confirm scenes in build settings include the intended Android XR sample scene.

### Implementation Tasks

1. Create or verify an Android XR sample scene:
   - camera/XR rig
   - `ImmPlayerManager`
   - sample IMM load path
   - simple locomotion only if needed
2. Add build automation for Android XR:
   - build target Android
   - OpenXR enabled
   - GLES3 graphics API
   - development build option for logs
3. Add Android-accessible logging:
   - do not rely only on Unity console
   - write plugin/XR validation output to a readable file if feasible
   - also capture `adb logcat` from the build/run script
4. Add runtime probe logging with a unique prefix:
   - graphics API
   - XR enabled/running
   - XR display subsystem state
   - render target dimensions
   - plugin event path
   - sample load status
5. Deploy to headset and validate visually.
6. Capture evidence:
   - `adb logcat` segment
   - app-specific log file if implemented
   - screenshot/screen recording if available

### Rendering Checks

Validate the same failure classes as Windows:

- nonblack output
- upright orientation
- correct stereo eye placement
- no mirror inversion
- no incorrect scale/IPD feel
- no depth clipping/black due to depth range
- no washed-out or double-gamma output
- no one-eye-only rendering
- no severe frame timing issue caused by plugin event ordering

### Acceptance Criteria

- Android Unity OpenXR app starts on headset.
- IMM sample content loads.
- IMM content renders in both eyes.
- Output is upright and spatially plausible.
- Logs show GLES3 graphics API and active OpenXR runtime.
- No current timestamped native plugin errors block rendering.

### Likely Risks

- Android OpenXR feature configuration may be incomplete for Quest/Meta runtime.
- Native plugin ABI/import settings may be incomplete.
- File paths for `.imm` assets may differ from Editor/Standalone.
- Unity render target orientation may differ from Windows.
- GLES external texture/render target semantics may differ from existing desktop path.
- Device logs may be noisy; validation needs unique prefixes.

### Do Not Do Yet

- Do not switch Android Unity to Vulkan until GLES3 OpenXR is proven or clearly blocked.
- Do not conflate standalone Android Vulkan/OpenXR with Unity Android OpenXR. They are different runtime ownership models.

## Step 3: Consider Unity Vulkan Only If Needed

### Goal

Decide whether Unity Vulkan is necessary after Windows DX11 OpenXR and Android GLES3 OpenXR are stable.

### Decision Inputs

Only prioritize Unity Vulkan if at least one is true:

- target headset/platform requires Vulkan for desired Unity OpenXR support
- GLES3 path is blocked by a Unity/OpenXR/device limitation
- performance requires Vulkan after measuring actual content
- feature work depends on Vulkan-specific native renderer behavior
- product/distribution requirements specify Vulkan

### What Unity Vulkan Would Mean Here

Unity Vulkan is not the same as standalone Vulkan OpenXR.

Unity still owns:

- OpenXR instance/session
- swapchains
- frame timing
- view poses
- presentation

The IMM Unity plugin would need to:

- receive Unity Vulkan plugin events correctly
- render into Unity-provided Vulkan render targets
- handle Unity's Vulkan texture/image state expectations
- use correct projection convention
- use correct depth convention
- use correct color/sRGB handling
- synchronize with Unity's render thread and command buffers

### Investigation Tasks

1. Confirm Unity version and target platform support matrix for Vulkan + OpenXR in this project.
2. Add a temporary Unity graphics API override for Vulkan in a branch/test run only.
3. Verify whether the IMM Unity native plugin currently initializes under Unity Vulkan.
4. Check Unity native rendering plugin API requirements for Vulkan:
   - device event callbacks
   - command buffer access
   - texture/image handles
   - render target layout/state
5. Run a minimal clear-only plugin event before full IMM rendering.
6. Run `SampleScene.unity` non-XR with Vulkan.
7. Run `SampleSceneVR.unity` with Vulkan OpenXR only after non-XR Vulkan is stable.

### Acceptance Criteria For Taking Vulkan Forward

- Unity Vulkan non-XR renders a clear or simple IMM frame.
- Unity Vulkan OpenXR starts with active XR runtime.
- Plugin event renders into XR eye targets.
- Output is upright, nonblack, and stereo-correct.
- Vulkan validation/logs do not show render-target state misuse.

### Reasons To Stop/Park Vulkan

Park Unity Vulkan if:

- DX11/GLES3 OpenXR meet current needs.
- Vulkan requires major Unity native plugin infrastructure that duplicates standalone work.
- headset validation is blocked by platform/runtime support.
- performance data does not justify the added backend risk.

## Shared Validation Rules

### Logs

Use unique prefixes for every validation run so relevant entries can be found in full logs. Do not rely on arbitrary tail limits.

Suggested prefixes:

- `IMM_UNITY_WIN_XR_`
- `IMM_UNITY_ANDROID_XR_`
- `IMM_UNITY_VULKAN_XR_`

### Visual Evidence

Rendering is the priority. Every backend claim should be backed by at least one of:

- screenshot
- headset observation with matching current logs
- mirror capture
- automated capture with nonblank threshold
- frame/debug log proving active render path and dimensions

### Current-Time Checks

When checking Unity logs, compare relevant errors against the current clock time. Old errors should not be reported as current failures.

## Recommended Order

1. Lock down Windows Unity DX11 OpenXR as a regression baseline.
2. Build/deploy Unity Android OpenXR with GLES3.
3. Fix Android GLES3 OpenXR rendering issues until sample content is correct.
4. Decide whether Unity Vulkan is needed based on real blocker/performance data.
5. Only then run Unity Vulkan experiments.

## Near-Term Checklist

- [ ] Confirm current Unity project settings after recent local edits.
- [ ] Record Windows DX11 OpenXR baseline command/MCP workflow.
- [ ] Add Android XR build/run script or document exact manual build steps.
- [ ] Verify Android OpenXR feature configuration for the target headset.
- [ ] Verify Android native plugin import settings and ABI coverage.
- [ ] Ensure sample IMM asset path works in Android player.
- [ ] Add Android runtime validation logging with unique prefix.
- [ ] Build and deploy Android GLES3 OpenXR sample.
- [ ] Validate headset rendering.
- [ ] Decide whether Unity Vulkan is justified.

## Non-Goals

- Do not make standalone OpenXR/Vulkan a prerequisite for Unity Android OpenXR.
- Do not fix legacy standalone Oculus fast stereo unless required.
- Do not start Unity Vulkan before Android GLES3 OpenXR is attempted.
- Do not treat theoretical Unity support as repo support without a local build/render validation.
