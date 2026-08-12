# Validation False-Failure Cleanup Plan

## Status

This is the authoritative active plan for validation integrity and the
remaining Unity Android Vulkan stereo work. The earlier
`ANDROID_UNITY_VULKAN_FORK_INTEGRATION_PLAN.md` is a completed historical record,
not a second active implementation plan.

Run `30913083187` cloud-confirmed the Godot Vulkan/Metal depth-composition
repairs, but it did not permanently close the CI-green workstream. Run
`31497084250` demonstrated that the hosted workflow could still be red because
of over-tight visual thresholds, missing evidence, unsupported hosted API
lanes, and Firebase execution failures. Commit `b82d6582` repaired those local
validation defects. Confirmation run `31501318696` passed every non-Firebase
canonical rendering lane, including macOS Godot Metal, Windows Godot Vulkan,
Unity macOS Metal, Unity Windows DirectX, and the GPU evidence report. Its only
root failures were the four Android Firebase lanes, all of which failed before
app launch with `Firebase Test Lab infrastructure failure: Internal System Error
3` on both attempts during Google's declared 11 August 2026 Test Lab
availability incident. The dependent engine, device, and final evidence reports
correctly remained red because no Android visual evidence was produced.
Run `31509841891` then cloud-built and packaged the first reproducible Unity 6
OpenXR/Vulkan Quest APK from the exact revision. The Quest shell cache,
same-commit native-plugin injection, signing, and XR manifest contract all
passed. Its Firebase Unity Android lane also returned complete visually correct
render, composition, external-screen, and distinct synthetic-eye evidence.
Physical two-eye Quest output remains the acceptance test.

That run also exposed two further false negatives in visually correct macOS
composition images. Unity ordered-overlay and Godot full-depth were rejected
because the lower red IMM brush was measured as one connected color component.
That is not stable for IMM's alpha-cut/MSAA/stippled rendering: the same correct
content varied from `0.006293` to `0.000295` between consecutive cloud runs.
The lower-red presence probe now measures total matching pixels in its region,
with reviewed floors below both correct captures. Cyan depth leakage continues
to use the connected-component maximum, so the occlusion detector is unchanged.
The Android standalone Vulkan lane still failed closed with no capture during
the ongoing Test Lab incident; it is not a produced visual regression.

Run `31514181525` cloud-confirmed the fragmented-brush correction: both macOS
Unity Metal and macOS Godot Metal passed their complete visual contracts, as
did Unity Android Vulkan and every other hosted engine/GPU rendering lane.
Android standalone Vulkan again produced no capture after waiting for Firebase,
so it remained red without evidence of a bad frame. The run also exposed a
report-scope defect: reusable hosted evidence reports inherited the verifier's
hardware-only default, causing the engine report to demand a skipped
self-hosted Windows Unity Vulkan row while ignoring the hosted rows it was
supposed to gate. Device, engine, and GPU reports now receive an explicit
`hosted` scope in normal validation and `all-supported` only in hardware mode.
The standalone GLES/Vulkan lanes now classify complete visual evidence
directly; a valid image can outrank redundant missing log markers, while a
Firebase failure with no image remains red as `infrastructure` or `evidence`
instead of being hard-coded as `visual`.

Run `31518237544` confirmed that the explicit hosted scope selects the intended
engine rows, and in doing so exposed a stale support declaration: Windows Unity
Vulkan synthetic stereo was marked as a supported hosted visual lane even
though Unity consistently rejects the hosted Lavapipe device and the lane's
authoritative manifest says `skipped`. That row is now `deferred` and remains a
runtime diagnostic only. It cannot claim a Vulkan pass, and it no longer makes
normal hosted validation fail for the known lack of an accepted Windows Vulkan
device. Unity Android Vulkan remains the supported hosted synthetic-stereo
visual path and passed in the same run. Android standalone Vulkan reached its
outer 30-minute Firebase step timeout without a capture or completed Firebase
summary; the new classifier reported `evidence_incomplete` rather than a visual
failure, and the device/final reports correctly remained red.

Run `31523497710` cloud-confirmed the completed false-failure cleanup. Every
produced canonical visual lane passed, including Unity Android Vulkan render,
composition, and synthetic stereo; Unity and Godot macOS Metal; Windows Godot
Vulkan; DirectX; Web/WASM; Android GLES; and Android Godot Vulkan. The engine,
GPU, and core evidence reports also passed. The sole remaining red lane was
Android standalone Vulkan: Firebase reached the outer 30-minute timeout without
returning a result or capture during Google's ongoing Test Lab incident. It was
correctly reported as `evidence_incomplete`, not as a rendering failure.

Run `31527970425` cloud-compiled the eye-authoritative Quest presentation
iteration and produced an exact-revision Unity 6 OpenXR/Vulkan Quest APK. Its
non-XR Unity Android Vulkan Firebase lane passed render, composition, external
screen, and distinct synthetic-eye validation; manual inspection agrees with
those verdicts. All other hosted canonical visual and evidence jobs passed.
Android standalone Vulkan was again the sole red lane after its Firebase step
timed out at 30 minutes without a result or capture, and it again reported
`evidence_incomplete`. The exact APK from this run was then installed on a
physical Quest 3S and manually accepted: both displayed eyes contain the correct
IMM rendering. This closes the Quest Unity Vulkan Multi Pass product defect.

No product-rendering defect is currently confirmed in the supported hosted
matrix or the accepted Quest Multi Pass path. macOS Godot Metal was a validation
false negative, not a renderer defect; run `31501318696` confirms the reviewed
threshold correction. A rerun of Android standalone Vulkan in run `31527970425`
again timed out after 30 minutes without a Firebase result or capture. Its lane
manifest correctly says `evidence_incomplete`, but the combined report incorrectly
rewrote the missing-capture metrics error as `render_failed`. The report now gives
typed lane failures precedence over generic metric errors, with a regression test
covering this exact missing-Firebase-capture case. Firebase evidence must still
remain red until a real image passes. Single Pass Instanced and iOS remain
separate future product/coverage workstreams.

## Objective

Make the validation report trustworthy in both directions: correct renders must not fail for irrelevant reasons, and broken renders must never pass because a visual contract is missing or too weak. Visual evidence remains authoritative. Text and log checks may fail early, but must not reject a visually proven render merely because a redundant diagnostic message was absent, and they can never substitute for a successful visual rendering check.

The hosted validation workflow must finish green when every supported hosted
lane has authoritative passing evidence. A platform outage or missing required
capture must continue to fail closed, but should be retried and reported as an
infrastructure/evidence failure rather than mislabeled as a visual regression.
An intentionally unsupported diagnostic lane must verify the expected rejection
without making the entire workflow permanently red; it must remain visibly
`not tested` or non-canonical in the report and cannot contribute a visual pass.

The cyan depth/occlusion contract distinguishes genuinely foreground cyan
inside the character silhouette from the intentionally visible rim of a rear
square and authored cyan scene details. The historical broken Windows Godot
ordered-overlay capture remains a negative fixture because cyan fills the
character interior; the current Windows Godot lane and reviewed Unity Metal and
Android full-depth captures pass.

The combined report starts with a four-by-three visual matrix whose rows name
platforms only; the canonical renderer is implicit and selected per cell.
Windows standalone and Unity use DX11, while Windows Godot uses Vulkan. Android
uses Vulkan and macOS/iOS use Metal. Status cells contain color only: green is a
passed standalone render or visually passed engine depth composition, yellow is
a visually passed engine render whose required depth path is absent (and is
therefore still CI-failing for Godot/Unity), red is a produced rendering or
attempted depth-composition failure, and gray is genuinely untested or out of
scope. Godot and Unity can never become green from render-only evidence, and
both yellow and gray engine cells remain required validation gaps.

The Android Unity lane's optional `ffmpeg` installation is bounded to five
minutes, with two-minute package-manager command limits. This prevents a
runner package-manager stall from consuming the entire one-hour device job
before Firebase validation begins.

## Audit chronology

### Audit chronology beginning with run `30901787140`

Run `30901787140` at `3e776a57` is the first cloud run in which the complete Unity Android Vulkan lane passes and its images survive manual review:

- render-only, composition, and physical-screen captures contain the expected IMM scene;
- both presented synthetic eyes contain complete, distinct views with real parallax;
- the presented pair matches the independently captured native-eye pair;
- the full-depth cyan probe is correctly occluded by the character apart from its exposed rim;
- the external-video validator selects two consecutive valid frames at two-second cadence.

This is strong cloud evidence for the native Multi Pass eye-matrix fix and the validation presenter fix. It is not a substitute for the final Quest/OpenXR compositor test, which still requires both physical eyes to be inspected.

The same run exposed two remaining false report signals. They are now fixed locally:

1. A passed build-only manifest with no strict image was converted into `failed` and overrode a sibling passing visual manifest. Build-only evidence is now `evidence_incomplete`: it still fails closed when it is the only evidence, but cannot override an authoritative visual pass. This corrects the false `unity/windows/non-vr/directx` matrix result and distinguishes the hardware-gated Windows Vulkan evidence gap from a produced bad image.
2. Unity Metal full-depth contained only a small renderer-dependent cyan edge component (`0.000207` of the crop), but the limit was `0.000150`. The reviewed image is correctly occluded. The limit is now `0.000500`; the known-bad Windows Godot overlay remains rejected at `0.003597`, more than seven times the revised maximum. Positive edge-noise fixtures and the existing foreground-cyan negative fixture protect both sides of the boundary.

After those corrections, the remaining visible failures in `30901787140` are genuine or explicit coverage limitations:

- Unity Metal ordered-overlay: magenta and yellow probes are absent. Source tracing found a fixture bug: ordered-overlay setup set the base camera culling mask to zero, then created all three probes on layer 0, which neither the base camera nor the later layer-30 overlay camera rendered. The base camera now reserves layer 29 for the composition probes while the later overlay remains isolated on layer 30; cloud confirmation is pending.
- Windows Godot full-depth: only the sky/background is present;
- Windows Godot ordered-overlay: the cyan rear square is drawn over the character. The fixture applied the transparent/no-depth-test ordered-overlay material to all three probes, so cyan could never be occluded. The corrected fixture keeps cyan opaque before the `PRE_TRANSPARENT` IMM callback while magenta/yellow remain transparent after it; this tests both sides of the ordering boundary without the previously broken `POST_TRANSPARENT` strategy. Cloud confirmation is pending.
- macOS Godot Metal: its current color-intermediate integration has no host
  depth input, so it could not satisfy the required depth-composition
  contract. The Metal wrapper now attaches a normal-Z intermediate depth
  texture alongside its existing color intermediate, stores IMM depth, and
  feeds both textures through the same RenderingDevice compositor used by the
  Vulkan path. Cloud Metal visual confirmation is pending.
- hosted Windows Unity Vulkan synthetic stereo: Unity rejects Lavapipe Vulkan and falls back to Direct3D, so no Vulkan image exists;
- Windows Unity Vulkan full-depth/ordered-overlay: hardware-gated jobs did not run, so the non-VR Vulkan row has incomplete evidence rather than a visual failure.

Run `30907547626` at `18b22b41` provides the next audited job-level results:

- Unity macOS Metal render-only, full-depth, ordered-overlay, and Editor Play
  captures all pass and survive manual review. The layer-29 composition-probe
  fixture repair is confirmed.
- Windows Godot Vulkan ordered-overlay now passes and is visually correct. The
  opaque-before/transparent-after probe split is confirmed.
- Windows Godot Vulkan full-depth now contains IMM strokes and all three
  composition probes, confirming the normal-Z clear fix, but host color is
  black wherever the depth compositor discards. The full-depth callback ran at
  `POST_TRANSPARENT`, where Godot may treat the final color attachment as
  discardable. Composition modes now run at `PRE_TRANSPARENT`, with resolved
  color/depth access declared, so the host attachment remains live while the
  depth-select shader preserves rejected pixels. Cloud confirmation is pending.
- The ordinary Godot Run-project capture is visually correct. Its localized
  character correlation was `0.760` against a `0.800` minimum, with mean
  absolute delta `0.058` against `0.060`; this is normal stipple/renderer drift,
  not the prior reverse-Z defect. The reviewed tolerance is now correlation
  `>= 0.700` and mean absolute delta `<= 0.080`, while a generated foreground
  depth-corruption fixture must still fail the same localized region.
- macOS Godot Metal still produces correct render-only output but no scene
  probes in full-depth on the pushed revision. The pending Metal depth
  attachment/render-graph composite change is the next cloud experiment.

Run `30909845212` at `dc7a99d4` confirms that the ordered-overlay repair is
visually correct on Windows Godot Vulkan: the IMM scene and all probes are
present, and cyan leakage in the character occlusion region is zero. The job
nevertheless failed because its GDScript fast-fail check was inverted and
required cyan in the region that the visual contract correctly requires to be
occluded. Ordered-overlay and full-depth now use the same rear-occlusion probe
condition; logs can no longer reject the correct image for the opposite rule.

The same run isolates the remaining shared Godot full-depth defect on both
Windows Vulkan and macOS Metal. IMM geometry and the depth probes render, but
the IMM far-depth 360 background is absent and rejected host pixels become
black. The compositor now performs depth selection into a separate complete
color target, writing either IMM or preserved Godot color for every pixel,
then copies that merged target back to Godot color. It identifies untouched IMM
pixels by transparent color rather than rejecting depth 1.0, because the 360
background is legitimate far-plane content. A local Windows Vulkan capture
contains the complete scene and correct occlusion, passes the authoritative
`sample1-full-depth` comparator at correlation `0.935`, and passes the pixel
probes. The following run records its cloud confirmation.

Run `30913083187` at `1687ce73` cloud-confirms the two-pass correction on both
backends. Windows Godot Vulkan and macOS Godot Metal full-depth images contain
the complete IMM scene and 360 background, preserve Godot host color, show the
magenta/yellow probes, and correctly occlude cyan inside the character. Both
strict full-depth metric files and both lane classifiers pass, and manual image
review agrees. The final visual matrix is green for every currently canonical
Windows, Android, and macOS product cell; iOS remains gray because those product
surfaces are not implemented/tested. Standalone passes are green because depth
does not apply to standalone. The workflow remains red only for the two
non-canonical hosted Windows Unity Vulkan rows: Unity rejects Lavapipe Vulkan,
so the non-VR row has incomplete evidence and the synthetic-stereo row has a
genuine requested-API runtime failure. Those are coverage/runtime limitations,
not false visual failures in a produced image.

Run `30913083187` supplied the required cloud confirmation. The later physical
Quest 3S test of run `31527970425` supplied the required Multi Pass acceptance.
The validated cross-platform suite remains the regression gate for later stereo
expansion.

### Current priority order

1. Keep failure classification watertight. The rerun of Android standalone
   Vulkan in run `31527970425` exposed an aggregate-report precedence bug: the
   authoritative lane manifest classified the absent Firebase result/capture as
   `evidence`, while generic missing-capture metrics caused the combined report
   to print `render_failed`/`rendering`. The report fix and regression test must
   preserve `evidence_incomplete`/`evidence` through aggregation. Missing
   evidence remains a red required lane; only its explanation changes.
2. Rerun Android standalone Vulkan after Google's 11 August 2026 Test Lab
   availability incident is resolved. Do not weaken or skip its required visual
   checks: run `31523497710` already cloud-confirmed the evidence-scope and lane
   classification corrections, and both attempts in run `31527970425` again
   exhausted the 30-minute execution window without returning a result or
   `native-render-after.ppm`. That missing evidence must remain red until a real
   passing capture exists.
3. Download and manually inspect the complete exact-revision evidence report
   after Firebase can execute the standalone APK. Preserve the now-confirmed Godot
   Vulkan/Metal depth path, Unity/Godot composition repairs, Android non-XR
   Vulkan, OpenGL, Metal, DirectX, Windows Vulkan, and WASM behavior. The full
   supported hosted workflow must be green; a green aggregate produced by
   suppressing missing visual evidence is not acceptable.
4. Add Single Pass Instanced as a later product extension now that Multi Pass
   passes on Quest. Reuse the same two-eye producer and stereo-aware presentation
   design; true Vulkan multiview remains an optional later optimization.
5. Track iOS accurately: add a Unity iOS Metal visual-validation lane as a
   coverage task, and treat Godot iOS as a product implementation task before
   adding its visual-validation lane.

### Run `31501318696` confirmation

1. macOS Godot Metal passed its render-only, full-depth, and ordered-overlay
   visual contracts. The prior lower-red-brush rejection was a threshold false
   negative and is closed.
2. Windows Godot Vulkan passed, including the ordinary Run-button path and the
   strict depth/ordering evidence. The fail-closed retry remains available but
   did not convert missing or failed visual evidence into a pass.
3. Unity macOS Metal and Unity Windows DirectX composition passed. The sealed
   macOS Editor Play evidence survived collection and was accepted only with
   passing image metrics.
4. Hosted Windows Unity Vulkan synthetic stereo reported the exact expected
   Lavapipe rejection as `skipped`/`not_tested`; it did not claim a Vulkan visual
   pass. Hardware-gated Windows Vulkan composition rows remained skipped.
5. Unity Android Vulkan, Android Godot Vulkan, Android standalone Vulkan, and
   Android standalone GLES each created two Firebase matrices. Every matrix
   failed before app launch with `Internal System Error 3`; no Android capture
   was produced. Firebase's status dashboard reported an ongoing, broadly
   affecting Test Lab availability incident on the same date.
6. The engine, device, and combined evidence reports correctly failed because
   their required Android evidence was absent. This is a trustworthy red signal
   for unavailable validation coverage, not evidence that those four renderers
   produced bad frames.

### Run `31497084250` remediation in progress

1. The reviewed macOS Godot Metal full-depth capture passes after changing only
   the lower-red-brush presence floor from `0.001000` to `0.000750`. Its measured
   share is `0.000949`; the spatial comparison and the independent
   magenta/yellow/cyan composition probes remain unchanged. The black/missing
   content negative fixture still fails.
2. Firebase returned `Internal System Error 3` before app launch in all four
   Android lanes. The immediate retry reproduced the same outage. Infrastructure
   retries now wait 90 seconds before submitting a fresh matrix; missing device
   captures still fail closed.
3. The hosted Windows Unity Vulkan synthetic lane now accepts only the exact
   proven rejection sequence (`Forcing GfxDevice: Vulkan`, `Vulkan detection:
   0`, then the expected-Vulkan/actual-D3D11 marker) as `skipped`/`not_tested`.
   It cannot produce a visual pass. Any other runtime or evidence failure stays
   red.
4. The macOS Unity Editor Play image and passing metrics are sealed immediately
   after creation so a later Unity lifecycle cleanup cannot remove the only
   authoritative project-Play evidence. The classifier accepts the sealed copy
   but still requires both the image and its passing visual metrics.
5. The Windows Godot full-depth smoke receives one retry after a native process
   failure. The current run showed the same hosted-runner crash during the
   preliminary project launch and then rendered correctly on the next launch.
   The retry is fail-closed: the lane passes only if one invocation produces the
   complete capture and all visual contracts pass.

Validation reliability is a gate, not a background cleanup task. Product fixes must retain full cross-platform validation so a narrowly targeted Android Vulkan change cannot silently regress an already working target.

### Unity Quest Vulkan stereo facts

- The Unity sample is configured for Multi Pass (`m_StereoRenderingPath: 0`). This is the mode the IMM Unity plugin expects for Quest Vulkan.
- CI builds a dedicated `ImmUnityQuestVulkan.apk` from the VR scene without
  stripping OpenXR. It verifies the Quest VR intent category, head-tracking
  feature, and supported-device declarations, then includes the APK in the
  Unity Android artifact. This proves build/package configuration only; both
  physical eyes must still be inspected on Quest.
- The plugin explicitly does not support instanced single-pass and forces a stereo camera to its `TwoPass` integration path, except for its legacy `SinglePass` mode.
- The pre-fix real Quest result was a product failure in that supported path: the left eye rendered IMM strokes correctly while the right eye showed only Unity's background sky sphere.
- Firebase synthetic stereo is not yet a Quest-equivalent test. It manually invokes `Camera.Render()` twice and supplies the eye index, so it verifies two target writes and capture plumbing but bypasses Unity's real XR Multi Pass callback/eye-target/composite handoff.
- The Firebase synthetic lane now passes with distinct, independently valid native and presented eye pairs. Use it as evidence that native eye production and the flat validation presenter are correct, but not as proof that the Quest/OpenXR compositor displays both eyes.
- Fork commit `52f3e462` exposed a concrete native Multi Pass defect: Vulkan paint shaders index `mEye[pass.mID]`, but `RenderStereoMultiPass` only populated slot 0. The equivalent fix is now on `main` as `df970cd0`; each one-eye call copies the current eye matrix into both shader slots so the paint and picture shader variants consume the same current-eye transform. CI rebuilds the Android native plugin from that revision and injects it into the APK, so this is not merely a source-only change.
- Run `30899560536` isolates the handoff. Both native eye RenderTextures independently pass the approved render baseline and differ by `697101` pixels, while the presented pair is byte-identical (`0` changed pixels) and shows eye 0 twice. This cloud-proves that the rebuilt native Multi Pass patch produces correct distinct left/right IMM renders; the failure is downstream in the flat synthetic presentation fixture.
- Source tracing found that downstream bug: the flat Android Vulkan `OnRenderImage` presenter always sampled `VulkanEyeTargets[0]`, including during the explicit eye-1 synthetic render. The validation-only override now selects `VulkanEyeTargets[presentationEye]`, and its log contract requires each presented target ID and native pointer to equal the corresponding dispatched eye target. This does not alter ordinary mono Android presentation or the real stereo XR path.
- Other VR targets already render correctly. Do not replace or broadly redesign their stereo integration. The additional offscreen-render-and-present mechanism is specific to Android Vulkan because Unity's Android Vulkan display render buffer is not reliably accessible to the native plugin.
- Commit `81e308f9` replaces the mutable Quest eye handoff. It registers both
  Android Vulkan eye targets together, records both native eye events in one
  frame-correlated command sequence, binds both completed eye textures, and
  selects the presentation texture in the Unity shader using the GPU stereo eye
  index. CPU-side `stereoActiveEye` in `OnPreCull` is no longer authoritative.
- The Unity 6 Android CI player compiled and packaged that change successfully
  in run `31497084250`. Its Firebase lane is non-XR and can prove that 2D Android
  Vulkan did not regress, but only a physical Quest test can accept the two-eye
  OpenXR handoff.
- The next Quest iteration removes the GPU-indexed scene quad from the default
  Multi Pass presentation path. The fork kept that quad opt-in after observing
  per-eye flicker, and the physical symptom remains at the final handoff. Unity
  explicitly guarantees that `Camera.stereoActiveEye` identifies the current
  eye during `OnRenderImage`; the Android Vulkan presenter now uses that
  supported final-image callback to choose `VulkanEyeTargets[0]` or `[1]` and
  composite it into Unity's own per-eye destination. The paired native producer
  is unchanged, and a kill switch retains the previous presentation path for
  device A/B diagnosis. Run `31527970425` passed Unity 6 compilation and the
  non-XR Android Vulkan visual regressions. The exact APK from that run was then
  installed on a Quest 3S and both displayed eyes were manually accepted.
- Multi Pass remains the first acceptance target. The implementation must nevertheless carry both eye results together so Single Pass Instanced can use the same producer and presentation path later.

### Unity Android Vulkan stereo implementation

#### Phase A: isolate the failing handoff on Quest

1. Add uniquely prefixed, frame-correlated diagnostics for both eyes: uploaded matrix translation, event ID, native target pointer, native render completion, and presentation source.
2. Capture each native Android Vulkan eye RenderTexture before Unity presentation.
3. Classify the failure from image evidence:
   - right native eye contains IMM but the displayed right eye does not: repair the Unity presentation step;
   - right native eye is empty: repair native event/target/matrix routing before presentation;
   - both native eyes contain identical views: repair matrix upload or eye indexing.
4. Treat logs only as fast-fail evidence. A fix is accepted only when both displayed Quest eyes contain the expected IMM view.

#### Phase B: replace mutable per-eye callback state

Implementation status: paired production landed in `81e308f9`; Unity 6 cloud
compilation passed. Eye-authoritative `OnRenderImage` presentation landed in
`53c83170` and passed cloud compilation plus the non-XR Android Vulkan visual
suite. The exact-revision APK from run `31527970425` passed physical Quest 3S
two-eye acceptance.

1. Obtain both current XR eye view/projection matrices once per frame.
2. Maintain two Android Vulkan offscreen eye targets initially. Do not alter the direct-rendering paths used by working graphics APIs.
3. Enqueue both native eye renders in deterministic order in one immutable frame command sequence, or in two immutable eye sequences whose contents cannot be overwritten by the other eye's callback.
4. Bind both completed eye targets to the presentation material.
5. In Multi Pass, present through `OnRenderImage`, where Unity documents
   `Camera.stereoActiveEye` as the currently rendering eye. Select the matching
   immutable eye target there; do not select an eye from `OnPreCull`. Retain a
   GPU eye-indexed shader path for the later Single Pass Instanced phase.
6. Preserve IMM's actual visibility model: alpha cut/discard, MSAA coverage, and stippled OIT. Do not introduce generic fullscreen straight-alpha blending as part of the stereo fix.
7. Re-query Unity render-buffer identities after recreation, pause/resume, or resolution changes; do not rely on stale native pointers.

Multi Pass exit criterion: on Quest Vulkan, both eyes contain IMM strokes over the Unity scene, have the correct per-eye view, and remain correct over repeated frames and pause/resume. Existing Metal, OpenGL, DirectX, Windows Vulkan, mono Android Vulkan, and composition evidence must not regress.

#### Phase C: extend the same design to Single Pass Instanced

1. Enable Single Pass Instanced only after the Multi Pass result is accepted.
2. Render both eye results from one native event. Initially, two sequential native eye renders are acceptable; correctness does not require Vulkan multiview.
3. Use the same stereo-aware Unity presentation pass so an instanced draw selects the matching eye result.
4. Prefer a two-layer `Texture2DArray` representation when native and Unity resource access is proven reliable. Until then, two ordinary RenderTextures bound simultaneously are an acceptable compatibility implementation.
5. Add true Vulkan multiview rendering into the two array layers only as a measured optimization. It is not part of the first correctness fix.

Single Pass exit criterion: Quest Vulkan Single Pass Instanced displays correct, distinct IMM content in both eyes and passes the same visual contracts as Multi Pass, with no fallback to Multi Pass hidden by the test.

The Windows Lavapipe lane remains useful as a runtime-contract test: the exact
Unity rejection of Vulkan and fallback to Direct3D must be detected and reported
as `skipped`/`not_tested`, never as visual success. An unexpected runtime failure
still reports `runtime_failed`. It is no longer the primary route to cloud
stereo evidence.

The Godot Run-button regression is now locally reproduced and fixed. The Quest-oriented Vulkan renderer had applied pipelined external-image submission and Unity reverse-Z depth handling to Godot even though Godot samples its standard-depth intermediate image in the same compositor callback. The corrected path waits on the non-dedicated queue, uses normal near-to-zero depth ordering, and restores shader-read layout before handoff. Two additional startup hazards were found and guarded: bounding-box queries during partial loading and child-count queries on non-group layers. The validation launches `project.godot` without a scene/script override, requires a clean native log, freezes only the evidence capture at frame zero, and compares the resulting 1280x720 frame to a reviewed Godot Vulkan baseline with a localized character/front-surface depth-order check.

## Completed validation-cleanup workstreams

The following workstreams are retained as an audit record. Statements such as
"pending" or "latest" inside their run-by-run findings describe the state at
that historical point; they are not current priorities. Run `30913083187` is
the later authoritative cleanup confirmation.

### 1. Validation-signal cleanup on `main`

At this stage of the cleanup, validator contracts for the reviewed Android Godot depth defects, missing Godot IMM content, duplicated stereo views, sky-only eyes, and localized cyan leakage were present. Unity Metal ordered-overlay probe loss and Godot ordered-overlay foreground cyan were product failures. Unity DirectX, Unity Metal full-depth, and Unity Android full-depth were false failures caused by measuring valid cyan outside the must-be-occluded character interior. The temporary generic alpha-blend compositor change was reverted because IMM's OIT/coverage model does not expose ordinary straight-alpha scene compositing.

At that stage, full run `30892709675` at `df6e68c0` was the latest manually audited exact-revision run. Its aggregate artifact, Android lane artifact, and targeted macOS lane rerun artifact were inspected. The audit closed the remaining CI-only Editor Play and Android physical-screen false failures:

1. The Unity Windows DirectX render-only image is visually correct, but its renderer variation narrowly exceeded the old spatial thresholds (`MAD 0.104 > 0.100`, `correlation 0.520 < 0.550`). The DirectX-only contract now uses `MAD <= 0.120` and `correlation >= 0.450`; the reviewed cloud image passes, while existing black, displaced, missing-content, and default-scene negative fixtures remain rejected.
2. The generic `sample1-composition-content` contract checked only broad IMM content. It therefore allowed a macOS Godot composition image with no magenta/yellow probes to pass. All full-depth lanes now use a dedicated `sample1-full-depth` contract requiring recognizable IMM content, both visible probes, and near-zero cyan leakage in the reviewed occlusion region. The ordered-overlay contract checks the same required probes and restricts cyan leakage measurement to the character occlusion area.
3. Replaying the localized contracts against the exact cloud artifacts reports the reviewed DirectX, Metal full-depth, and Android full-depth images as successful, while the Windows Godot ordered-overlay image remains `composition_failed`. macOS Unity still fails for missing ordered-overlay probes, Android still fails for duplicated synthetic eyes, and the Windows synthetic-stereo lane remains correctly `runtime_failed` when hosted Unity rejects Lavapipe Vulkan and falls back to Direct3D.
4. The normal push event does not execute the two hardware-gated Windows Unity Vulkan full-depth/ordered-overlay jobs. Their contracts are covered by validator fixtures, but they are not new exact-revision cloud visual evidence. Do not describe those two lanes as cloud-proven until an appropriate hardware dispatch runs them.
5. The first cloud run of the strict ordered-overlay contract exposed a remaining false positive: its widened cyan region included a legitimate cyan butterfly in `sample1.imm`. The contract is now restricted to the character occlusion area. Replays prove the macOS Unity overlay no longer reports the butterfly as leakage (it still correctly fails because both required overlay probes are absent), while the Windows Godot cyan square remains a hard failure. A positive fixture places legitimate cyan outside the occlusion area so this regression cannot return unnoticed.
6. The combined licensed macOS Unity Editor invocation reaches Play mode. Its first synchronous `Camera.Render()` capture produced only Unity's default sky/ground and the visual contract correctly rejected that image. A manual macOS test of the same pushed revision, project, and `SampleScene.unity` renders IMM immediately after pressing Play, proving the sample itself is not broken. Run `30891318133` then proved why the normal end-of-frame replacement also fails in CI: batch-mode Unity advanced 270 coroutine frames in about one second before the native document was ready, and `WaitForEndOfFrame` never resumed because no Game View render loop exists. The runtime smoke is now disabled for this Editor-only test. The Editor controller pumps the sample camera at a throttled wall-clock cadence, waits for native sequence readiness plus an applied and settled spawn-area viewpoint, and only then performs the explicit 1280x720 camera capture. Item 9 records its exact-revision cloud confirmation.
7. The Firebase physical-screen video contains a visually correct Unity Android Vulkan render-only interval, but the external-video contract falsely rejected it. With two-second sampling, the first correct sample passed and the next visually correct animated sample missed the correlation floor by only `0.007` (`0.493` versus `0.500`); the following sample had intentionally transitioned to the composition probes. The contract still requires two consecutive matching samples and absence of the magenta/yellow composition probes, but its renderer-variation correlation floor is now `0.490`. Earlier default/blank frames remain far below it (about `0.069`), and composition frames remain independently rejected by the color-component probes. Replaying the exact run `30869412758` Firebase video now passes with two consecutive samples at reported times 22 and 24 seconds; the selected external-display image visibly contains the expected IMM scene without composition probes.
8. Run `30892709675` cloud-proves the Android external-video correction. The external-display validator passes two consecutive physical-screen samples at reported times 20 and 22 seconds, and the selected `unity-android-vulkan-external-render.png` visibly contains the correct IMM scene without composition probes. The independent render-only capture is also correct. Android remains genuinely red for byte-identical synthetic eye images; the cyan result from this run is reclassified in item 10.
   Run `30897579019` exposed a remaining renderer-variation edge case: the visually correct 20-second sample missed the correlation floor by `0.001`, the 22-second sample passed, and the fixture transitioned to composition probes by 24 seconds. The floor is now `0.480`, which passes both reviewed correct frames while blank startup remains around `0.067` and the later composition state remains both below the floor (about `0.431`) and independently rejected by magenta/yellow probe detection. Two-second sampling and the two-consecutive-sample rule are retained, avoiding the substantial cost of evaluating every one-second frame.
9. The first macOS job attempt in run `30892709675` ended before rendering when Unity's license-activation process aborted during shutdown with exit code 134. A targeted rerun reached the readiness gate at frame 229, wrote `unity-macos-metal-editor-play.png`, and passed the macOS Metal render contract. A second targeted rerun was downloaded before aggregate cleanup and manually inspected: the Editor Play image contains the expected IMM character and branch scene and closely matches the separately captured Metal player render. The macOS lane remains genuinely red for absent magenta/yellow ordered-overlay probes; the full-depth cyan result is reclassified in item 10.
10. Pixel-component inspection corrects the full-depth cyan conclusion in items 8 and 9. In the Android image, the former largest cyan component is the authored butterfly at `(767–795, 300–317)`; the remaining components are the intentionally visible portions of the rear square outside the character. In the Metal image, the largest component is the square's top rim. The reviewed DirectX image has the same correct rim-only occlusion. None contains cyan in the localized character-interior region. The Windows Godot ordered-overlay failure fills all `3315` pixels of that same interior region, so the localized contract passes all three reviewed Unity images while retaining the known-bad Godot result and the synthetic foreground-cyan negative fixture.

The earlier `df6e68c0` audit removed most false visual failures but did not close the gate: the cyan region was still too broad. The localized character-interior contract now replays correctly against the reviewed DirectX, Metal, Android, and known-bad Godot images; a new cloud run must confirm the aggregate report before cleanup is called complete. Hosted Windows Unity Vulkan remains an explicit API/runtime coverage failure because Unity rejects Lavapipe and falls back to Direct3D, and the hardware-gated Windows Unity Vulkan composition jobs remain explicit incomplete coverage rather than false visual results.

Run `30867812844` confirmed the DirectX render-only false negative is removed and the stricter full-depth contract catches the previously false-passing macOS Godot image. Its aggregate artifact was manually audited image by image: all green visual lanes contain recognizable IMM content; Windows Godot full-depth is sky-only; Windows Godot ordered overlay has cyan in front; Android synthetic stereo contains two valid but byte-identical eyes; and hosted Windows Unity Vulkan is an API-fallback runtime failure. The initial conclusion that the reviewed Unity DirectX, Metal full-depth, and Android full-depth images had cyan leakage was wrong; item 10 records the pixel-localized correction. Follow-up run `30869412758` cloud-proved that the narrowed ordered-overlay region excludes legitimate cyan butterfly content while continuing to reject the actual Windows Godot cyan square. Run `30892709675` then cloud-proved both the Android physical-screen correction and the readiness-gated macOS Editor Play capture.

Full run `30861503259` was manually audited. It confirmed the earlier conclusions and proved two fixes in the cloud: Android Godot passed from its two valid visual contracts despite a missing redundant log marker, and nested Editor/Sample Play captures no longer became bogus independent report lanes. Every remaining visible red image in that run was a genuine rendering or composition failure. The audit nevertheless exposed three validation/reporting defects that were fixed locally and at that point awaited cloud confirmation:

1. Windows Godot hard-coded every failed job as `runtime`, while its in-scene process exit also set `Rendering: failed` when only a composition probe failed. A lane-level evidence classifier now requires the Run-button, render-only, full-depth, and ordered-overlay captures and metrics. It writes consistent full-depth/overlay status JSON, reports the reviewed render as successful while preserving cyan/missing-content failures as `composition_failed`, and treats a missing redundant success marker as a warning. Only explicit crash/device-loss evidence produces `runtime_failed`.
2. A separate macOS Unity Editor invocation cannot reuse the GameCI license after the builder action returns it; run `30865518088` demonstrated that this path fails before rendering with `No valid Unity Editor license found`. The build and project Play-button smoke are therefore combined into one licensed GameCI invocation with `manualExit: true`. A runtime coroutine captures the normal Game View at end of frame and writes the PNG before the Editor controller exits Play mode. A confirmation step requires the file. The lane-level classifier requires the Editor Play, render-only, full-depth, and ordered-overlay images and metrics, while a missing Editor capture cannot hide an independently proven player composition failure.
3. The hosted Windows Unity Vulkan fallback had no image and therefore disappeared from the detailed report. Synthetic stereo is now a distinct supported matrix row rather than contaminating the non-VR Vulkan row. Failed status-only visual lanes receive their own section, result, failure class, and classifier details; a build-only manifest is explicitly `evidence_incomplete`, never visual success.

A subsequent suite-wide audit found the same architectural false-failure risk in four additional visual lanes: Unity Windows DirectX, Unity Windows Vulkan full-depth, Unity Windows Vulkan ordered-overlay, and Godot macOS Metal. Their run, preliminary classification, metric, report, and redundant log checks now collect diagnostics without independently deciding the job. A final lane classifier requires the relevant captures and external baseline metrics, preserves explicit crashes and requested-API fallback as hard failures, and makes passing image evidence authoritative over stale in-scene probe messages. The Windows Vulkan synthetic-stereo lane likewise has one final classifier combining the runtime result, side-by-side structure check, independent left/right baseline metrics, and per-eye target/matrix routing contract; its manifest no longer cites only the preliminary runtime result. These changes passed local classifier and workflow-contract tests and were subsequently covered by the exact-revision cloud audit.

The Android Unity classifier retains the same completed-evidence rule: a secondary Firebase reporting error becomes a warning once every required image exists and passes, but it cannot mask a render, composition, stereo, external-screen, API-fallback, crash, or incomplete-evidence failure. Classifier failures and warnings are now preserved in manifests so aggregate status-only sections can explain the verdict.

All validator self-tests, report aggregation tests, workflow-matrix checks, negative visual fixtures, and sample-entrypoint contracts pass locally. The cleanup gate remains open until a fresh full cloud run at the exact revision is downloaded and every automatic verdict is reconciled against every image. The supported matrix now contains 17 rows because Windows Vulkan synthetic stereo is tracked separately.

Full run `30854621320` exposed a report-level false pass even though the underlying job correctly failed: `godot-vulkan-ordered-overlay.png` showed the cyan square in front of the character, and its dedicated metric rejected leakage (`0.007910 > 0.000250`), but the aggregate report selected the sibling passing render-only metric and printed `passed`. Metric aggregation now associates a generated report with its named candidate capture and preserves that candidate's failed metric. A regression fixture contains both a passing render-only metric and a failed ordered-overlay metric and requires the aggregate section to remain `composition_failed` through normalization.

The same run exposed a renderer-variation false failure in the otherwise reviewed Unity Metal render: spatial correlation was `0.387` against a `0.400` floor. The Metal-only floor is now `0.350`; black, default-scene, shifted-pose, missing-content, and composition fixtures remain independently rejected. Cloud validation is still required before this cleanup gate is complete.

The Android Godot `character-front-depth-order` regions now have a pixel-level negative fixture, rather than only a source-token check. The fixture injects foreground geometry into the reviewed character region and requires both the render-only and composition contracts to reject it, covering the reverse-Z/incorrect foreground ordering class that broad whole-frame metrics previously missed.

The first cloud run after the report fix also proved the preflight signal is useful: Unity 6 project serialization had changed Android `m_InitManagerOnStart` from `0` to `1`, bypassing the scene-controlled `SampleSceneVR` bootstrap and making ordinary Android launches try to enter XR. This is a genuine sample-project regression, not a false failure. Restore `m_InitManagerOnStart: 0`, retain the verifier contract, and let the VR scene initialize OpenXR explicitly.

Full runs `30811686247`, `30813880906`, `30816894610`, and `30820773061` have been inspected. Their images show that Unity DirectX, Unity Metal full-depth, Unity Android Vulkan composition, and Godot Metal have genuine composition failures. Run `30820773061` contains all 16 supported rows and preserves both semantic eye captures. It also exposed two remaining false report signals: a generic root-level `Composition` section and a visually correct Unity Metal render rejected only because spatial MAD was `0.152` against `0.150`.

1. Index every valid manifest, including failed and expected-failed manifests, while allowing only a passed manifest to satisfy a supported row.
2. Carry non-build status manifests and their strict visual metrics through each aggregation layer so the final report retains preflight and failed-lane evidence.
3. Canonicalize `macOS` identifiers before matching reports, manifests, and matrix rows.
4. Show the manifest result and failure class in the aggregate report.
5. Suppress generic nested capture-mode sections such as `Render`, `Full depth`, and `Ordered overlay` when the authoritative parent lane is already reported.
6. Classify the Unity DirectX and Metal cyan depth failures as `compositing`, not generic `visual` failures.
7. Classify Android Unity Vulkan from its actual evidence instead of hard-coding every failure as `compositing`: app/API crashes are `runtime`, Firebase command or collection failures are `infrastructure`, absent authoritative captures are `evidence`, failed render/stereo images are `rendering`, and only a failed depth image after the other contracts pass is `compositing`.
8. Run the full suite and manually inspect the regenerated report.

Exit criterion: every report result agrees with its underlying evidence, every remaining red entry is actionable, and each known-bad visual fixture is rejected for the correct reason.

### 2. Android/Firebase synthetic-stereo Vulkan lane

The immediate implementation priority is hardware-backed Android Vulkan in Firebase, reusing the existing Unity Android Vulkan build and capture infrastructure. The synthetic test exercises IMM eye routing and composition without requiring an OpenXR headset or separate-eye presentation.

Run `30820773061` proves target priming works: both split eye images independently pass the approved render baseline, and the exact native write-target pointers are non-zero and distinct. The files are nevertheless byte-identical, so the stereo disparity contract correctly fails. The next iteration uses an exaggerated validation-only eye separation and records the uploaded matrix translations, while retaining the strict requirement that the resulting images differ. Log-only success is not accepted.

1. Make synthetic stereo call the production render-both-eyes entry point. Remove its dependence on manually selecting an eye and invoking `Camera.Render()` twice as the authoritative test.
2. Add a deterministic Android presentation fixture that exposes the two produced eye views side by side without requiring an OpenXR headset.
3. Run that mode on a Firebase device that reports and uses Vulkan.
4. Capture a side-by-side image through the existing Firebase video/evidence path.
5. Keep the test honest:
   - require `actual=Vulkan`;
   - require eye 0 and eye 1 to use the same IMM camera ID;
   - require distinct adjacent eye event IDs;
   - require distinct non-zero native render-target pointers;
   - require a side-by-side image with independently validated left and right halves;
   - never accept an API fallback or a single duplicated eye as synthetic-stereo Vulkan evidence.
6. Add a negative fixture or classifier test in which one half contains only the Unity sky/default scene; it must fail even when the other half renders IMM correctly.
7. Add a duplicated-eye negative fixture; two independently valid but byte-identical views must fail the disparity contract.
8. Inspect the resulting image manually before declaring the lane valid.
9. Record the limitation in the report: this exercises the production IMM Android Vulkan two-eye producer and a non-XR presentation fixture, not the Quest/OpenXR compositor itself. A Quest acceptance test remains required for the headset handoff.

Exit criterion: the Firebase job produces recognizable IMM content in both eye images through the Vulkan path, both halves satisfy an approved visual baseline, and the combined evidence appears in the validation report.

### 3. Reported-failure inventory by failure class

Status: completed for full run `30811686247`; results are recorded in `VALIDATION_FAILURE_INVENTORY.md`.

For one exact commit and workflow run, create a table containing:

- lane and capture name;
- whether the expected image was produced;
- manual visual verdict;
- automatic visual verdict and exact failed metric;
- composition-probe verdict;
- log-contract verdict;
- infrastructure/runtime verdict;
- whether the failure is genuine, a false failure, or unresolved.

Do not change thresholds until the associated image has been inspected.

### 4. Log-only false failures

Status: completed. The macOS Metal lane no longer fails because the optional `Loaded in CPU` and `Loaded in GPU` diagnostic strings are absent. Explicit load errors and the mandatory visual checks remain failures.

1. Find the actual destination of native IMM logs on each platform.
2. If the markers are reliably available in another artifact, validate that artifact instead.
3. Otherwise replace them with a reliable structured load-completion marker emitted by the integration layer.
4. Treat load messages as fast-fail diagnostics. A redundant missing message must not override valid visual evidence.
5. Retain hard failures for explicit load errors, missing documents, missing captures, renderer initialization failure, or absence of recognizable IMM content.

Exit criterion: Metal does not fail solely because redundant native log strings are absent, while genuine document-load failures still fail before visual comparison.

### 5. Stale or mismatched visual baselines

Status: the reviewed Unity render baseline now accepts the correct DirectX, Metal, and Android Vulkan render-only captures from run `30811686247`. No baseline change is justified by that run.

1. Confirm the intended camera pose, playback timestamp, resolution, color space, and scene state for every Unity capture.
2. Generate candidate baselines only from a reviewed, correct rendering of that exact fixture.
3. Compare candidates across at least two repeat runs to measure normal nondeterminism.
4. Commit a baseline contract that tolerates normal renderer variation but rejects:
   - black or blank frames;
   - the default Unity scene without IMM content;
   - badly displaced or distant cameras;
   - missing major scene regions;
   - incorrect orientation;
   - severe color or exposure corruption.
5. Keep reference provenance in the baseline metadata: fixture, camera, freeze time, Unity version, renderer, and source run.

Exit criterion: reviewed correct images pass repeatedly, and deliberately altered black, default-scene, displaced-camera, and missing-content fixtures fail.

### 6. Composition probes based on actual visible geometry

Run `30811686247` shows that the front-visible and rear-visible probe checks are no longer causing the Unity failures. Later pixel inspection proved that broad cyan-region failures can count valid outside-silhouette pixels. Keep the leakage threshold strict, but apply it only inside the reviewed must-be-occluded character interior.

1. Derive the expected probe mask from the rendered probe geometry rather than a loose projected bounding rectangle.
2. Ignore antialiased boundary pixels and allow a small renderer-dependent edge tolerance.
3. Validate front-visible and rear-visible probes independently.
4. Use depth-aware expected masks for occluded probes rather than a single whole-rectangle color share.
5. Add negative fixtures proving that missing, swapped, fully hidden, and incorrectly foregrounded probes fail.

Exit criterion: intact probes pass on DirectX, Metal, Vulkan, and OpenGL; deliberately incorrect depth ordering fails on every applicable renderer.

### 7. Explicit cyan depth contract

The cyan probe currently exposes an apparent depth/occlusion problem, but at least one classifier reports success. Tightening this check is more important than making the report green.

1. Define exactly which cyan pixels should be visible and which scene/IMM geometry should occlude them.
2. Produce an expected occlusion mask from a reviewed correct reference.
3. Measure leakage only inside regions that must be occluded; do not penalize cyan pixels intentionally extending beyond the occluder.
4. Report leakage count, leakage area, and a diagnostic overlay showing the unexpected pixels.
5. Add controlled negative fixtures:
   - cyan forced in front;
   - depth testing disabled;
   - IMM depth absent;
   - Unity scene depth absent.
6. Require the existing defective capture to fail the new contract before accepting the test change.

Exit criterion: the known cyan defect fails with a clear diagnostic overlay, while a reviewed correct occlusion capture passes across repeated runs.

### 8. Product, infrastructure and evidence failure separation

Do not label a renderer visually broken when the failure occurred before rendering.

Use distinct report states:

- `render_failed`: a produced image violates the approved visual contract;
- `composition_failed`: depth or ordering violates the composition contract;
- `runtime_failed`: the requested graphics API or player could not start;
- `infrastructure_failed`: runner, Firebase, driver, download, or tool failure;
- `evidence_incomplete`: required artifacts were not collected;
- `passed`: all required visual and supporting contracts passed.

Aggregate reports must preserve these distinctions instead of flattening all of them into a red visual failure.

### 9. Validator regression tests

Each validator must be tested with known positive and negative fixtures.

Required cases:

- correct render passes;
- small normal renderer variation passes;
- black frame fails;
- default Unity scene fails;
- wrong camera pose fails;
- missing IMM content fails;
- correct composition passes;
- cyan depth leakage fails;
- absent optional diagnostic marker does not override visual success;
- explicit load or renderer error fails immediately;
- requested Vulkan falling back to another API fails as a runtime failure;
- missing capture reports `evidence_incomplete`, not `render_failed`.

### 10. Exact-revision cleaned-report audit

1. Run the full cloud validation suite.
2. Download and manually inspect every rendered capture, including both synthetic eyes.
3. Check every red report entry against its underlying evidence.
4. Confirm that all remaining failures are reproducible, correctly classified, and actionable.
5. Do not loosen a contract merely to make the report green.

## Validation-cleanup outcome

Run `30913083187` demonstrated the intended outcome:

- correct Metal and DirectX renders no longer appear as visual failures because of stale thresholds or redundant log markers;
- known-bad reverse-Z, missing-content, duplicated-eye, sky-only-eye, black/default-scene, and incorrect-occlusion images cannot appear green;
- infrastructure problems are clearly separated from rendering defects;
- the synthetic-stereo lane provides two-eye Vulkan evidence without requiring a physical headset;
- corrected depth/occlusion output passes, while the known-bad cyan fixtures
  remain rejected.

## Implementation findings

- Run `30804478423` (`82c538c038035ad373a36fb924b0ba23b2d50df2`) proved that the standard GitHub Windows runner exposes no Vulkan GPU accepted by Unity. Mesa Lavapipe was installed and selected correctly, but Unity reported no Vulkan device and fell back to Direct3D 11.
- The repository's Android native dependency graph is ARM64-only, including the imported static libraries and `libjpeg-turbo`. An x86_64 Android emulator lane therefore requires a separate native-porting task and is not a workflow-only substitute.
- The Windows software-Vulkan synthetic lane must continue to report `runtime_failed` rather than a visual failure when Unity falls back to Direct3D.
- The primary cloud stereo strategy is now an ARM64 Android/Firebase hardware-Vulkan lane, avoiding both the unsupported Windows software-Vulkan device and the x86_64 native dependency gap.
- The known Swappy failure artifact from run `30813880906` is now covered by a classifier regression test and is reported as `runtime_failed`, rather than the lane's former hard-coded `compositing` label.
- Run `30816894610` shows why complete visual evidence must outrank a secondary Firebase CLI failure: the CLI returned `1` because Cloud Tool Results API is disabled after the test, but all captures were downloaded and contain an unambiguous black-eye rendering failure. The classifier now reports that result as `render_failed` and retains the Firebase error as supporting detail.

### Godot Vulkan full-depth sky-only capture

- Root cause identified in the shared Vulkan external-image path: Godot's
  intermediate depth pipeline uses normal-Z/LESS, but the image was cleared to
  `0.0` by the Unity external-eye reverse-Z feature flag. That rejects every
  IMM depth-tested fragment and leaves only Godot's sky/background.
- The clear now follows the player's configured LESS/GREATER depth state, with
  an explicit integration-level reverse-Z declaration retained as a first-frame
  fallback. Godot external images declare normal-Z and clear to `1.0`; Unity
  external images declare reverse-Z and clear to `0.0` even on platforms that
  do not use Android's dedicated queue.
- Godot 4.3+ exposes the scene depth texture as reverse-Z. The compositor now
  converts that sample to normal-Z before comparing it with IMM's normal-Z
  intermediate depth. The prior direct comparison mixed opposite conventions.
- Run `30913083187` visually confirmed that authored strokes returned and that
  the full-depth cyan probe was occluded correctly on Windows Vulkan and macOS
  Metal.
