# macOS Support Plan (Standalone Viewer + Unity Plugin)

This document captures the macOS port status and the next steps to make the standalone viewer run on macOS, based on the work completed in this session.

## Summary of current macOS status

- macOS build pipeline exists in `code/projects/macos/CMakeLists.txt`.
- Builds static libraries and the Unity plugin bundle, plus a new `appImmViewer` executable.
- Unity macOS plugin loads as `ImmUnityPlugin.bundle` and is copied into `Assets/Plugins/OSX/` by the mac build.
- Core platform layers (mutex, timer, file I/O, logging, threads, system info) are implemented for macOS.
- A macOS windowing + OpenGL context is implemented (`piWindow.mm`, `piGL4X_RenderContextOS.cpp`).
- The standalone viewer *builds* and launches on macOS, initializes OpenGL, but currently fails during Player init because the OpenGL shader path depends on features not available in macOS OpenGL 4.1 (SSBOs, GL 4.3+ features).

## Known macOS constraints

- Apple Silicon macOS supports OpenGL 4.1 only. No GL 4.3+ features.
- Current GL4x shaders in `libImmPlayer` use:
  - `layout(std430)` SSBOs
  - `GL_ARB_shader_draw_parameters`
  - explicit `layout(binding=...)` for uniforms/samplers
- These features fail to compile in GL 4.1, causing `LayerPaintRender` to fail and the viewer to exit.

## Changes already made (for reference)

### Build and project
- Added macOS CMake project: `code/projects/macos/CMakeLists.txt`
- Added macOS CI job in `.github/workflows/build.yml` (macOS build + upload `ImmUnityPlugin.bundle`)
- `.gitignore`: ignores macOS Unity `UserSettings/`, CMake/Ninja artifacts
- Removed accidental SDK include path (was breaking libc++ on GitHub runners)

### macOS platform layers (new files)
- `code/libImmCore/src/libBasics/macos/piMutex.cpp`
- `code/libImmCore/src/libBasics/macos/piThread.cpp`
- `code/libImmCore/src/libBasics/macos/piTimer.cpp`
- `code/libImmCore/src/libBasics/macos/piFileOS.cpp`
- `code/libImmCore/src/libBasics/macos/piLog.cpp`
- `code/libImmCore/src/libBasics/macos/piSystemInfo.cpp`
- `code/libImmCore/src/libBasics/macos/piWindow.mm`

### OpenGL context and renderer support for macOS
- `code/libImmCore/src/libRender/opengl4x/macos/piGL4X_RenderContextOS.cpp`
  - Creates/activates CGL context from window handle (CGLContextObj)
- OpenGL function loading on macOS uses `dlsym` (no wgl/glX)
- Added non-DSA fallback paths for macOS in:
  - buffer creation/mapping
  - vertex arrays
  - framebuffer creation/attachment
  - texture creation (2D/2D array)
  - bindless texture guard

### Unity plugin macOS naming and meta
- Bundle renamed to `ImmUnityPlugin.bundle` (matches `DllImport("ImmUnityPlugin")`).
- Added `Assets/Plugins/OSX/ImmUnityPlugin.bundle.meta` targeting macOS.
- Disabled macOS and Linux for Windows DLLs in `Assets/Plugins/x86_64/*.meta`.

### Standalone viewer macOS entry point
- Added `code/appImmViewer/src/macos/main.mm`.
- Added `appImmViewer` target to macOS CMake project.

### Shader compile fixes for macOS
- GLSL version forced to `#version 410 core` on Apple
- Removed GL 4.5-only extensions for mac (`GL_ARB_bindless_texture`, `GL_ARB_shader_draw_parameters`)
- Resolve shader now avoids explicit uniforms on Apple (uses uniform locations instead)

### Known runtime log output
Current `appImmViewer` launch shows:
- Renderer initialized OK
- Resolve initialized OK
- Player Init fails when LayerPaintRender tries to compile shaders

Error indicates shader compilation failure due to GL 4.1 limits:
- `layout(std430)` SSBOs
- `GL_ARB_shader_draw_parameters`
- binding qualifiers

## Required next work: GL 4.1 compatible renderer path

### Goal
Get `appImmViewer` to run on macOS OpenGL 4.1 by replacing GL 4.3+ shader features with GL 4.1-compatible equivalents.

### Recommended approach
Introduce a **GL41 compatibility path** for shaders and data bindings, focused on the paint renderer first.

#### Step 1 — Create a GL 4.1 capability switch
- Add a renderer feature flag: `SupportsFeature(RendererFeature::GL41Limited)` (or similar) on macOS.
- Decide at runtime if GL 4.1 path should be used.

#### Step 2 — Replace SSBO usage in paint renderers
- Files:
  - `code/libImmPlayer/src/layerRenderers/layerRendererPaint/static/shader_static_brush_vs.glsl`
  - `code/libImmPlayer/src/layerRenderers/layerRendererPaint/pretessellated/*`
- Replace `layout(std430) buffer VertexData` with one of:
  - **Texture buffer** (TBO): pack vertex data into a buffer texture and fetch via `texelFetch`.
  - **Texture2D** atlas: pack arrays into a 2D texture and index by vertex id.
- Add GL 4.1-compatible GLSL variants (`shader_static_brush_vs.gl41.glsl` etc.).

#### Step 3 — Remove `GL_ARB_shader_draw_parameters`
- Replace use of `gl_DrawID` / draw parameters in shaders.
- Feed draw parameters via uniform values or per-instance attributes.

#### Step 4 — Remove `layout(binding=...)` for samplers/uniforms on macOS
- Use uniform locations via `glGetUniformLocation`.
- Already added `GetShaderUniformLocation` to the renderer; use it in shader setup.

#### Step 5 — Restrict UBO layout qualifiers
- macOS GL 4.1 requires stricter layout rules.
- Remove `row_major` if needed; ensure `std140` layout is valid.

#### Step 6 — Compile-time shader selection
- Add shader variant selection by feature flag (`GL41` vs `GL45`).
- Example: use `shader_static_brush_vs.gl41.glsl` when GL 4.1 is detected.

#### Step 7 — Validate with minimal content
- Use `exampleImmFiles/sample1.imm` (already used in `settings.json`).
- Run `build/macos/viewer/appImmViewer` and confirm it stays open and renders.

## Optional: Metal renderer track (future)

- Implement Metal renderer backend (device, pipeline, buffers, textures)
- Create shader pipeline (MSL compilation or SPIR-V -> MSL translation)
- Use `IUnityGraphicsMetal` for Unity plugin support

## Unity plugin notes

- Unity Editor uses Metal by default on macOS; plugin supports OpenGL Core only.
- In Editor, plugin will fail unless OpenGL Core is forced (if available in the Unity version).
- A Metal backend is needed for full Unity Editor support on macOS.

## TODO Checklist

### Build + platform
- [ ] Keep macOS CMake build working in CI
- [ ] Ensure macOS bundle artifacts are copied to Unity sample project

### Standalone viewer runtime
- [ ] Add GL 4.1 capability flag and use it to select shader variants
- [ ] Implement GL 4.1 shader variants for paint renderers
- [ ] Replace SSBO usage with TBO/texture fallback
- [ ] Remove `GL_ARB_shader_draw_parameters` dependency
- [ ] Replace explicit uniform bindings with uniform location setup
- [ ] Confirm `appImmViewer` renders sample content on macOS

### Unity plugin (optional)
- [ ] Add Metal renderer backend (Unity Editor support)
- [ ] Verify plugin works on macOS Player builds with OpenGL Core (if supported)

## Build / run commands

```
cmake -S code/projects/macos -B build/macos
cmake --build build/macos --config Release
build/macos/viewer/appImmViewer
```

## Files to review for shader compatibility

- `code/libImmPlayer/src/layerRenderers/layerRendererPaint/static/shader_static_brush_vs.glsl`
- `code/libImmPlayer/src/layerRenderers/layerRendererPaint/pretessellated/*`
- `code/libImmPlayer/src/layerRenderers/layerRendererPicture/*`
- `code/libImmPlayer/src/layerRenderers/layerRendererModel/*`
