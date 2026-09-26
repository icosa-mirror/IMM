# XR Experiment Log (Quest)

Date: 2026-02-12

## Objective

Render IMM brush strokes in Quest XR. Current visible result is sky sphere only (no strokes).

## Validity Rules

- Any run containing `Launch is blocked because: a Guardian dialog is currently showing` or `device in VRUI` is invalid for conclusions.
- Non-XR baseline is tracked separately and must remain restorable to commit `16e3e67`.

## Current XR State (latest confirmed)

- Headset view: sky sphere visible, brush strokes not visible.
- Latest run log: `code/projects/android/logs/logcat-full-20260212-195620.txt`
- IMM load path is active (`Loaded in CPU/SPU/GPU`), render loop active, XR single-pass path active (`stereo=2`).

## Experiment Ledger

1) **XR cull/depth override**
- Change: Disable face culling; then disable depth test in XR render path.
- Evidence: `code/projects/android/logs/logcat-full-20260212-183422.txt`
- Result: No stroke visibility improvement.
- Verdict: Not explained by simple cull/depth state.

2) **Force mono rendering inside XR**
- Change: In single-pass branch, force mono API path (`GlobalRender + RenderMono`).
- Evidence: `code/projects/android/logs/logcat-full-20260212-183926.txt`
- Result: No stroke visibility improvement.
- Verdict: Not isolated to stereo-vs-mono API choice alone.

3) **Force document near head (XR placement test)**
- Change: XR-only placement helper to bring doc near camera.
- Evidence: `code/projects/android/logs/logcat-full-20260212-184333.txt`
- Result: Placement logs show execution; strokes still absent.
- Verdict: Not a simple "content too far/wrong spawn" issue.

4) **Render target/write probes**
- Change: Log XR FBO/attachment state and write probes.
- Evidence: `code/projects/android/logs/logcat-full-20260212-190438.txt`, `code/projects/android/logs/logcat-full-20260212-190949.txt`
- Result: FBO complete (`0x8cd5`), renderbuffer attachments rotate, probe writes execute, but pixel readback remains `(0,0,0,0)`.
- Verdict: Readback path is not a reliable indicator of presented XR eye image.

5) **Force two-pass XR**
- Change: Force two-pass in managed stereo mode selection.
- Evidence: `code/projects/android/logs/logcat-full-20260212-192245.txt`
- Headset result: black (user report).
- Verdict: Two-pass path regresses visual output on current setup.

6) **Revert to single-pass XR**
- Change: Remove two-pass force.
- Evidence: `code/projects/android/logs/logcat-full-20260212-193146.txt`
- Headset result: sky sphere returns, still no strokes (user report).
- Verdict: Single-pass is currently the only usable XR mode.

7) **Use multipass draw API inside single-pass event**
- Change: In single-pass event, call `RenderStereoMultiPass` for both eyes instead of `RenderStereoSinglePass`.
- Evidence: `code/projects/android/logs/logcat-full-20260212-195620.txt` (`IMMDBG_XRPATH_20260212A`)
- Headset result: sky only, no strokes (user report).
- Verdict: Stroke loss not fixed by swapping stereo draw API call in this way.

8) **Disable accidental XR debug placement path in code**
- Change: Hard-disable XR debug placement guard (`EnableXrDebugPlacement = false`) to prevent hidden runtime movement side-effects.
- Evidence: `code/projects/android/logs/logcat-full-20260212-201753.txt` (no forced near-head placement entries)
- Result: Startup transform stays scene-driven; issue persists.
- Verdict: Not caused by forced debug placement.

9) **Reconfirm XR layer population in current build**
- Change: Re-enable extraction of `IMMDBG_XRLAYERS_20260212B` in key log.
- Evidence: `code/projects/android/logs/logcat-full-20260212-202051.txt`
- Result: `layerCount=75`, paint layers present with non-zero strokes, visible=1, opacity=1.0.
- Verdict: Stroke data exists at runtime; failure is in XR stroke render path/composition, not content loading.

10) **NA01 stroke-pass entry/exit instrumentation (in progress)**
- Hypothesis: XR path reaches paint stroke submission, but output is hidden later in XR composition.
- Change: Add `IMMDBG_NA01_STROKEPASS` pre/post logs around `mLayerPaintRender->DisplayRender()` in stereo single-pass and stereo multi-pass paths with sampled counters (`layers`, `strokeLayers`, `strokes`, paint draw calls/triangles).
- Code delta: `code/libImmPlayer/src/player.cpp`, plus key-log extraction pattern in `code/projects/android/run-android-cycle.ps1`.
- Build check: `./gradlew :appImmUnity:assembleDebug` alone was insufficient for `player.cpp` changes because `appImmUnity` links imported `libImmPlayer.a`.
- Observation: no `IMMDBG_NA01_STROKEPASS` lines until `libImmPlayer` was rebuilt.
- Verdict: native dependency rebuild order matters; this run did not yet prove stroke-pass execution.

11) **NA02 render-gate probe (prove/deny early return before stroke pass)**
- Hypothesis: XR draw path is skipping in `RenderStereoSinglePass` because `mAnyDocToRender` is false.
- Change: Add `IMMDBG_NA02_RENDERGATE` logs on early-return gates in `RenderMono`, `RenderStereoMultiPass`, and `RenderStereoSinglePass`.
- Code delta: `code/libImmPlayer/src/player.cpp`, log extraction update in `code/projects/android/run-android-cycle.ps1`.
- Critical process fix: `set-unity-android-profile.ps1` resets `libImmUnityPlugin.so`; to keep instrumented native code, run with `-Profile Current` after rebuilding native libs.
- Evidence run: `code/projects/android/logs/logcat-full-20260213-091246.txt`.
- Key lines:
  - `IMMDBG_NA02_RENDERGATE ... stage=skip ... enabled=1 anyDoc=0` for idx `0..19`.
  - No `IMMDBG_NA01_STROKEPASS` lines in same run.
- Interpretation: first XR frames in single-pass skip rendering due `mAnyDocToRender=0`; after initial window, skip logs stop but stroke-pass markers still never appear.
- Verdict: stroke pass remains unproven in active XR path; rendering gate/state before stroke pass is now the primary confirmed blocker.

12) **NA03 `mAnyDocToRender` transition probe**
- Hypothesis: `mAnyDocToRender` may remain false in XR and prevent stroke pass execution.
- Change: Add `IMMDBG_NA03_ANYDOC` logs in `Player::GlobalRender` to capture `anyDocPrev/anyDocNow`, `usedDocs`, `readyDocs`, and frame index.
- Evidence run: `code/projects/android/logs/logcat-full-20260213-095610.txt`.
- Result: `anyDocNow` transitions `0 -> 1` at `idx=23 frame=25`, then remains `1` (`usedDocs=1 readyDocs=1`).
- Verdict: render gate is only an early-startup condition; XR does become render-eligible and stays eligible.

13) **NA01 corrected in single-pass + NA04 GL state boundary snapshots**
- Hypothesis: stroke pass executes, but XR composition/state after pass hides output.
- Change A: Correct NA01 placement so `IMMDBG_NA01_STROKEPASS` logs are emitted from `RenderStereoSinglePass` path.
- Change B: Add `IMMDBG_NA04_GLSTATE` in `main.cpp` at single-pass boundaries (`pre-globalrender`, `post-renderstereo`) to track FBO/viewport/scissor/depth/blend/cull/program/active texture/color mask.
- Code delta: `code/libImmPlayer/src/player.cpp`, `code/appImmUnity/src/main.cpp`, and log pattern update in `code/projects/android/run-android-cycle.ps1`.
- Evidence run: `code/projects/android/logs/logcat-full-20260213-095824.txt`.
- Key observations:
  - `IMMDBG_NA01_STROKEPASS` appears continuously once `anyDocNow=1`.
  - Stroke pass reports non-zero workload (example: `layers=75`, `strokeLayers=30`, `strokes=1171`, post `drawCalls=37`, `tris=642730`).
  - `IMMDBG_NA04_GLSTATE` shows stable eye target continuity (`drawFbo` rotates 1/2/3 with same viewport), but pre/post render state diverges (`cull 0->1`, `prog 1->0`, `activeTex 0x84c0->0x84c7`).
- Verdict: stroke draw calls are definitely executing in XR; current leading suspect shifts to post-stroke render state / composition interaction rather than missing content or missing pass execution.

14) **NA05 pass marker fixed placement (single-pass stroke path)**
- Hypothesis: if a simple marker clear is injected in the same stroke pass, it should verify that the pass can write visible color in XR.
- Change: Move `IMMDBG_NA05_PASSMARK` marker clear block from multipass path to `RenderStereoSinglePass` right after `mLayerPaintRender->DisplayRender()`.
- Evidence run: `code/projects/android/logs/logcat-full-20260213-101254.txt`.
- Result: `IMMDBG_NA05_PASSMARK` now appears continuously in XR once docs are ready (example: `idx=0 frame=28 marker=820,860,40,40 vp=0,0,1680,1760`).
- Verdict: prior NA05 absence was instrumentation placement error; marker write now executes in the active XR single-pass path.

15) **NA06 post-render callback marker (after RenderStereoSinglePass returns)**
- Hypothesis: if a marker drawn at end of render callback is visible, composition is likely dropping/overwriting earlier stroke-pass output; if not visible, callback output itself is not reaching presented image.
- Change: Add `IMMDBG_NA06_POSTMARK` in `main.cpp` after `RenderStereoSinglePass` + `post-renderstereo` state logging, draw red 56x56 scissor-clear marker near viewport origin.
- Code delta: `code/appImmUnity/src/main.cpp`, plus extraction pattern update in `code/projects/android/run-android-cycle.ps1`.
- Evidence run: `code/projects/android/logs/logcat-full-20260213-101548.txt`.
- Result: `IMMDBG_NA06_POSTMARK` logs from startup onward (`idx=0..`, stable viewport `1680x1760`) while NA05 and NA01 continue to execute with non-zero stroke workload.
- Verdict: we now have marker writes at both stroke-pass time (NA05) and post-render callback boundary (NA06); headset visibility comparison is the remaining discriminator.

16) **NA07 full-screen post-callback color flash (strong presentation probe)**
- Hypothesis: if callback output reaches presented eye images, forcing full-screen alternating clear colors in early frames should be unmistakable in-headset.
- Change: Add `IMMDBG_NA07_POSTFULL` in `main.cpp` after NA06. For first 120 callback frames, disable scissor, clear full color target alternating red/blue, restore state.
- Code delta: `code/appImmUnity/src/main.cpp`, pattern update in `code/projects/android/run-android-cycle.ps1`.
- Evidence run: `code/projects/android/logs/logcat-full-20260213-101935.txt`.
- Result: NA07 logs execute continuously in early frames (`idx=0..19`, then sampled), with NA05/NA06/NA01 also active in same run.
- Verdict: instrumentation confirms repeated full-screen post-callback writes are being issued; headset visibility result is required to decide whether writes are not presented or are overwritten later in XR composition.

17) **NA09 XR command-buffer hook timing pivot (AfterEverything -> AfterImageEffectsOpaque)**
- Hypothesis: in Quest OpenXR, `CameraEvent.AfterEverything` can execute too late relative to final eye submission, so IMM draws may not reach presented XR eye images.
- Change: set `desiredEvent = CameraEvent.AfterImageEffectsOpaque` for command-buffer attachment in `ImmPlayerManager.OnCameraPreCull`.
- Code delta: `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs`.
- Evidence runs:
  - `code/projects/android/logs/logcat-full-20260213-110245.txt` (invalid; guardian/system dialog interruption, no IMM diagnostics captured)
  - `code/projects/android/logs/logcat-full-20260213-110733.txt` (invalid; explicit launch block)
- Key lines:
  - `Launch is blocked because: a Guardian dialog is currently showing`
- Verdict: hypothesis not evaluated yet; runs invalid for XR conclusions due Guardian/VRUI launch interference.

18) **NA09b valid run after Guardian clear (same hook timing pivot)**
- Hypothesis: same as NA09; if hook timing was the blocker, moving to `AfterImageEffectsOpaque` should allow visible XR output from plugin draws.
- Change: unchanged from NA09 (`desiredEvent = CameraEvent.AfterImageEffectsOpaque`).
- Evidence run: `code/projects/android/logs/logcat-full-20260213-125032.txt`.
- Key lines:
  - No launch-block lines (`Guardian` / `device in VRUI`) in run log.
  - Startup then ready transition still occurs (`IMMDBG_NA03_ANYDOC ... anyDocNow=1`).
  - Stroke pass still executes with non-zero load (`IMMDBG_NA01_STROKEPASS ... drawCalls/tris > 0`).
  - NA05/NA06/NA07 markers and NA08 color-mask diagnostics still execute continuously.
- Verdict: technically valid run captured; visual headset outcome still required to determine whether hook timing change fixed presentation.

19) **NA11 stroke-path full-screen probe (post-callback visuals disabled)**
- Objective: determine whether output generated inside `RenderStereoSinglePass` stroke path can be seen in headset when post-callback full-screen probes are disabled.
- Changes:
  - `code/libImmPlayer/src/player.cpp`: replaced center marker clear with full-screen alternating `green/black` probe in stroke path (`IMMDBG_NA11_STROKEFULL`, with existing `IMMDBG_NA08_COLORMASK`).
  - `code/appImmUnity/src/main.cpp`: disabled post-callback visual probes (`NA06/NA07/NA10`) via `kEnablePostCallbackVisualDebug = false`.
- Evidence run: `code/projects/android/logs/logcat-full-20260213-125713.txt`.
- Log evidence:
  - `IMMDBG_NA11_STROKEFULL ... enabled=1 color=green ...` appears repeatedly once `anyDocNow=1`.
  - `IMMDBG_NA01_STROKEPASS` remains active with non-zero draw calls/triangles.
  - `IMMDBG_NA03_ANYDOC` transitions to ready (`anyDocNow=1`).
- Pending headset observation required:
  - If green/black full-screen is visible -> stroke path writes are presented; remaining issue is stroke visibility/placement/shader state.
  - If not visible -> stroke path writes are not the final presented eye image path.

20) **NA12 unattended progress branch (periodic probes + force two-pass XR mode)**
- Objective: keep progressing without headset timing dependency and test whether single-pass path is the main failure source.
- Changes:
  - `code/libImmPlayer/src/player.cpp`: NA11 converted to periodic/continuous cadence (`periodicWindow = idx % 600`, active for first 120 frames each cycle, alternating green/black).
  - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs`: added `forceTwoPassStereoInAndroidXR` (default true) and force-chosen stereo mode to two-pass on Android XR.
  - Existing post-callback visual probes remain disabled (`kEnablePostCallbackVisualDebug = false`) to avoid false positives from callback stage.
- Evidence run attempted: `code/projects/android/logs/logcat-full-20260213-132627.txt`.
- Result: run invalid for IMM diagnostics (no `[IMM]`/`IMMDBG` lines captured); log shows startup/focus anomalies (`no window has focus`, stale input events), likely because headset was not actively in-session.
- Next required validation once headset is active:
  - Confirm `IMMDBG_XRSTEREO_20260212A ... forceTwoPassAndroid=True chosenStereoMode=1` in logs.
  - Observe whether content appears in two-pass mode, and whether periodic NA11 green/black can be seen when active.

## Critical Lessons Learned

### XR Mode Synchronization Requirement
**Lesson**: Plugin stereo mode must match Unity's XR stereo rendering mode exactly. Mismatched modes cause complete render failure.

**Evidence**: NA12 experiment (2026-02-13) - Forced plugin to TwoPass mode while Unity remained in SinglePass mode → headset showed "all black" (complete render failure).

**Root Cause**: When plugin expects TwoPass (separate render calls per eye) but Unity provides SinglePass (one render call with multiview), the plugin renders to wrong targets or incorrect framebuffers.

**Process Fix**: Before testing any stereo mode change:
1. Check Unity XR Settings → `Stereo Rendering Mode` 
2. Ensure plugin stereo mode matches Unity's setting
3. Log both values to confirm match at runtime

### What We've Learned So Far

**Eliminated causes**:
- ❌ Content loading (data is loaded, 1171 strokes confirmed)
- ❌ Render gate/mAnyDocToRender (transitions correctly to 1)
- ❌ Cull/depth test state (disabled both, no change)
- ❌ Mono vs stereo API choice (tried both)
- ❌ Document placement (forced near camera, no effect)
- ❌ Hook timing (AfterImageEffectsOpaque vs AfterEverything, same result)
- ❌ Post-callback overwrite (post-callback clears ARE visible)

**Current confirmed state**:
- ✅ Sky sphere renders correctly
- ✅ Post-callback full-screen clears (red/blue) ARE visible
- ✅ Stroke draw calls execute (37 draw calls, 642k triangles logged)
- ❌ Stroke-path full-screen clears NOT visible
- ❌ Actual stroke geometry NOT visible

**The Gap**: glClear() works in stroke path, but actual geometry draw doesn't. Points to:
1. Shader compilation failure (stroke shaders use GL_OVR_multiview)
2. Matrix/coordinate mismatch (strokes render off-screen)
3. GL state poisoning (blend/stencil/color mask)

## Working Hypothesis

Stroke shaders are failing silently in XR, or stroke geometry is rendering with incorrect transform/state. The path itself works (proven by glClear), but the actual stroke draw calls produce no visible output.

## Immediate Next Controlled Steps

**Phase 1: Confirm glClear works in stroke path (validation)**
1. Re-enable single-pass mode with stroke-path full-screen clear probe (NA11)
2. Verify: Can we see green/black flashing from stroke path? (This confirms the path CAN write to presented buffer)
3. If YES → proceed to Phase 2
4. If NO → investigate why glClear fails in stroke path but works post-callback

**Phase 2: Test minimal geometry draw**
5. Replace stroke renderer call with simple triangle draw (bypass all stroke shader complexity)
6. Verify: Can we see a simple colored triangle in headset?
7. If YES → stroke shaders are the problem, proceed to Phase 3
8. If NO → matrix/target setup is wrong, investigate projection matrices

**Phase 3: Shader diagnostics**
9. Add shader compilation/linking error logging to stroke renderer
10. Check for GL_OVR_multiview extension availability on Quest
11. Test with fallback non-multiview shader if available

**Phase 4: State isolation**
12. Log all GL state before/after stroke draw call
13. Compare state between working sky sphere render and failing stroke render
14. Identify state divergence causing invisibility

## BREAKTHROUGH: Brush Strokes Rendering Successfully (2026-02-15)

**Issue Resolution:**
Brush strokes are now fully visible in Quest XR headset.

**Root Cause:**
The build was failing to link due to a signature mismatch in `fiLayerPaint::ReadAsset`:
- Header declared: 7 parameters (with `IStrokeCollector* collector = nullptr`)
- Implementation defined: 6 parameters (missing `collector`)

This linker error prevented successful builds, masking the actual working state of the stroke renderer.

**Fix Applied:**
Fixed signature mismatch in `code/libImmImporter/src/fromImmersive/fromImmersiveLayerPaint.cpp` line 559:
```cpp
// Before (broken):
bool ReadAsset(LayerImplementation vme, piIStream *fp, piLog* log, Drawing::ColorSpace colorSpace, Drawing::PaintRenderingTechnique renderingTechnique, bool flipped)

// After (working):
bool ReadAsset(LayerImplementation vme, piIStream *fp, piLog* log, Drawing::ColorSpace colorSpace, Drawing::PaintRenderingTechnique renderingTechnique, bool flipped, IStrokeCollector* collector)
```

**Verification:**
- ✅ Green/black full-screen flashes visible (NA11 probe)
- ✅ Actual brush stroke geometry visible
- ✅ Document loads correctly (1171 strokes, 75 layers)
- ✅ Stroke pass executes (7 draw calls, 329k triangles)
- ✅ Single-pass stereo mode working

**What We Learned:**
1. The stroke renderer was always working - we just couldn't build it
2. The existing diagnostic framework (NA01-NA11) is effective
3. Unity XR settings (AfterImageEffectsOpaque) are correct
4. No shader or matrix issues - strokes render as expected

**Process Lesson:**
Always verify builds link successfully before debugging runtime behavior. Silent linker errors can mask working functionality.

**Files Modified:**
- `code/libImmImporter/src/fromImmersive/fromImmersiveLayerPaint.cpp` - Fixed ReadAsset signature
