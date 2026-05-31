# Vulkan renderer plan

## Current renderer shape

- `piRenderer` is the shared rendering abstraction used by the player, Unity/Godot bridges, and standalone apps.
- Existing backends are selected through `piRenderer::API` and `piRenderer::Create`.
- Android currently defaults to GLES and builds `piGLES_Renderer.cpp` into `libImmCore`.
- The Metal backend is the best template for a new explicit-API backend: it owns backend state, supports externally-owned native frame/device integration, wraps renderer resources, implements the full `piRenderer` surface, and is working well enough for playback in the macOS standalone player, macOS Unity plugin, and macOS Godot plugin.

## Current learning

- Metal is a working playback backend used by the macOS standalone player, macOS Unity plugin, and macOS Godot plugin.
- Vulkan is a blocker for the Godot plugin on Windows and Android.
- The first target should be a Windows standalone Vulkan player because it is the easiest path to verify locally while building out backend behavior.
- No local `dxc`, `glslc`, `glslangValidator`, `shaderc`, or Vulkan SDK shader toolchain is currently available in PATH/vcpkg, so a generated SPIR-V path needs either new tooling or checked-in generated assets.
- To get standalone visual output sooner, the current implementation step is an interim Windows standalone CPU raster path for the static paint data exercised by `sample1.imm`, presented through the Vulkan-selected renderer until real Vulkan command buffers and pipelines are available.

## Acceptance target

- The Windows standalone player must render `exampleImmFiles/sample1.imm` through `RenderingAPI: "Vulkan"` with visible output comparable to the existing base standalone rendering path.
- Vulkan initialization-only smoke is not sufficient.
- Placeholder diagnostics for render target binding or draw submission are blockers, not acceptable completion criteria.

## Scope for this pass

1. Add `piRenderer::API::Vulkan` without changing existing defaults.
2. Add a Vulkan backend under `code/libImmCore/src/libRender/vulkan`.
3. Make the backend compile in the Windows standalone build first, then keep Android integration as a follow-up target.
4. Implement initialization, teardown, reporting, CPU-side resource wrappers, state tracking, query timing, viewport state, and unsupported-feature diagnostics.
5. Add enough Windows standalone player selection/build wiring to instantiate and verify the Vulkan backend locally.
6. Replace placeholder render target and draw submission paths with real Vulkan rendering for the standalone player.
7. Translate or provide the shader path needed by `sample1.imm` playback.

## Implementation tasks

1. [complete] Create `piVulkan_Renderer.h/.cpp` with the full `piRenderer` implementation surface.
2. [complete] Support two initialization paths:
   - externally supplied Vulkan handles through an opaque config struct;
   - self-created Vulkan instance/device/queue for smoke/build validation.
3. [complete] Add minimal Vulkan resource wrappers for textures, buffers, samplers, render targets, shaders, vertex arrays, fixed states, and CPU timing queries.
4. [complete] Add one-time reporter diagnostics for unsupported shader compilation, draw submission, render target operations, image bindings, compute, atomics, and pixel pack buffers.
5. [complete] Register `API::Vulkan` in `piRenderer::Create` for Windows and Android.
6. [complete] Update Windows project files to build the Vulkan source and link/load the Vulkan loader.
7. [complete] Add Windows standalone viewer selection for Vulkan.
8. [complete] Update shared bridge/Godot API names so Vulkan can be requested explicitly on Windows and Android while preserving existing defaults.
9. [in progress] Verify with the closest available Windows standalone build.
10. [complete] Update Android CMake to build the Vulkan source and load `libvulkan.so` dynamically after Windows verification.
11. [complete] Add a separate Windows standalone Vulkan smoke settings file without changing the default viewer config.
12. [in progress] Implement real Vulkan render targets, command buffers, render pass/framebuffer setup, and swapchain presentation for the Windows standalone viewer. The owned Windows path now creates a Win32 `VkSurfaceKHR`, selects a graphics queue that can present to it, enables `VK_KHR_swapchain`, creates a window-sized swapchain, and enumerates swapchain images; actual image acquisition, command submission, and presentation are still pending.
13. [pending] Implement or generate Vulkan-compatible shaders for the player paths exercised by `sample1.imm`.
14. [in progress] Verify standalone Vulkan output for `sample1.imm` against the existing base rendering behavior. Nonblank output is now proven, but the current capture is an early partial paint frame and is not yet comparable to the full base renderer.
15. [complete] Implement interim CPU raster output for static paint so `sample1.imm` becomes visible in the Windows standalone Vulkan-selected path before the full SPIR-V/pipeline path lands.

## Follow-up work after this pass

- Define the host-frame contract for Vulkan swapchain images or Godot `RenderingDevice` textures.
- Add GLSL/HLSL-to-SPIR-V shader generation for player layer renderers.
- Implement descriptor set layout generation for constants, textures, samplers, and storage buffers.
- Implement render pass/framebuffer compatibility, pipeline cache keys, and draw submission.
- Add Android runtime smoke tests that validate initialization, selected API logging, and a simple nonblank Vulkan render once draw paths exist.

## Verification notes

- Windows `libImmCore` Debug builds with `piVulkan_Renderer.cpp`.
- Windows `appImmViewer` Debug builds successfully with Vulkan selectable from settings.
- Existing OpenGL standalone smoke reached IMM CPU/GPU load and playback start before manual termination.
- Vulkan standalone smoke using `code/appImmViewer/exe/settings-vulkan-smoke.json` initialized an owned Vulkan device and loaded `sample1.imm`, but did not render before the interim paint/present work. The placeholder diagnostics for render target binding and draw submission are now tracked as blockers.
- Vulkan standalone smoke now reaches static paint CPU rasterization for `sample1.imm`: diagnostic `IMM_VK_CPU` reported projected paint vertices, inside-target samples, visible segments, max alpha 1.0, and a nonblack paint target. The generated capture is visible but currently only an early partial frame, so comparable full-output verification remains in progress.
- Current learning: rasterizing every triangle-strip index as a line sample is redundant and slow for static paint. The interim CPU path is being narrowed to unique source paint points per chunk and throttled presentation so the standalone player can progress toward a fuller frame.
- Current learning: the capture hook initially wrote only the first nonblack paint target, which is too weak for comparable-output verification. `IMM_VULKAN_CPU_CAPTURE_OVERWRITE` now allows smoke tests to keep the latest nonblack paint output.
- Latest Windows Vulkan smoke with overwrite capture ran `sample1.imm` for 45 seconds, produced a visibly fuller paint capture, and had no `not implemented`, render-target placeholder, or draw-submission diagnostics in `debug.txt`. Capture stats: 909,307 nonblack pixels, 127,345 near-visible pixels, max RGB 208,204,173.
- Base-renderer comparison is still not fully established locally: OpenGL and DirectX validation capture attempts both exited with `0x80000003` after import-time/runtime issues before producing a validation image. Comparable-output verification remains in progress until a stable base capture or a stronger visual invariant is available.
- Real Vulkan presentation progress: the Windows standalone owned-device path now requests `VK_KHR_surface`/`VK_KHR_win32_surface`, creates a Win32 `VkSurfaceKHR`, requires queue-family surface support, enables `VK_KHR_swapchain` on the logical device, creates a swapchain, and enumerates swapchain images.
- Android `:libImmCore:assembleDebug` builds successfully with `piVulkan_Renderer.cpp` included; CMake still emits the pre-existing dev warning about no top-level `project()` command.
