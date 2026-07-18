# Procedural Runtime Authoring Demo

Open `Scenes/ProceduralAnimationDemo.unity` and enter Play mode. The
`Procedural IMM Demo` component will:

1. Build a multi-frame, multi-strand ribbon using `ImmAuthoringDocument`.
2. Commit the complete animation through one `ImmAuthoringTransaction`.
3. Export the committed revision with `ImmAuthoringCompiler.ExportToFile`.
4. Load the generated IMM into the native player and leave it playing.

The generated file is `procedural-ribbon.imm` in
`Application.persistentDataPath`. Its full path, byte size, source revision,
and current state are visible on the component while the scene runs.

The component context menu also provides:

- **Generate and Play**
- **Export IMM Only**
- **Unload Generated IMM**

The scene is intentionally an engine sample: it uses inspector properties and
context-menu commands, without application-level animation-editor UI.
