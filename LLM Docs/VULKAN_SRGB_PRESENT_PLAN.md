# Vulkan Output Encoding Plan

## Problem

The standalone renderer produces linear scene color in a `C3_11_11_10_FLOAT`
texture. The reference OpenGL standalone path runs the app resolve shader at the
final output boundary; that shader averages MSAA samples and applies
`linear2srgb()` before the image reaches the window.

The Vulkan standalone path was presenting the linear B10G11R11 target directly,
so linear values were displayed as if they were already display encoded. That
made Vulkan much darker than OpenGL. A CPU readback conversion proved the color
transform but was too slow for playback.

## Target Architecture

1. Keep all OIT/MSAA rendering linear.
2. Do not change render target formats, sample counts, blending, OIT
   accumulation, or the renderer's existing final MSAA resolve boundary.
3. Make final output encoding explicit with a resolve-level flag:
   `OutputEncoding::Linear` or `OutputEncoding::DisplaySrgb`.
4. For standalone presentation, request `DisplaySrgb`.
5. For host engines such as Unity and Godot, keep the door open for `Linear`
   output when the host compositor owns display encoding.
6. In Vulkan, consume the resolve output intent when the resolve target is the
   swapchain. The backend can then route the final linear texture through the
   GPU sRGB present pass instead of accidentally presenting the OIT render target
   as linear display values.

## Implementation Steps

1. Add `Resolve::OutputEncoding` to the standalone resolve helper.
2. Pass the selected output encoding into the resolve shader options as
   `OUTPUT_ENCODING`.
3. Guard the resolve shader's `linear2srgb()` call with `OUTPUT_ENCODING==1`.
4. Request `Resolve::OutputEncoding::DisplaySrgb` from the standalone viewer.
5. Regenerate the HLSL resolve fragment shader include with
   `OUTPUT_ENCODING=0..1` permutations and select the display-sRGB binary for
   standalone output.
6. In Vulkan `DrawUnitQuad_XY`, when the final resolve is targeting the
   swapchain and requests display-sRGB output, mark the attached resolve source
   as the pending present texture.
7. Keep the Vulkan GPU present pass as the backend mechanism that applies the
   final linear-to-sRGB transform when the pending present texture is linear
   B10G11R11.
8. Fix the Vulkan present pass orientation and resource teardown ordering.

## Validation

1. Build the Release standalone viewer.
2. Run `code/appImmViewer/scripts/run-vulkan-sample1-smoke.ps1 -Configuration Release`.
3. Confirm the log contains the sRGB GPU present marker and no Vulkan failure
   patterns.
4. Inspect the generated capture for corrected brightness.
5. Launch `sample1.imm` in the standalone Vulkan player and confirm the live
   image is upright and playback FPS is no longer CPU-readback limited.
6. Confirm the change did not alter OIT/MSAA internals: the only color-space
   decision is at the final resolve/present boundary.

## Follow-Up

Unity and Godot should get an explicit output-encoding contract instead of
inheriting standalone assumptions. Their desired mode depends on whether the
host image is expected to contain linear color for compositor encoding or
display-sRGB color for direct presentation.
