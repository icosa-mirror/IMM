# Android IMM Handover (2026-02-11)

## Scope

This handover summarizes the current Android investigation status for IMM Unity playback/rendering and provides a reproducible workflow for a new agent session.

Primary question being investigated: why Android appears blank while PC/editor builds show expected content.

## Current State (What Is Confirmed)

1. Rendering callbacks are firing on Android.
   - Evidence in key logs:
     - `code/projects/android/logs/logcat-key-20260211-150734.txt:10`
     - `code/projects/android/logs/logcat-key-20260211-150851.txt:10`
   - Prefix: `[IMMDBG_RENDER_20260211A]`.

2. Plugin draw output modifies framebuffer pixels.
   - Pixel probe transitions from clear-color values to scene-like values:
     - `code/projects/android/logs/logcat-full-20260211-150734.txt:3218`
     - `code/projects/android/logs/logcat-full-20260211-150851.txt:3083`
   - Prefix: `[IMMDBG_RENDERPIX_20260211A]`.

3. In-app screenshot capture is non-black.
   - Files:
     - `code/projects/android/logs/imm-appcap-20260211-150734.png`
     - `code/projects/android/logs/imm-appcap-20260211-150851.png`
   - Both images have broad color distribution and non-zero luminance.

4. `adb screencap` images for the app are black in this setup, while home-screen captures are normal.
   - Black app captures (all zero luminance):
     - `imm_latest.png`
     - `imm_now.png`
     - `imm_screen.png`
   - Non-black control capture:
     - `home_now.png`
   - Conclusion: `adb screencap` is not a reliable truth source for this app path/device.

5. Bounds corruption exists and is severe.
   - Giant/sentinel/inverted values appear in layer bounds:
     - `code/projects/android/logs/logcat-full-20260211-150734.txt:3287`
     - `code/projects/android/logs/logcat-full-20260211-150851.txt:3220`
     - `code/projects/android/logs/logcat-full-20260211-150851.txt:3224`
   - Problematic cluster around layers ~61-68.

6. Managed/native bounds struct layout mismatch was fixed earlier.
   - File with correction:
     - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmNativePlugin.cs`
   - Managed order now aligned to native (`minX,maxX,minY,maxY,minZ,maxZ`).

7. Filtered bbox aggregation in plugin can produce sane managed bbox despite invalid layers.
   - Example sane autoplay bbox:
     - `code/projects/android/logs/logcat-key-20260211-150851.txt:39`
   - But this does not by itself prove that all visual output issues are solved on-device.

## What Is Not Yet Proven

1. Whether the user-visible blank screen (physical device view) is fully explained by bad bbox/framing.
   - Bounds corruption is real and harmful, but rendering also clearly occurs.
   - Therefore "bounds-only" is currently an incomplete explanation.

2. Whether remaining issue is compositor/surface presentation path vs camera/frustum placement vs content visibility logic.

## Working Hypothesis (Ranked)

1. **Confirmed bug:** invalid layer bounds corrupt document framing/placement.
2. **Likely additional factor:** Android presentation/capture path mismatch caused diagnosis confusion (`adb screencap` false black).
3. **Open question:** Is there still a true on-screen blank on device after sane framing, or was this primarily a measurement artifact?

## Repository / Environment Notes

- Repo root: `C:\Users\andyb\Documents\IMM`
- This is a dirty worktree with many modified/untracked files. Do not revert unrelated changes.
- Android cycle script:
  - `code/projects/android/run-android-cycle.ps1`
- Unity batch build method called by script:
  - `ImmPlayer.Editor.BuildAutomation.BuildAndroidDebug`
- Unity editor constraint from local instructions:
  - If Unity script reload/compile is needed in open editor mode, Unity must be focused and not in play mode (cannot be automated here).

## Detailed Testing Workflow

### 1) Baseline unattended cycle

From repo root (`C:\Users\andyb\Documents\IMM`):

```powershell
powershell -ExecutionPolicy Bypass -File "code/projects/android/run-android-cycle.ps1" -ForceCloseUnity -RunSeconds 20
```

What this script does (important for continuation):

1. Builds native Android plugins via Gradle:
   - `:appImmUnity:assembleDebug`
   - `:appImmStrokeReader:assembleDebug`
2. Builds Unity APK in batchmode.
3. Installs APK (`adb install -r`).
4. Clears logcat, launches app, waits `RunSeconds`, dumps full log.
5. Pulls in-app screenshot from:
   - `/storage/emulated/0/Android/data/com.DefaultCompany.IMMUnityTest/files/imm_appcap.png`
6. Extracts key diagnostics into `logcat-key-*.txt`.

Generated artifacts:

- Full log: `code/projects/android/logs/logcat-full-<timestamp>.txt`
- Key log: `code/projects/android/logs/logcat-key-<timestamp>.txt`
- In-app screenshot: `code/projects/android/logs/imm-appcap-<timestamp>.png`
- Unity build log: `code/projects/android/logs/unity-android-build.log`

### 2) Verify render-path activity quickly

Check for these prefixes in latest full log:

- `[IMMDBG_RENDER_20260211A]`
- `[IMMDBG_RENDERPIX_20260211A]`
- `[IMMDBG_PLAY_20260211B]`

If render events and render pixel post-values change from pre-values, plugin drawing is active.

### 3) Validate bounds health

Search for bbox diagnostics in full log:

- `[IMMDBG_BBOX_20260211D]`

Red flags:

- Values near `1e30`
- Inverted bounds (`min > max`)
- Giant layer bounds dominating document bbox

### 4) Compare capture channels

Use both channels per run:

1. In-app capture pulled by script (`imm-appcap-*.png`) - high trust.
2. Optional manual `adb screencap` - low trust in this environment.

### 5) Crash triage (if app crashes)

- Search log for `SIGSEGV|FATAL|tombstone`.
- Preserve latest tombstone files in repo root if generated.
- Do not assume crash and blank-screen have the same root cause.

## Decisive Next Tests (Fastest Path)

1. **A/B presentation test (highest priority):**
   - Add a forced fullscreen Unity camera clear/color overlay pass independent of plugin visibility.
   - Run cycle and verify physical device output + in-app capture.
   - Outcome interpretation:
     - If overlay visible but plugin content absent -> plugin compositing/framing/content issue.
     - If overlay not visible -> broader app presentation/surface path issue.

2. **Hard framing override test:**
   - Temporarily bypass auto-framing from document bbox.
   - Pin camera + content transform to known good values.
   - If content appears, bbox/framing remains the primary blocker.

3. **Layer isolation test:**
   - Render only known-good layer subset (exclude layers with sentinel bounds).
   - Confirm visibility delta.

4. **Bounds source-of-truth fix planning:**
    - Keep plugin-side filtering as guard.
    - Move canonical fix upstream into importer/player layer-bounds generation and validity rules.

## Continuation Update (2026-02-11 15:57)

### Test Executed: A/B presentation probe via forced Unity fullscreen overlay

- Implemented Android-only diagnostic overlay + camera solid-color clear in:
  - `code/ImmUnitySampleProject/Assets/Scripts/ImmFeatureExamples.cs`
- New log prefix:
  - `[IMMDBG_PRESENT_20260211A]`
- Diagnostic run:
  - `code/projects/android/logs/logcat-full-20260211-155953.txt`
  - `code/projects/android/logs/logcat-key-20260211-155953.txt`
  - `code/projects/android/logs/imm-appcap-20260211-155953.png`

### What This New Run Confirms

1. Unity-side presentation path is active and visibly updating on Android.
   - Overlay enable + repeated color swaps are logged throughout the run.
   - Example lines:
     - `code/projects/android/logs/logcat-full-20260211-155953.txt:3383`
     - `code/projects/android/logs/logcat-full-20260211-155953.txt:3399`
     - `code/projects/android/logs/logcat-full-20260211-155953.txt:4435`

2. In-app capture contains the forced fullscreen diagnostic color (not black).
   - File:
     - `code/projects/android/logs/imm-appcap-20260211-155953.png`

3. Plugin render events still fire in the same run.
   - Example lines:
     - `code/projects/android/logs/logcat-key-20260211-155953.txt:7`
     - `code/projects/android/logs/logcat-key-20260211-155953.txt:14`

### Interpretation After This Test

- This reduces likelihood of a global app surface/compositor failure.
- The remaining blank-appearance risk is more consistent with IMM content visibility/framing/path-specific composition behavior than with total presentation failure.
- Next highest-value step remains hard framing override with the overlay disabled, to directly test content visibility independent of bbox-derived framing.

### Follow-up Test Executed: Hard framing override with overlay disabled

- Code path used (Android-only):
  - `code/ImmUnitySampleProject/Assets/Scripts/ImmFeatureExamples.cs`
- Overlay default switched off for this run (`forcePresentationOverlayOnAndroid = false`).
- Hard framing default switched on for this run (`forceHardFramingOnAndroid = true`).
- New framing prefix:
  - `[IMMDBG_FRAME_20260211A]`
- Diagnostic run:
  - `code/projects/android/logs/logcat-full-20260211-160838.txt`
  - `code/projects/android/logs/logcat-key-20260211-160838.txt`
  - `code/projects/android/logs/imm-appcap-20260211-160838.png`

Observed evidence:

1. Hard framing override executed.
   - `code/projects/android/logs/logcat-full-20260211-160838.txt:3323`

2. Presentation overlay logs are absent in this run (expected when disabled).

3. In-app screenshot is non-black and non-solid-color under hard framing.
   - `code/projects/android/logs/imm-appcap-20260211-160838.png`

Interpretation:

- With hard framing forced and overlay disabled, capture output remains valid and scene-like.
- This is consistent with the diagnosis that camera/content framing controls materially affect visibility outcomes and remains a primary investigation axis.

## Important File Locations

- Android cycle automation:
  - `code/projects/android/run-android-cycle.ps1`
- Native Unity plugin render path:
  - `code/appImmUnity/src/main.cpp`
- Unity managed native interop (bounds struct):
  - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmNativePlugin.cs`
- Unity player manager/render callback logic:
  - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs`
- Playback/sample harness and screenshot capture logic:
  - `code/ImmUnitySampleProject/Assets/Scripts/ImmFeatureExamples.cs`
- Importer layer bounds logic under investigation:
  - `code/libImmImporter/src/document/layer.cpp`
- Player bbox diagnostics/hardening:
  - `code/libImmPlayer/src/player.cpp`

## Practical Handoff Guidance

1. Start with one clean unattended cycle and only then change code.
2. Use unique log prefixes for any new diagnostics.
3. Avoid relying on `adb screencap` for pass/fail decisions.
4. Keep changes minimal and hypothesis-driven (one variable per run).
5. Preserve existing user changes; do not clean unrelated dirty files.

## One-Line Status

Rendering on Android is active and measurable; invalid layer bounds are confirmed and impactful, but additional validation is still required to explain the exact physical on-device blank-display symptom end-to-end.
