# Quest XR Handover - 2026-02-12

## Goal

Get `ImmUnitySampleProject` rendering IMM paint strokes in Quest OpenXR immersive mode without regressing Android non-XR or Windows builds.

## Current Outcome

- Quest XR launch is partially working: app can start in immersive mode and scene/background renders.
- IMM plugin initializes and render loop runs.
- **IMM strokes still not visible in headset**.

## Important Confirmed Facts

1. Runs containing `Launch is blocked because: device in VRUI` are invalid for XR conclusions.
2. The old single-pass viewport double-width hack was removed from native rendering path.
3. Extra C# startup probing around layer visibility previously caused UnityMain SIGSEGV; that approach was reverted.
4. Layer diagnostics exist in native code, but are now disabled by default (guarded toggle).

## What Changed Since Earlier Handover

Two commits landed after the earlier debugging round:

- `6398c60` `partially working xr android builds`
- `e0e6041` `Guard/reduction pass for last commit`

The guard/reduction pass notably did this:

1. `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs`
   - XR-specific event behavior is gated to Android XR (`isAndroidXr`).
   - CommandBuffer hook event is switched per mode (`AfterEverything` in XR, `AfterImageEffectsOpaque` otherwise).
   - Immediate XR plugin issue default is now conservative (`forceImmediateIssueInXR = false`).

2. `code/appImmUnity/src/main.cpp`
   - Native XR layer-forcing diagnostics in `GlobalWork` are behind `kEnableXrLayerDiag` and defaulted off.
   - Android compile guards wrap the XR-layer diagnostic block.

3. `code/ImmUnitySampleProject/Assets/Plugins/Android/AndroidManifest.xml`
   - `android.hardware.vr.headtracking` changed to `android:required="false"` to reduce non-XR install/routing risk.

4. `code/ImmUnitySampleProject/Assets/Scenes/SampleSceneVR.unity`
   - Temporary diagnostic scene edits were normalized (light/background/viewpoint/spawn setting reverted).

## Current Working Tree (Now)

From `git status --short`:

- `M .gitignore`
- `M code/ImmUnitySampleProject/Assets/XR/Settings/OpenXR Package Settings.asset`
- `?? QUEST_XR_HANDOVER_20260212.md`
- `?? code/projects/android/run-android-cycle.ps1`

Notes:

- `OpenXR Package Settings.asset` has local normalization edits pending (disabling toggled profiles from experimentation).
- `run-android-cycle.ps1` contains adb timeout/fail-fast hardening and is not yet committed in current tree.

## Key Logs / Captures (Still Relevant)

- `code/projects/android/logs/logcat-full-20260212-160009.txt` (stable XR run, single-pass activity)
- `code/projects/android/logs/logcat-full-20260212-160447.txt` (managed layer force reported count 0)
- `code/projects/android/logs/logcat-full-20260212-161457.txt` (SIGSEGV from heavy C# startup probing)
- `code/projects/android/logs/logcat-full-20260212-161821.txt` (VRUI launch block evidence)
- `code/projects/android/logs/questcap-20260212-1601.png`
- `code/projects/android/logs/questcap-20260212-1548.png`

## Immediate Next Steps

1. Create a clean baseline run with valid launch state (no VRUI block).
2. Run small regression matrix before new XR experiments:
   - Android non-XR smoke
   - Android XR smoke
   - Windows build + run smoke
3. If regressions are clear, continue XR stroke investigation with diagnostics that stay native-side and lightweight.
4. If needed, temporarily enable native XR layer diagnostics by toggling `kEnableXrLayerDiag` in `code/appImmUnity/src/main.cpp`, then disable again after capture.

## Known Bad / Avoid

- Do not trust any run with `device in VRUI` launch block.
- Do not reintroduce heavy C# per-frame startup layer probing in XR path.
- `adb screencap` is eye-view only, not full immersive proof.

## Continuation Update (2026-02-13)

### What Was Proved After 2026-02-12

1. **XR render gate startup behavior is real but temporary.**
   - In early XR single-pass frames, render can skip with `mAnyDocToRender=0` (`IMMDBG_NA02_RENDERGATE`).
   - `mAnyDocToRender` transitions to `1` and remains `1` (`IMMDBG_NA03_ANYDOC`), so startup gating is not the persistent failure.

2. **Stroke pass executes in active XR path with non-zero workload.**
   - `IMMDBG_NA01_STROKEPASS` confirmed from `RenderStereoSinglePass` after placement correction.
   - Representative counters: non-zero layers/stroke layers/strokes and paint draw calls/triangles.

3. **XR layer content exists and is populated at runtime.**
   - Layer diagnostics reconfirmed `layerCount=75` with visible paint layers and non-zero strokes.

4. **Post-pass/callback writes are being issued, but headset visibility outcome is still the discriminator.**
   - NA05 pass marker now executes in single-pass stroke path.
   - NA06 post-render callback marker executes after `RenderStereoSinglePass`.
   - NA07 full-screen alternating post-callback color clears execute in early frames.
   - Logs prove commands are issued; they do not alone prove presented-eye visibility.

### Key New Evidence Files

- `code/projects/android/logs/logcat-full-20260213-091246.txt` (NA02 render-gate probe)
- `code/projects/android/logs/logcat-full-20260213-095610.txt` (NA03 any-doc transition)
- `code/projects/android/logs/logcat-full-20260213-095824.txt` (NA01 corrected + NA04 GL state)
- `code/projects/android/logs/logcat-full-20260213-101254.txt` (NA05 pass marker placement fix)
- `code/projects/android/logs/logcat-full-20260213-101548.txt` (NA06 post-callback marker)
- `code/projects/android/logs/logcat-full-20260213-101935.txt` (NA07 full-screen callback flash)

### Updated Technical Read

- The issue is no longer "content missing" or "stroke pass never runs".
- Current strongest suspect is XR composition/state interaction after (or around) stroke rendering.
- NA04 state snapshots show meaningful state drift across callback boundaries (for example cull/program/active texture changes), which may matter to composition outcome.

### Process Constraint Discovered

- `set-unity-android-profile.ps1` can reset `libImmUnityPlugin.so` to profile baselines.
- For native instrumentation changes (`player.cpp`, `main.cpp`) to survive into APK:
  1. rebuild native libs,
  2. avoid overwriting them with profile reset,
  3. run with `-Profile Current` when appropriate.

### Current Working Tree Snapshot (2026-02-13)

From latest `git status --short` at handover update time:

- `M .gitignore`
- `A code/ImmUnitySampleProject/Assets/Plugins.meta`
- `A code/ImmUnitySampleProject/Assets/Plugins/Android.meta`
- `A code/ImmUnitySampleProject/Assets/Plugins/Android/AndroidManifest.xml`
- `A code/ImmUnitySampleProject/Assets/Plugins/Android/AndroidManifest.xml.meta`
- `M code/ImmUnitySampleProject/Assets/Scripts/ImmFeatureExamples.cs`
- `MM code/ImmUnitySampleProject/Assets/XR/Settings/OpenXR Package Settings.asset`
- `M code/ImmUnitySampleProject/Assets/XR/XRGeneralSettingsPerBuildTarget.asset`
- `MM code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/Android/libs/arm64-v8a/libImmUnityPlugin.so`
- `M code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs`
- `M code/ImmUnitySampleProject/ProjectSettings/EditorBuildSettings.asset`
- `MM code/appImmUnity/src/main.cpp`
- `M code/libImmPlayer/src/player.cpp`
- `?? NEXT_AGENT_STRATEGY_GUIDE.md`
- `?? QUEST_XR_HANDOVER_20260212.md`
- `?? XR_EXPERIMENT_LOG.md`
- `?? code/projects/android/run-android-cycle.ps1`
- `?? code/projects/android/set-unity-android-profile.ps1`

### Immediate Next Single Hypothesis

**Hypothesis:** XR callback writes are executed but later composition stage does not present those writes (or presents a different target).

**Next run objective:** In one valid non-VRUI Quest XR run, capture direct headset observation tied to NA05/NA06/NA07 markers, then decide branch:

1. NA06/NA07 visible, strokes absent -> isolate stroke shader/data/state path under proven-present callback output.
2. NA06/NA07 not visible -> prioritize Unity OpenXR plugin event integration / eye target presentation ownership tracing.
