# WASM Standalone Viewer Implementation Plan

**Date:** 2026-06-05
**Subject:** Implementation plan for a browser standalone IMM viewer with a WASM performance core
**Related report:** `WASM_FEASIBILITY_REPORT.md`
**Status:** Planning - fastest prototype route selected

---

## Summary

The feasibility report's direction is broadly valid: the performance-critical runtime should start from the existing player core, the OpenGL ES renderer, null audio, synchronous loading, and a new Emscripten build. The main correction is that the current GLES backend is not yet a browser-ready WebGL2 backend. It has stubbed render target/readback functions and uses OpenGL ES 3.1/3.2-style APIs that WebGL2 does not expose directly.

The viewer should not be a full C++ port of the native standalone app. Only code that is performance critical, format/runtime-specific, or already deeply embedded in the native IMM stack should live in WASM. TypeScript/JavaScript should own the browser application shell, controls, file selection, layout, state presentation, browser APIs, and any logic that is easier and safer to maintain outside the C++ runtime.

The fastest credible prototype is a constrained mono WebGL2 viewer that proves `sample1.imm` can load and render in a browser through a small WASM core and a TypeScript/JavaScript shell. This should force the pretessellated paint path for the browser target, because the native/static paint path uses shader storage buffers that WebGL2 does not support. Audio, WebXR, WebGPU, streaming, worker loading, and stereo/multiview should remain out of scope until that baseline is stable.

The implementation must be isolated so existing Windows, Android, macOS, Unity, and Godot paths keep their current behavior. WASM-specific behavior should be behind new CMake targets, new files, and explicit compile definitions such as `__EMSCRIPTEN__` or `IMM_WASM`.

---

## Repo Anchors

- `code/appImmViewer/` is the standalone viewer app.
- `code/appImmViewer/src/viewer/viewer.cpp` is the reusable viewer layer over `ImmPlayer::Player`.
- `code/appImmViewer/src/viewer/viewer.cpp:27` loads configured content in `Viewer::Init`.
- `code/appImmViewer/src/viewer/viewer.cpp:249` drives per-frame `Viewer::GlobalWork`.
- `code/appImmViewer/src/viewer/viewer.cpp:337` drives `Viewer::GlobalRender`.
- `code/appImmViewer/src/viewer/viewer.cpp:444` drives mono rendering through `Viewer::RenderMono`.
- `code/libImmCore/src/libRender/piRenderer.cpp:26` currently does not select the GLES renderer for `__EMSCRIPTEN__`.
- `code/libImmCore/src/libRender/opengles/piGLES_Renderer.cpp:602` has stubbed `CreateRenderTarget`.
- `code/libImmCore/src/libRender/opengles/piGLES_Renderer.cpp:627` has stubbed `SetRenderTarget`.
- `code/libImmCore/src/libRender/opengles/piGLES_Renderer.cpp:1978` has stubbed texture readback.
- `code/projects/macos/CMakeLists.txt:58` contains reusable source-list precedent for core/importer/player CMake targets.

---

## MVP Scope

Build a browser-hosted standalone viewer with:

- Mono rendering only.
- WebGL2 through Emscripten GLES.
- WASM limited to the IMM runtime, importer/decoder paths that are needed for playback, renderer, and a thin C ABI.
- TypeScript/JavaScript app shell for UI, controls, browser event handling, file handling, state display, and orchestration.
- Pretessellated paint forced for the WASM target.
- Null audio backend in WASM initially.
- TypeScript/JavaScript-mediated file loading into WASM memory or MEMFS.
- Synchronous WASM-side decode/load initially.
- Bundled `exampleImmFiles/sample1.imm` as the first smoke asset.
- Browser canvas resize support.
- Basic mouse/touch camera controls.
- Basic play/pause and chapter controls.
- Console/log-file diagnostics sufficient to diagnose browser startup and render failures.

Explicitly defer:

- WebXR.
- Stereo/multiview.
- WebGPU.
- Web Audio.
- HTTP streaming/range requests.
- Web Workers.
- Emscripten pthreads.
- NPM packaging.

---

## Responsibility Split

### Keep in WASM/C++

- IMM importer and binary parsing.
- Compression/image/audio decode paths already used by the native runtime, where needed for playback.
- `ImmPlayer::Player` runtime state, scene update, animation sampling, layer evaluation, and render command generation.
- WebGL-facing renderer backend and GPU resource management.
- Performance-sensitive geometry/paint processing.
- Minimal camera/render entry points that accept matrices, viewport size, and timing.
- Minimal playback control functions: load, unload, play, pause, seek/chapter navigation, state query.
- WASM-only renderer compatibility shims needed to make GLES code run on WebGL2.

### Keep in TypeScript/JavaScript

- Browser app lifecycle.
- HTML/CSS layout and viewer controls.
- Drag/drop and file picker.
- Fetching bundled or remote `.imm` files.
- Copying file bytes into WASM memory or MEMFS.
- Loading/error/progress UI.
- Mouse, touch, pointer-lock, keyboard, and accessibility handling.
- Canvas resize and device-pixel-ratio management.
- URL/query-string routing and local storage preferences.
- Screenshot capture through browser APIs where possible.
- Test harness glue for Playwright.

### Avoid Porting from Native Standalone

- Native window management.
- Native message loop.
- Native settings-file UX.
- Windows/macOS-specific capture scripts.
- VR/Oculus/OpenXR shell code.
- Native filesystem assumptions beyond the small WASM file bridge.
- Static paint as the first browser paint path.

---

## Regression-Avoidance Rules

1. Do not change native defaults.
   - Windows/macOS/Android viewer settings should keep their current rendering technique defaults.
   - WASM may force pretessellated paint through its own init config or compile-time target only.

2. Keep WASM source additions additive.
   - Prefer new files under `code/projects/wasm`, `code/libImmCore/src/libBasics/wasm`, and `code/appImmViewer/src/wasm`.
   - Avoid editing shared native app entry points unless the change is behind `#if defined(__EMSCRIPTEN__)` or a new explicit define.

3. Keep renderer changes capability-gated.
   - Shared GLES renderer changes must preserve Android GLES behavior.
   - Browser-specific fallbacks should be guarded by `__EMSCRIPTEN__` or helper functions selected only for WASM.
   - Do not remove ES 3.1/3.2 paths that Android may rely on.

4. Preserve native threading behavior.
   - Single-threaded loading should be enabled only for WASM through `IMM_SINGLE_THREADED_LOADING` or equivalent.
   - Detached thread behavior should remain unchanged for existing native targets.

5. Preserve shader generation for native targets.
   - WASM shader fixes should generate separate WebGL2 variants where needed.
   - Do not weaken desktop, Android, Metal, Vulkan, or DirectX shader paths to satisfy WebGL2.

6. Add targeted validation before merging broad changes.
   - Run or at least preserve the existing native smoke scripts/targets.
   - Add local compile checks for native projects touched by shared code.
   - Add a WASM smoke separately; do not replace existing validation.

7. Treat `.gitignore` local changes as user-owned local hacks.
   - Do not commit `.gitignore` changes unless explicitly requested.

---

## Implementation Phases

### Phase 0: Baseline Audit

1. Build and run an existing native standalone smoke so there is a current reference for `sample1.imm`.
2. Record expected baseline signals:
   - document reaches loaded state,
   - nonzero draw calls,
   - nonzero visible pixels,
   - expected picture/paint draw counts if available.
3. Confirm the intended MVP paint path: `Drawing::PaintRenderingTechnique::Pretessellated`.
4. Measure or log the native pretessellated baseline for `sample1.imm`.
5. Confirm whether the pretessellated path for `sample1.imm` exercises any remaining non-WebGL2 renderer path.
6. Keep the static paint path unchanged for native targets.

### Phase 1: WASM Core Target

1. Add `code/projects/wasm/CMakeLists.txt`.
2. Model target organization after `code/projects/macos/CMakeLists.txt`, but trim it to the runtime pieces:
   - `libImmCore`,
   - `libImmImporter`,
   - `libImmPlayer`,
   - a small WASM module target such as `ImmViewerCoreWasm`.
3. Reuse the existing shader-generation CMake targets for GLES shader headers.
4. Configure with:

```bash
emcmake cmake -B build/wasm code/projects/wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build/wasm --config Release
```

5. Initial Emscripten link flags:

```cmake
-s USE_WEBGL2=1
-s FULL_ES3=1
-s WASM=1
-s ALLOW_MEMORY_GROWTH=1
-s MODULARIZE=1
-s EXPORT_NAME=ImmViewerModule
```

6. Do not enable pthreads in the MVP.
7. Do not port the full native `appImmViewer` loop into C++. The browser shell should call a narrow exported API.
8. Add an explicit target compile definition such as `IMM_WASM=1`.
9. Keep all WASM build flags local to `code/projects/wasm/CMakeLists.txt`.

### Phase 2: WASM Platform Layer

1. Add `code/libImmCore/src/libBasics/wasm/`.
2. Implement `piFileOS.cpp` using normal C/POSIX-style APIs (`fopen`, `stat`, `access`, `mkdir`) so Emscripten maps file access to MEMFS.
3. Implement `piTimer.cpp` using `emscripten_get_now()` or `std::chrono`.
4. Implement `piLog.cpp` to write to browser console, and optionally `/debug.txt` in MEMFS.
5. Implement `piMutex.cpp` conservatively. Prefer real `std::mutex` if it compiles cleanly; use no-op only if the single-threaded build proves all access is same-thread.
6. Implement `piThread.cpp` as unsupported or compile-excluded for WASM phase 1.
7. Implement `piSystemInfo.cpp` with browser-safe defaults.
8. Avoid `piWindow` in the browser shell unless a minimal stub is needed to satisfy linkage.
9. Do not reuse `windows`, `android`, or `macos` platform files through conditional hacks. Add WASM files or share only clearly portable code.

### Phase 3: Renderer Routing

1. Update `code/libImmCore/src/libRender/piRenderer.cpp`.
2. Under `__EMSCRIPTEN__`, include `opengles/piGLES_Renderer.h`.
3. Return `new piRendererGLES()` for `piRenderer::API::GLES`.
4. Keep this isolated from Android, Windows, Apple, Metal, Vulkan, and desktop GL paths.
5. Do not change existing platform renderer selection behavior.

### Phase 4: WebGL2 Compatibility Pass

The GLES renderer needs a deliberate WebGL2 pass before the viewer can be reliable in browsers.

1. Header selection:
   - Use Emscripten GLES headers under `__EMSCRIPTEN__`.
   - Avoid local Android-style GLES headers for browser builds if they conflict.
   - Preserve Android header behavior.

2. Unsupported or risky constants/extensions:
   - Map `GL_CLAMP_TO_BORDER` to `GL_CLAMP_TO_EDGE` for WebGL2.
   - Disable or guard timer query paths unless the required browser extension is present.
   - Guard `glGetStringi` and extension enumeration.

3. Unsupported WebGL2 features:
   - Remove or compile-gate `GL_SHADER_STORAGE_BUFFER` paths.
   - Remove or compile-gate compute shader dispatch.
   - Remove or compile-gate image load/store and atomics.
   - Disable multiview until WebXR/stereo work starts.
   - Do not delete native support for these features; compile-gate the WASM path.

4. Vertex input fallbacks:
   - Replace `glBindVertexBuffer` and `glVertexAttribFormat` usage with `glBindBuffer` plus `glVertexAttribPointer` for WebGL2.
   - Replace `glDrawElementsInstancedBaseVertex` usage with a no-base-vertex path, or pre-adjust index/vertex data where needed.
   - Keep Android/native GLES paths on their existing code path unless testing proves the fallback is equivalent.

5. Render target implementation:
   - Implement `CreateRenderTarget`.
   - Make `DestroyRenderTarget` null-safe.
   - Implement `SetRenderTarget`.
   - Implement color/depth attachment setup.
   - Implement `BlitRenderTarget` only if resolve/capture needs it for the first smoke.

6. Readback:
   - Implement minimal `GetTextureContent` by binding the texture to a framebuffer and using `glReadPixels`.
   - Use this for automated nonblank validation once the viewer renders.

7. Paint path:
   - Force pretessellated paint for WASM.
   - Do not attempt to support static paint in the first WebGL2 prototype.
   - Keep the static paint renderer compiled and used by native targets as it is today.

### Phase 5: Single-Threaded Loading

1. Add a compile-time mode such as `IMM_SINGLE_THREADED_LOADING`.
2. In `code/libImmPlayer/src/document.cpp`, replace detached `std::thread` loading with direct state-machine progression for WASM.
3. In `code/libImmImporter/src/fromImmersive/fromImmersive.cpp`, disable detached asset-loading threads for WASM or force immediate loading.
4. Keep `Player::mMutex` initially. Removing it is not necessary for single-threaded WASM and risks unrelated behavior changes.
5. Preserve native threaded behavior for Windows, Android, and macOS.
6. Add small comments at each WASM-only branch explaining that it exists because browser MVP builds do not use pthreads.

### Phase 6: WASM API Boundary

1. Add a small C++ WASM bridge, for example `code/appImmViewer/src/wasm/imm_viewer_core_wasm.cpp`.
2. Keep this bridge independent of `piWindow` and native viewer entry points.
3. Create or adopt the WebGL2 context from the canvas supplied by JavaScript.
4. Reuse `ExePlayer::Viewer` only where it does not drag in native window/event assumptions. If `Viewer` is too native-shell-shaped, use `ImmPlayer::Player` directly behind the bridge.
5. The C++ bridge should expose a narrow, stable C ABI or embind surface:
   - `imm_init(canvasSelectorOrId, width, height)`,
   - `imm_shutdown()`,
   - `imm_load_bytes(ptr, len, namePtr)`,
   - `imm_unload()`,
   - `imm_frame(deltaSeconds, width, height, cameraStatePtr)`,
   - `imm_play()`,
   - `imm_pause()`,
   - `imm_toggle_playback()`,
   - `imm_next_chapter()`,
   - `imm_prev_chapter()`,
   - `imm_get_state(outStatePtr)`,
   - `imm_get_last_error(outBufferPtr, outBufferSize)`.
6. Prefer JavaScript-owned frame scheduling via `requestAnimationFrame`; each frame calls `imm_frame(...)`.
7. Keep camera integration simple:
   - TypeScript owns input accumulation.
   - TypeScript passes camera transform or deltas to WASM.
   - WASM applies camera state only where required for player/render matrices.
8. Keep settings minimal:
   - TypeScript owns user-facing settings.
   - WASM receives a compact init config, not a browser clone of the native settings file.
   - WASM init config forces pretessellated paint.
   - Native app settings parsing remains unchanged.

### Phase 7: TypeScript/JavaScript Viewer Shell

1. Add `code/appImmViewer/wasm/index.html`.
2. Add `code/appImmViewer/wasm/viewer.ts` or `viewer.js`. Prefer TypeScript if the repo is ready to add a small web build step; otherwise start with JavaScript and keep the API typed in comments or `.d.ts`.
3. TypeScript/JavaScript responsibilities:
   - module boot,
   - canvas setup,
   - device-pixel-ratio resize,
   - fetch/preload for `sample1.imm`,
   - drag/drop and file picker,
   - copying bytes into WASM memory,
   - optional MEMFS file creation only if the C++ side requires file paths,
   - mouse/touch input mapping,
   - play/pause/prev/next controls,
   - loading/error state display.
4. Keep UI minimal and functional; the first milestone is render correctness.
5. Prefer byte-buffer loading over path-based MEMFS loading for browser-supplied files. This avoids carrying native filesystem assumptions into the browser shell.
6. Use browser APIs directly for non-critical features:
   - `fetch` for bundled/remote assets,
   - `FileReader` or `Blob.arrayBuffer()` for local files,
   - `requestAnimationFrame` for frame timing,
   - canvas APIs for screenshots where possible,
   - `localStorage` or IndexedDB for preferences/caches when needed.

### Phase 8: Validation

1. Add a WASM/browser smoke target/script once the TypeScript/JavaScript shell starts reliably.
2. Initial smoke should verify:
   - JavaScript shell loads,
   - WASM module loads,
   - WebGL2 context exists,
   - `sample1.imm` reaches loaded state,
   - at least one frame renders nonzero pixels,
   - draw call count exceeds a small threshold,
   - browser console does not contain shader compile/link failures.
3. Once `GetTextureContent` works, port the existing native nonblank/readback validation logic.
4. Add Playwright-based browser validation after the first manual browser smoke is stable.
5. Keep Playwright and browser automation in TypeScript/JavaScript; WASM should only expose enough state/readback to make validation deterministic.
6. Native regression checks for shared edits:
   - If `piRenderer.cpp`, `piGLES_Renderer.cpp`, `document.cpp`, importer loading, or shader generation changes, run the smallest available native compile/smoke for affected platforms.
   - On Windows, preserve the standalone Vulkan/OpenGL smoke scripts.
   - On macOS, preserve `appImmViewerMetal` validation targets.
   - For Godot/Unity bridge changes, run existing local verification scripts where available.

---

## Phase 2+ Enhancements

After the MVP renders:

1. Add browser file upload and drag/drop.
2. Add IDBFS caching for previously loaded files.
3. Implement a Web Audio backend.
4. Add progress reporting by chunking importer/player work across frames.
5. Add Web Worker loading only after measuring load stalls and confirming the complexity is justified.
6. Add TypeScript-friendly API wrappers and `.d.ts` typings.
7. Add optional NPM packaging for the JavaScript shell plus WASM artifact.
8. Investigate WebGPU as a high-fidelity renderer path after the WebGL2 prototype proves loading, player state, input, and browser packaging.

---

## Phase 3+ Enhancements

Longer-term work:

1. WebGPU renderer prototype for higher-fidelity paint and modern mobile GPUs.
2. WebXR presentation.
3. Stereo/multiview paths where browser support allows.
4. HTTP range/streaming loader for large IMM files.
5. Pthreads only if measurements justify cross-origin isolation and deployment complexity.

---

## Main Risks

| Risk | Impact | Notes |
|------|--------|-------|
| WebGL2 feature mismatch | High | Current GLES code uses ES 3.1/3.2 style calls and has stubbed render target/readback paths. |
| Static paint incompatibility | High | Static paint uses SSBO/std430 shader storage, which WebGL2 does not support. MVP must force pretessellated paint. |
| Native regressions | High | Renderer and loading changes touch shared code. Guard every WASM-specific behavior and keep native validation intact. |
| Shader compatibility | Medium | GLES shader assets exist, but browser compilation may expose unsupported extensions or layout assumptions. |
| File loading stalls | Medium | Synchronous loading is acceptable for MVP but may be visibly blocking on large files. |
| Memory growth/performance | Medium | Emscripten heap growth should be enabled initially, then tuned after measurements. |
| Validation gap | Medium | Browser validation needs readback or Playwright screenshot checks to catch blank-frame regressions. |
| Over-porting native shell | Medium | Keep browser UI, file handling, and frame scheduling in TypeScript/JavaScript so WASM stays limited to performance-critical runtime work. |
| WebGPU distraction | Medium | WebGPU may be better long term, but building it first delays a visible prototype. Keep it post-MVP. |

---

## Suggested Milestones

### Milestone 1: Runtime Build Skeleton

- `ImmViewerCoreWasm` compiles and links.
- Browser page loads the module.
- Console logging works.
- Renderer factory returns GLES on Emscripten.
- TypeScript/JavaScript shell can call a trivial exported function.
- Native renderer selection remains unchanged.

### Milestone 2: WebGL2 Renderer Baseline

- WebGL2 context initializes.
- GLES renderer creates buffers/VAOs without WebGL errors.
- A hardcoded clear color renders to canvas.
- Basic render target creation and switching works.

### Milestone 3: Load IMM

- `sample1.imm` is available in MEMFS.
- Player loads the file without threads.
- Loading state reaches `Loaded`.
- Failures are visible in browser logs.
- WASM path forces pretessellated paint.
- Native threaded loading remains unchanged.

### Milestone 4: First Real Frame

- `sample1.imm` renders nonblank pixels.
- Draw calls and triangles are nonzero.
- No shader compile/link failures.
- Mono camera movement works.
- Native static paint path is still available and unchanged.

### Milestone 5: Browser Viewer MVP

- File upload works.
- Play/pause and chapter navigation work.
- Canvas resize works.
- Browser smoke test validates loaded state and nonblank output.
- TypeScript/JavaScript owns browser interaction and calls the WASM runtime through the narrow API.

---

## Recommended First Work Item

Start with `code/projects/wasm/CMakeLists.txt`, minimal `libBasics/wasm` platform files, and a tiny exported runtime function callable from TypeScript/JavaScript. Then make `piRenderer::Create(API::GLES)` work under `__EMSCRIPTEN__` and force pretessellated paint through the WASM init config only. This will reveal compile-time incompatibilities quickly while preserving the intended split: WASM for the IMM runtime and renderer, TypeScript/JavaScript for the browser viewer shell.

Do not start by rewriting shared renderer or loading code broadly. Add the WASM target first, let the compiler identify the smallest required shared edits, and gate each shared edit so native paths continue using their existing code.
