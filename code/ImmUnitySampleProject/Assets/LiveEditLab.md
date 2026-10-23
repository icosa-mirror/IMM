# Live Edit Lab

1. Open the sample project in Unity. On Windows, use Direct3D12 for this URP sample;
   **IMM > Live Edit Lab > Restart Editor with Direct3D12 (Windows)** switches it.
   Then choose **IMM > Live Edit Lab > Open sample and play**.
2. In the Game view, browse to a real `.imm` file and choose **Load / Reload**.
3. Filter/select a layer by ID/path. Use the property controls to edit it. Watch the scene
   and the revision status: **Presented** means published; **Rejected** reports the
   native error. A busy/loading document cannot accept another edit yet.
4. Try the same operations while playing, paused and after seeking. Enter key
   times in seconds. Visibility keys have their own checkbox; opacity and transform
   keys use the displayed draft property values. Loop uses repeat `0` for true;
   Offset uses the repeat field as an exact integer tick count.
5. For geometry, select a paint layer and enter a frame index. **Replace with
   marker** replaces that frame's drawing with a magenta marker at the layer's local
   origin. A shared drawing changes in every frame that references it. Adjust the
   marker size and use **Frame scene** or fly the camera to find it.
6. Use an ordinary group as the selected parent and enter a new child name. Reparenting
   takes a destination layer ID and child index. Delete affects the entire selected
   subtree. Unsupported content is reported explicitly.
7. **Unload**, then **Load / Reload**, to restore the source scene. All edits affect
   memory only; this lab does not save or overwrite the IMM file. There is no undo
   stack yet.

The Game view opens maximized; Shift+Space toggles its size.
Right mouse + WASD flies the camera, Q/E moves down/up, and Shift moves faster.
F9 hides or shows the panel. Stop Play mode to close the session. Choose **IMM >
Live Edit Lab > Disable automatic panel** to keep the panel out of later Play sessions.

The lab calls the native live-edit API directly. It does not use compile/reload
preview or the sample's transient layer-override controls. General mixed-operation
transactions and nested timeline/resource-subtree visibility refresh remain unfinished;
they are not implied by the presence of these controls.
