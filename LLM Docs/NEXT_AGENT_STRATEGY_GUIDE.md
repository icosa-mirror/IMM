# Next Agent Strategy Guide (Quest XR Strokes Missing, Sky Visible)

## Mission
- Preserve a known-good Android **NonXR baseline**.
- Isolate why Quest OpenXR XR shows sky/background but **no brush strokes**.
- Drive short, evidence-based experiment loops and stop speculative drift.

## Current Ground Truth
- NonXR behavior was restored from commit `16e3e67` and must stay protected.
- XR launches and renders sky/background; IMM document loads and render loop runs.
- Layers are populated in XR (`layerCount` and non-zero stroke counts observed).
- Strokes are still not visible in headset in both:
  - OpenXR Single Pass
  - OpenXR true Multi Pass (`xrMode=MultiPass`, `stereo=1` confirmed)
- Prior toggles (cull/depth off, mono fallback, forced multipass API patterns) did not fix visibility.

## Hard Constraints
- Do not mix XR experiments into NonXR profile/state.
- Treat `Launch is blocked because: device in VRUI` / Guardian prompts as invalid runs.
- Use unique log prefixes per experiment (required), e.g. `IMMDBG_NA01_*`, `IMMDBG_NA02_*`.
- For questions vs actions: if user asks a question, answer first; do not assume action.
- If Unity needs script reload/compile and focus is required, ask user explicitly.

## Files To Treat As High Risk
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs`
- `code/appImmUnity/src/main.cpp`
- `code/ImmUnitySampleProject/Assets/XR/Settings/OpenXR Package Settings.asset`
- `code/ImmUnitySampleProject/Assets/XR/XRGeneralSettingsPerBuildTarget.asset`
- `code/ImmUnitySampleProject/ProjectSettings/EditorBuildSettings.asset`
- `code/ImmUnitySampleProject/Assets/Scenes/SampleSceneVR.unity`

## Working Model (Why This Is Likely Failing)
- Content exists (layers/strokes loaded), but XR final composition path likely drops or masks stroke draw output.
- Because failure survives both Single Pass and true Multi Pass, root cause is likely **not** only stereo mode selection.
- Most probable divergence: draw order/state/material/target mismatch specific to stroke pass in XR branch.

## Priority Investigation Plan

### 1) Prove stroke draw calls execute in XR path
- Instrument `main.cpp` around stroke-pass entry/exit with a unique prefix.
- Log per-eye and per-layer/stroke pass counters actually submitted in-frame.
- Add one-frame sampling counters every N frames (e.g. every 60) to reduce log flood.
- Success criterion: clear evidence that stroke draw calls are executed for XR eyes.

### 2) Validate XR GL state at stroke pass boundaries
- Log key state before and after stroke draw:
  - bound framebuffer
  - viewport/scissor
  - depth test/write, blend state, cull mode
  - color mask
  - active shader/program and uniform validity checks (minimal)
- Compare with equivalent NonXR stroke draw state snapshots.
- Success criterion: identify first material state mismatch that can fully hide geometry.

### 3) Verify render target continuity for stroke pass
- Confirm stroke pass writes into the same eye target later presented.
- Trace target IDs from eye begin -> stroke pass -> final submit for each eye.
- If target differs, isolate where target switch occurs and why it is not merged.

### 4) Add a known-visible XR debug primitive in same pass
- Draw a forced bright test primitive in the **exact stroke pass** (same target/state path).
- If primitive appears but strokes do not: issue is stroke-specific shader/data path.
- If primitive does not appear: issue is pass/target/composition pipeline.

### 5) Narrow to shader/data vs pipeline
- If stroke pass runs and primitive appears:
  - inspect stroke shader uniforms/attributes/texture bindings in XR path.
  - verify per-eye matrices and coordinate conventions used for strokes.
- If pass does not reach visible output:
  - focus on render pass ordering/resolve/submission handoff.

## Experiment Hygiene (Non-Negotiable)
- One hypothesis per run; one log prefix per run.
- Record each run in `XR_EXPERIMENT_LOG.md` with:
  - hypothesis
  - code delta
  - build/run log file
  - headset-observed outcome
  - keep/revert decision
- Keep changes minimal and reversible; avoid broad refactors.
- Do not claim fixes without headset-visible stroke confirmation.

## Execution Loop Template
1. Set profile to `QuestXR` explicitly.
2. Apply one minimal code change for one hypothesis.
3. Build/deploy/run with hardened android cycle script.
4. Validate run is not blocked by VRUI/Guardian.
5. Pull and inspect logs for current prefix across full file.
6. Update `XR_EXPERIMENT_LOG.md`.
7. Decide: iterate, keep, or revert.

## Fast Triage Checklist Before Each Run
- Correct profile active (`QuestXR`) and scene/build settings match.
- No stale mixed config from prior NonXR run.
- Log prefix updated to new unique ID.
- Launch validity checks enabled.
- Expected instrumentation points compiled into current build.

## Stop Conditions (Ask User)
- Unity compile/reload requires editor focus or exiting Play Mode.
- Any step requires credentials/secrets/accounts not in repo.
- A destructive action would be required.

## Deliverable Standard For Next Handover
- A short statement of what was proven/disproven.
- Exact file paths and small diffs that remain.
- Log filenames with key lines tied to the run prefix.
- Clear next single hypothesis, not a list of speculative options.
