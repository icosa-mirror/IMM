# Live Edit Lab

1. Open the sample project in Unity. On Windows, use Direct3D12 for this URP sample;
   **IMM > Live Edit Lab > Restart Editor with Direct3D12 (Windows)** switches it.
   Then choose **IMM > Live Edit Lab > Open sample and play**.
2. In the Game view, browse to a real `.imm` file and choose **Load / Reload**.
3. Use the full-width timeline at the bottom. Layer names and channel lanes share
   one vertical scroll with their tracks. Children are indented; arrows expand
   groups and + opens channel lanes. Filtering retains matching ancestors. Select
   a layer to edit its properties in the inspector. Watch the scene
   and the revision status: **Presented** means published; **Rejected** reports the
   native error. A busy/loading document cannot accept another edit yet.
4. Click or drag the ruler/tracks to pause and scrub. Use frame-step buttons,
   zoom +/−, range arrows, Home, Fit, Follow and Snap to navigate. Key squares show
   stored times: select one to inspect it, drag to move it atomically, or use Delete
   key. You can also enter a time and choose Move selected key. A move onto an
   occupied key time is rejected. Try edits while playing, paused and after seeking.
   Enter key times in seconds and choose interpolation in the inspector. Visibility keys have their own checkbox; opacity and transform
   keys use the displayed draft property values. Loop uses repeat `0` for true;
   Offset uses the repeat field as an exact integer tick count.
5. Blue cells preview drawing sequences; white marks indicate drawing changes,
   with holds sharing a drawing. Zoomed-out ranges show a bar instead of individual
   frame marks. Click a supported sequence to select its frame. Insert hold after
   frame, Remove frame and Use drawing at frame edit the real frame table. For
   geometry, select a paint layer and enter a frame index. **Replace with
   marker** replaces that frame's drawing with a magenta marker at the layer's local
   origin. A shared drawing changes in every frame that references it. Adjust the
   marker size and use **Frame scene** or fly the camera to find it.
6. Use an ordinary group as the selected parent and enter a new child name. Reparenting
   takes a destination layer ID and child index. Delete affects the entire selected
   subtree. Unsupported content is reported explicitly.
7. **Unload**, then **Load / Reload**, to restore the source scene. All edits affect
   memory only; this lab does not save or overwrite the IMM file. Timeline undo is
   described below; other document edits clear that history.

The Game view opens maximized; Shift+Space toggles its size.
Viewpoint arrows select an authored view; **Move to view** moves the camera once.
New documents initially use their available authored view. **Frame selected** fits
one layer; **Frame scene** includes every bound, including large backgrounds.
Right mouse + WASD flies the camera, Q/E moves down/up, and Shift moves faster.
F9 hides or shows the panel. Stop Play mode to close the session. Choose **IMM >
Live Edit Lab > Disable automatic panel** to keep the panel out of later Play sessions.

The lab calls the native live-edit API directly. It does not use compile/reload
preview or the sample's transient layer-override controls. General mixed-operation
transactions and nested timeline/resource-subtree visibility refresh remain unfinished;
they are not implied by the presence of these controls.

The initial timeline exposes Visibility, Opacity, Transform, Loop and Offset keys.
It is not a complete animation editor yet. Nested timeline clocks, nonzero offset
or visibility-controlled sequence placement, and loop-key projection need further
mapping work; unsupported sequence lanes say so instead of inventing clip spans,
and nested key lanes are hidden until their time domain can be represented.
Drawing sequence previews do not describe a layer's complete visibility lifetime.
Undo/Redo covers timeline keys, drawing frame-table edits and canonical paint
repeat counts. It records published edits only; rejected edits keep history.
Ctrl/Cmd+Z undoes, Ctrl/Cmd+Shift+Z or Ctrl+Y redoes when the timeline has focus;
text fields retain their own text-edit shortcuts. History retains up to 64 actions
and 32 MiB of snapshot payload. Other document edits, observed external changes,
and unload/reload clear it. History checks detect observed external changes, but
do not provide atomic multi-writer editing. Graph curves and saving are not yet
provided by this milestone.

The lab has no required Input System package dependency. It binds optional input
controls when installed and uses legacy input when enabled; timeline buttons and
IMGUI keyboard handling remain available without either polling path. The optional
Input System reflection bindings have sample-only linker preservation.
