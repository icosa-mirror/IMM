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

## Scope for this pass

1. Add `piRenderer::API::Vulkan` without changing existing defaults.
2. Add a Vulkan backend under `code/libImmCore/src/libRender/vulkan`.
3. Make the backend compile in the Windows standalone build first, then keep Android integration as a follow-up target.
4. Implement initialization, teardown, reporting, CPU-side resource wrappers, state tracking, query timing, viewport state, and unsupported-feature diagnostics.
5. Add enough Windows standalone player selection/build wiring to instantiate and verify the Vulkan backend locally.
6. Keep draw and shader paths explicitly diagnosed until the player shader pipeline is translated to SPIR-V and render pass/swapchain ownership is defined; this pass should establish the Vulkan backend skeleton and integration, not claim Metal-equivalent playback.

## Implementation tasks

1. [in progress] Create `piVulkan_Renderer.h/.cpp` with the full `piRenderer` implementation surface.
2. [in progress] Support two initialization paths:
   - externally supplied Vulkan handles through an opaque config struct;
   - self-created Vulkan instance/device/queue for smoke/build validation.
3. [pending] Add minimal Vulkan resource wrappers for textures, buffers, samplers, render targets, shaders, vertex arrays, fixed states, and CPU timing queries.
4. [pending] Add one-time reporter diagnostics for unsupported shader compilation, draw submission, render target operations, image bindings, compute, atomics, and pixel pack buffers.
5. [complete] Register `API::Vulkan` in `piRenderer::Create` for Windows and Android.
6. [pending] Update Windows project files to build the Vulkan source and link the Vulkan loader.
7. [pending] Add Windows standalone viewer selection for Vulkan.
8. [pending] Update shared bridge/Godot API names so Vulkan can be requested explicitly on Windows and Android while preserving existing defaults.
9. [pending] Verify with the closest available Windows standalone build.
10. [pending] Update Android CMake to build the Vulkan source and link `vulkan` after Windows verification.

## Follow-up work after this pass

- Define the host-frame contract for Vulkan swapchain images or Godot `RenderingDevice` textures.
- Add GLSL/HLSL-to-SPIR-V shader generation for player layer renderers.
- Implement descriptor set layout generation for constants, textures, samplers, and storage buffers.
- Implement render pass/framebuffer compatibility, pipeline cache keys, and draw submission.
- Add Android runtime smoke tests that validate initialization, selected API logging, and a simple nonblank Vulkan render once draw paths exist.
