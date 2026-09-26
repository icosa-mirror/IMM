# IMM runtime animation editor roadmap

Status: active roadmap for replacing compile-and-reload preview with live runtime authoring.

## Document authority

This roadmap owns the product objective, measured budgets, milestone order, risks, and stop
conditions. [IMM_RUNTIME_LIVE_DOCUMENT_ARCHITECTURE.md](IMM_RUNTIME_LIVE_DOCUMENT_ARCHITECTURE.md)
is the normative source for identity, batching, commit semantics, renderer resource lifetime,
the C ABI, threading, ownership, errors, and milestone acceptance details. If the two documents
conflict on an implementation rule, the architecture document wins.

`IMM_RUNTIME_ANIMATION_EDITOR_PLAN.md` is retained only as historical context. The implemented
compile-and-reload authoring boundary remains documented in
`docs/runtime-authoring-engine-contract.md`; this roadmap describes its live-runtime successor.

## 1. Objective

Let an application edit an IMM document while it is playing and present the complete result at
interactive latency on every platform where the runtime authoring path is supported.

The interactive path must no longer encode the whole document to IMM bytes and parse it back.
IMM remains the persistence and interchange format for save, load, import, publish, process
boundaries, and regression tests.

## 2. Measured trigger and latency budget

The existing preview path serialises the entire authoring graph and reloads the resulting IMM
document. The recorded `edit-document` baseline is:

| Document | Drawings | Full compile and reload |
|---|---:|---:|
| Small | 10 | 1.79 ms |
| Medium | 400 | 51.61 ms |
| Large | 4,000 | 561.74 ms |

The Large cost is dominated by document-wide graph walking, allocation, quantisation,
encoding, parsing, dequantisation, renderer setup, and copying. Compression changes can reduce
part of that cost but cannot make a one-drawing edit scale with the edit rather than the whole
document.

Targets for a commit changing one drawing are:

1. Small and Medium: presented in under 16 ms.
2. Large: presented in under 50 ms.
3. Canonical visibility, opacity, or transform changes: presented in under 2 ms without paint
   tessellation or geometry upload.
4. Cost scales with affected layers and drawings, not unrelated document content.

Every measurement must distinguish ABI copy time, queue time, CPU preparation, GPU
preparation, and end-to-end time to the presented revision. Median and p95 are recorded on
matched content and artifact hashes.

## 3. Product and architecture decision

The runtime-owned document becomes the live authoring target and the renderer consumes a
presented revision of that document directly.

1. Mutation commands are copied into an owned edit batch.
2. A commit prepares replacement model and renderer state without altering the active
   revision.
3. The complete revision is presented at one renderer boundary.
4. Old resources retire through renderer-owned synchronization.
5. Save and publish project a presented revision into IMM bytes.

This does not introduce another long-lived document graph. Batches, handle maps, replacement
objects, and renderer bundles are transactional control structures around the existing runtime
model. Detailed rules belong exclusively to the architecture document.

## 4. Scope

The planned live path covers:

1. Static paint layers and groups.
2. Drawing creation, geometry replacement, frame mapping, and deletion.
3. Canonical layer properties and supported animation keys.
4. Spawn areas where the runtime representation supports them.
5. Save and publish from a presented live revision.
6. Managed authoring API cutover after the native boundary is proven.

The initial roadmap excludes:

1. A new IMM file format.
2. Persistent authoring handles across save and reload.
3. Picture, model, sound, or effect mutation.
4. Concurrent writers or multi-document transactions.
5. A host-facing editor UI.
6. Lossless source precision beyond the existing runtime paint representation.
7. Per-drawing GPU suballocation unless measurement justifies it.

## 5. Milestones

Each milestone ends with the relevant regression suite and matched latency measurements.

### M0 — Architecture and harness

Status: document complete; implementation does not yet satisfy it.

1. Maintain the normative live-document architecture and C ABI contract.
2. Keep the existing player regression runner and compile-and-reload baseline.
3. Add failure-injection and revision-consistency checks before claiming atomic commits.

### M1 — One-drawing vertical slice

1. Allocate session-stable drawing handles independently of drawing indices.
2. Copy one geometry replacement into an owned batch and queue it through the document command
   path.
3. Expose requested, prepared, presented, and rejected commit status.
4. Prepare a replacement static-paint layer resource bundle without touching the active one.
5. Present the new bundle atomically and retire the old bundle through renderer-owned lifetime
   tracking.

Acceptance: old-or-new frame consistency under normal execution and injected failure, no IMM
encode/parse, existing playback unchanged when authoring is not attached, and the Large edit
under 50 ms.

### M2 — Editing breadth and cache correctness

1. Add drawing creation, frame remapping, and deletion through handle resolution.
2. Add canonical layer-property edits, separate from playback overrides.
3. Add supported animation-key and spawn-area mutation.
4. Invalidate bounds, timelines, spawn caches, and other enumerated derived state.
5. Keep per-layer rebuilding as the correctness baseline; attempt narrower regeneration only
   with measured benefit.

Acceptance: every supported mutation presents atomically, property-only commits remain under
2 ms, and no unrelated layer is rebuilt.

### M3 — Save and publish

1. Drive the IMM writer from a selected presented revision.
2. Reject or explicitly select around unresolved commits.
3. Preserve existing file compatibility and content baselines.

Acceptance: unchanged content remains byte-comparable where the current baseline requires it,
and written documents round-trip through existing readers and render checks.

### M4 — Managed authoring cutover

1. Re-point the managed authoring API at the live native document.
2. Preserve the application-facing transaction and revision model where it agrees with the
   native contract.
3. Remove compile-and-reload from interactive preview while retaining explicit save/load.

Acceptance: existing managed authoring tests pass against the live path and no interactive
preview operation touches the IMM byte format.

### M5 — Surfaces and platforms

1. Integrate the Unity sample and viewer with the live path.
2. Measure and validate each supported graphics backend and platform.
3. Document unsupported mutation surfaces explicitly rather than silently falling back.

Acceptance: platform results and any gated fallback are recorded against the same content and
revision-consistency checks.

### M6 — Optional measured optimisations

Consider per-drawing GPU allocation, large-transaction fast paths, streaming, or additional
document-wide operations only when M1–M5 measurements identify a limiting cost. Keep rejected
candidates and their evidence in the decision record.

## 6. Existing prototype disposition

The prototype proved useful mechanisms but is not the target transaction model.

1. Keep the static drawing geometry builder, ABI conversion, quantisation helpers, regression
   runner, benchmark corpus, and diagnostic timing hooks.
2. Replace direct mutation before commit with owned batches and prepared revisions.
3. Replace in-place renderer slot refresh as the publication mechanism with replacement bundle
   presentation.
4. Replace the fixed three-frame destruction delay with renderer-owned retirement.
5. Do not treat playback property overrides as persisted authoring changes.
6. Do not expose drawing vector indices as public identity.

Detailed experiments, failed approaches, and the next implementation slice are recorded in
section 13 of the architecture document.

## 7. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Runtime objects assume immutability | Start with one affected static-paint layer and enumerate every derived cache before broadening scope. |
| CPU and GPU state from different revisions are mixed | Prepare a complete resource bundle and switch model and renderer references at one presentation boundary. |
| GPU resources are destroyed while still in flight | Retire through backend-owned fences or its configured frames-in-flight policy. |
| Handle identity leaks storage indices | Resolve 64-bit session handles through document-owned maps. |
| Save observes pending or partial state | Save only an explicitly selected presented revision. |
| Preview and exported pixels diverge unexpectedly | Use the runtime's existing quantisation rules and keep round-trip/render baselines. |
| Existing playback regresses | Keep authoring opt-in and run the player regression suite before and after each milestone. |
| Scope expands into a general scene editor | Restrict M1–M5 to the supported content in section 4. |

## 8. Stop and fallback conditions

1. If the M1 Large edit cannot get under 50 ms without touching unrelated layers, stop before
   broadening the mutation surface and profile the measured limiting stage.
2. If an atomic replacement bundle cannot be presented on a backend, keep live authoring gated
   off there until a correct backend-owned lifetime mechanism exists.
3. If a prototype improves CPU headroom but not visible commit latency, do not promote it into
   the default path.
4. If persistent IDs become a requirement, stop and approve a format extension or sidecar;
   do not imply persistence from traversal order.
5. Keep compile-and-reload available as an explicit compatibility path until live authoring is
   accepted on the relevant platform, but do not silently use it while reporting live-edit
   latency.

## 9. Current handoff

The existing prototype has direct geometry replacement, drawing append/frame mapping, dirty
queues, and experimental CPU/GPU refresh paths. Those changes are evidence, not completion of
M1 under the revised architecture.

Resume with the one-drawing vertical slice in M1 and section 13 of
[IMM_RUNTIME_LIVE_DOCUMENT_ARCHITECTURE.md](IMM_RUNTIME_LIVE_DOCUMENT_ARCHITECTURE.md).
