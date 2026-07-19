# Procedural Runtime Authoring Demo

Open `Scenes/ProceduralAnimationDemo.unity` and enter Play mode. The
`Procedural IMM Demo` component will:

1. Build a multi-frame, multi-strand ribbon using `ImmAuthoringDocument`.
2. Commit the complete animation through one `ImmAuthoringTransaction`.
3. Export the committed revision with `ImmAuthoringCompiler.ExportToFile`.
4. Request that exact revision through `ImmAuthoringPreviewCoordinator`.
5. Compile it to memory on a worker and atomically install it in the native player.

The generated file is `procedural-ribbon.imm` in
`Application.persistentDataPath`. Its full path, byte size, source revision,
and current state are visible on the component while the scene runs.

The component context menu also provides:

- **Generate and Play**
- **Export IMM Only**
- **Unload Generated IMM**

The scene is intentionally an engine sample: it uses inspector properties and
context-menu commands, without application-level animation-editor UI.

The coordinator is the Phase 4 engine path used by an external editor. A later
request supersedes queued, compiling, or loading work. The installed native
document stays visible until its replacement is fully loaded, and it also stays
installed if replacement compilation or loading fails. Request state,
revision, byte count, compilation time, player-load time, and total latency are
available through `ImmAuthoringPreviewRequest`.
## Phase 1 lifecycle and performance gate

Add `ImmRuntimeAnimationSpike` to a scene with the existing IMM player setup.
The component context menu provides:

- **Run Animation Spike** for one end-to-end cycle.
- **Run 100-cycle Lifecycle Gate** for the Phase 1 soak gate.

Each cycle builds a mutable multi-frame paint document, compiles it to an owned
memory buffer using the batch point ABI, loads it into the native player, seeks,
plays one rendered frame, and unloads it. Logs use `[IMM_AUTHOR_SPIKE_P1]` and
report construction, graph compilation, serialization, load-to-ready,
first-render timing, output size, and memory deltas. The gate stops immediately
if the manager's live-document or retained input-buffer count does not return to
its starting value after an unload.
