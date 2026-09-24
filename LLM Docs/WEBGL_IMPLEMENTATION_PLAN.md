# Unity WebGL Build Target Implementation Plan

## Objective

Add support for Unity's WebGL build target to the IMM Unity integration.

There are two practical support levels:

1. **Stroke-reader WebGL support**: load IMM data in WebGL and expose parsed strokes, layers, pictures, transforms, and chapter metadata to managed Unity code.
2. **Full IMM player WebGL support**: run the native IMM player/rendering plugin in a Unity WebGL build and render IMM documents through WebGL.

The stroke-reader path is the recommended first milestone because it avoids Unity native rendering plugin callbacks, graphics-device events, stereo rendering, native audio backends, and renderer state integration. The full player path is feasible only after the native codebase is cleanly buildable with Emscripten and after the render integration has been proven in a minimal Unity WebGL scene.

## Current State

The Unity sample project currently ships native plugin binaries for desktop/mobile platforms:

- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/x86_64/ImmUnityPlugin.dll`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/OSX/ImmUnityPlugin.bundle`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/Android/libs/arm64-v8a/libImmUnityPlugin.so`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64/ImmStrokeReader.dll`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/macOS/libImmStrokeReader.dylib`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/iOS/libImmStrokeReader.a`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/Android/arm64-v8a/libImmStrokeReader.so`

Unity WebGL cannot load those dynamic libraries at runtime. WebGL native calls must be linked into the generated WebAssembly module using Unity's Emscripten toolchain. C# bindings also need WebGL-specific `DllImport("__Internal")` declarations.

Important current code paths:

- Runtime player bindings: `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmNativePlugin.cs`
- Runtime player manager/render event integration: `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs`
- Native Unity rendering plugin: `code/appImmUnity/src/main.cpp`
- Runtime stroke-reader bindings: `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ImmStrokeReader.cs`
- Native stroke-reader entry point: `code/appImmStrokeReader/src/main.cpp`
- Shared importer code: `code/libImmImporter`
- Shared player/renderer code: `code/libImmPlayer`
- Shared core code: `code/libImmCore`

## Key Constraints

### WebGL Native Plugin Model

Unity WebGL does not use platform dynamic libraries such as `.dll`, `.dylib`, `.bundle`, or `.so`.

Native C/C++ code must be:

- Compiled with Unity-compatible Emscripten.
- Linked into the Unity WebGL build as source, object, bitcode, or static archive.
- Called from C# with `DllImport("__Internal")` in WebGL player builds.

### Browser Runtime Model

WebGL runs in a browser sandbox. The implementation must account for:

- No direct access to arbitrary local file paths.
- Browser-backed virtual filesystem behavior.
- Asynchronous browser downloads/uploads.
- Different thread support depending on Unity WebGL threading settings and browser headers.
- WebAudio instead of native desktop or Android audio APIs.
- WebGL/OpenGL ES shader and API limits.

### Current Native Player Shape

The full player plugin is built around Unity native plugin callbacks:

- `UnityPluginLoad`
- `UnityPluginUnload`
- `IUnityGraphics`
- `GetRenderEventFunc`
- `GL.IssuePluginEvent`
- `CommandBuffer.IssuePluginEvent`

This integration needs a dedicated WebGL proof-of-concept before committing to full player support. Even if the C++ compiles to Wasm, rendering may require WebGL-specific adjustments or a different managed/native call pattern.

## Recommended Roadmap

## Phase 0: Decide the First Supported Surface

### Recommendation

Implement `imm-stroke-reader` WebGL support first.

Reasons:

- It has no native Unity graphics plugin dependency.
- It can validate Emscripten compilation of the importer/core code.
- It enables useful WebGL workflows: load IMM, inspect layers, convert to Unity meshes/line renderers, and render using normal Unity materials.
- It creates a reusable WebGL build pipeline for the full player later.

### Deliverables

- A written support decision in `README.md` or package README.
- WebGL support matrix:
  - Stroke reader: targeted first.
  - Full native player: experimental/future milestone.
  - Exporter: separate follow-up if required.

## Phase 1: Add WebGL-Safe C# Bindings

### Files

- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Runtime/ImmStrokeReader.cs`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmNativePlugin.cs`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmExporter.cs`

### Work

Add a shared conditional plugin-name pattern:

```csharp
#if UNITY_WEBGL && !UNITY_EDITOR
private const string DllName = "__Internal";
#else
private const string DllName = "ImmStrokeReader";
#endif
```

Use the equivalent for `ImmUnityPlugin`:

```csharp
#if UNITY_WEBGL && !UNITY_EDITOR
private const string DllName = "__Internal";
#else
private const string DllName = "ImmUnityPlugin";
#endif
```

### Notes

- Keep the public managed API stable.
- Do not enable WebGL calls for APIs that are not implemented natively yet.
- For unimplemented full-player WebGL APIs, prefer clear runtime errors over silent no-ops.
- Avoid WebGL-specific behavior leaking into non-WebGL platforms.

### Validation

- Unity Editor still compiles for desktop.
- WebGL build compiles C# without `DllNotFoundException`-specific assumptions.
- Existing desktop/macOS/Android plugin names remain unchanged.

## Phase 2: Create the Emscripten Build Pipeline

### Files To Add

Proposed:

- `code/appImmStrokeReader/Projects/WebGL/CMakeLists.txt`
- `code/appImmStrokeReader/Projects/WebGL/build_webgl.sh`
- `code/appImmUnity/Projects/WebGL/CMakeLists.txt` later, for full player support.
- `code/appImmUnity/Projects/WebGL/build_webgl.sh` later.

### Output Locations

For stroke-reader:

- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/WebGL/libImmStrokeReader.a`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/WebGL/libImmStrokeReader.a.meta`

For full player:

- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/WebGL/libImmUnityPlugin.a`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/WebGL/libImmUnityPlugin.a.meta`

### Build Rules

- Use Unity's bundled Emscripten version, not an arbitrary system Emscripten, unless the Unity version explicitly supports it.
- Build static archives with exported C symbols preserved.
- Use the same ABI assumptions as Unity WebGL:
  - WebAssembly target.
  - No exceptions unless required.
  - No RTTI unless required.
  - No unsupported native platform libraries.
  - No dynamic library loading.

### Initial Compiler Flags

Start conservative:

```sh
emcmake cmake -S . -B build-webgl \
  -DCMAKE_BUILD_TYPE=Release \
  -DIMM_WEBGL=ON

cmake --build build-webgl --config Release
```

Then tighten once compiling:

- `-fvisibility=default` where exported C ABI symbols require it.
- `-s WASM=1`
- `-s ALLOW_MEMORY_GROWTH=1` if large IMM files exceed initial memory.
- `-s USE_PTHREADS=1` only if Unity WebGL threading is intentionally enabled and the deployment can set COOP/COEP headers.

### Validation

- Static archive is generated reproducibly from a clean checkout.
- Build script fails on missing Unity Emscripten path with a clear message.
- Archive can be linked by a minimal Unity WebGL build.

## Phase 3: Port `imm-stroke-reader` Native Code

### Scope

Target:

- `code/appImmStrokeReader/src/main.cpp`
- `code/libImmImporter`
- Required parts of `code/libImmCore`
- Required image/audio decode dependencies only where used by importer APIs.

### Work

1. Add `IMM_WEBGL` / `__EMSCRIPTEN__` conditionals where platform APIs are used.
2. Remove or replace desktop-only logging paths.
3. Ensure `StrokeReader_LoadFromMemory` is the primary WebGL loading path.
4. Keep `StrokeReader_LoadFromFile` either:
   - Disabled with a documented error code, or
   - Implemented against Emscripten's virtual filesystem only.
5. Audit all returned strings and structs for WebGL marshalling compatibility.
6. Confirm all exported functions use `extern "C"` and default visibility.

### Likely Issues To Fix

- File APIs that assume native absolute paths.
- Wide string path handling.
- Third-party libraries that only have Windows project files or desktop-oriented build scripts.
- Any dependency on OS-level temp directories.
- Unbounded memory allocations for large IMM payloads.

### Validation

Create a WebGL sample scene or test harness that:

- Loads `exampleImmFiles/sample1.imm` as bytes.
- Calls `StrokeReader_Init`.
- Calls `StrokeReader_LoadFromMemory`.
- Reads layer count.
- Reads stroke counts.
- Reads at least one stroke point buffer.
- Unloads and ends cleanly.

## Phase 4: Add Unity WebGL Plugin Import Metadata

### Files

- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/WebGL`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/WebGL` later.

### Work

- Add `.meta` files that include the static archive only for WebGL.
- Ensure desktop/mobile binaries are excluded from WebGL.
- Ensure WebGL archives are excluded from Editor/Standalone/iOS/Android.
- Confirm package `asmdef` files do not block WebGL compilation.

### Validation

- Unity Plugin Inspector shows WebGL-only compatibility for WebGL archives.
- WebGL build does not try to include `.dll`, `.dylib`, `.bundle`, or `.so`.
- Desktop Editor still loads the existing native binaries.

## Phase 5: Browser File Loading

### Stroke Reader

Preferred WebGL API:

- Load `.imm` files as `TextAsset` / `byte[]`.
- Pass bytes to `StrokeReader_LoadFromMemory`.

Optional browser upload:

- Add a small `.jslib` bridge for `<input type="file">`.
- Copy selected file bytes into Wasm heap.
- Call a managed callback that then calls `StrokeReader_LoadFromMemory`.

### Full Player

Preferred WebGL API:

- Use `ImmPlayerManager.LoadDocumentFromMemory(byte[] data, string fileName)`.
- Avoid `LoadDocument(string filePath)` in WebGL unless the file is first written to Emscripten FS.

### Work

- Add WebGL guards around file-path APIs.
- Update samples to demonstrate memory loading.
- Document that arbitrary local file paths are unsupported in browser builds.

### Validation

- Build works with a bundled `.imm` asset.
- Build works with user-selected `.imm` file if upload bridge is implemented.
- Failed loads report actionable messages in browser console and Unity logs.

## Phase 6: Full Player WebGL Feasibility Spike

Do this only after stroke-reader support is working.

### Files

- `code/appImmUnity/src/main.cpp`
- `code/libImmPlayer`
- `code/libImmCore/src/libRender`
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs`

### Goals

Prove a minimal WebGL render path:

- Initialize plugin.
- Load one IMM from memory.
- Set mono camera matrices.
- Render one frame in a Unity WebGL build.
- Avoid stereo/XR initially.
- Avoid native audio initially.

### Native Work

1. Add an explicit WebGL branch in `main.cpp`.
2. Replace the current non-Windows/non-Android default path that assumes macOS OpenGL Core.
3. Create `piRenderer::API::GLES` or a dedicated WebGL renderer path if the GLES renderer is compatible.
4. Audit GL calls for WebGL 2 compatibility.
5. Audit shaders for WebGL GLSL ES compatibility.
6. Replace native logging with browser-safe logging.
7. Use null audio backend at first.
8. Disable unsupported stereo modes initially.

### Managed Work

1. Guard `GL.IssuePluginEvent` usage if Unity WebGL does not support the current callback path.
2. If needed, add a direct WebGL render-call path:
   - `ImmNativePlugin.Render(cameraId, eventId)` or similar.
   - Called from `OnPostRender`, SRP callback, or command buffer-compatible path.
3. Add explicit WebGL capability flags:
   - `SupportsNativePlayer`
   - `SupportsNativeAudio`
   - `SupportsStereo`
   - `SupportsFilePaths`

### Validation

- A simple non-XR Unity WebGL scene renders visible IMM content.
- Browser console has no WebGL errors during steady-state rendering.
- WebGL canvas remains stable across resize.
- Document unload/reload works.
- No unmanaged heap growth across repeated load/unload cycles.

## Phase 7: Full Player Audio Support

### Initial Recommendation

Use the null audio backend for the first full-player WebGL milestone.

### Later Options

1. Decode audio in native Wasm and pass PCM to Unity `AudioClip`.
2. Decode audio in managed/browser code and use Unity/WebAudio playback.
3. Add a dedicated WebAudio backend.

### Work

- Identify all audio formats used by IMM content.
- Confirm Opus/Vorbis decode feasibility in Emscripten.
- Decide whether spatial audio is required for WebGL.
- Add feature flags for audio support.

### Validation

- Documents with sound do not fail to load when audio is disabled.
- Muting/volume APIs behave predictably.
- If audio is enabled, playback stays synchronized with document time.

## Phase 8: Threading And Async Loading

### Initial Recommendation

Use a single-threaded WebGL path first.

### Work

- Audit `Player.GlobalWork`, importer loading, and any worker-thread assumptions.
- Add synchronous or cooperative-loading fallback under `IMM_WEBGL`.
- Only enable pthreads after proving:
  - Unity project has WebGL threads enabled.
  - Hosting environment sets required cross-origin isolation headers.
  - Browser support is acceptable.

### Validation

- Load progress does not deadlock.
- Large files either load successfully or fail cleanly with memory guidance.
- Browser main thread stalls are measured and documented.

## Phase 9: Memory Budgeting

### Work

- Measure memory usage for representative IMM files.
- Set Unity WebGL memory settings intentionally.
- Consider `ALLOW_MEMORY_GROWTH` if supported by the Unity version in use.
- Add managed-side checks for oversized files before copying into unmanaged memory.
- Free pinned/allocated buffers immediately when native no longer needs them.

### Validation

- Repeated load/unload cycles do not steadily grow memory.
- Browser tab survives loading target-size IMM files.
- Out-of-memory failures are clear.

## Phase 10: CI And Build Verification

### Work

- Add a scriptable WebGL native build step.
- Add Unity batchmode WebGL build if Unity licensing/CI environment supports it.
- Add a lightweight post-build smoke test if possible:
  - Serve the WebGL build locally.
  - Open with Playwright or browser automation.
  - Assert expected Unity log messages.

### Validation

- Native WebGL archive builds in CI or documented local workflow.
- Unity WebGL sample builds.
- Sample loads one IMM file and emits expected layer/stroke counts.

## Proposed Milestones

### Milestone 1: WebGL Stroke Reader Prototype

Deliverables:

- WebGL C# binding switch to `__Internal`.
- Emscripten build script for `ImmStrokeReader`.
- WebGL plugin archive imported into Unity package.
- WebGL sample scene loads `sample1.imm` from memory.

Acceptance criteria:

- Unity WebGL build succeeds.
- Browser console reports successful stroke-reader init.
- Sample logs layer count and at least one stroke point.

### Milestone 2: WebGL Stroke Reader Productization

Deliverables:

- Robust memory loading.
- Browser upload sample or documented `TextAsset` workflow.
- Package README WebGL instructions.
- Platform support matrix.
- Basic automated validation.

Acceptance criteria:

- Multiple IMM samples load.
- Load failures are understandable.
- Desktop/mobile behavior remains unchanged.

### Milestone 3: Full Player WebGL Feasibility

Deliverables:

- Experimental WebGL build of `ImmUnityPlugin`.
- Minimal mono render path.
- Null audio.
- Memory loading only.

Acceptance criteria:

- A WebGL scene renders visible IMM content.
- Browser console has no repeated WebGL errors.
- Load/unload works at least once per run.

### Milestone 4: Full Player WebGL Stabilization

Deliverables:

- Render path integrated with Unity Built-in Render Pipeline and/or SRP.
- Resize handling.
- Document unload/reload stability.
- Clear unsupported-feature behavior.

Acceptance criteria:

- Representative IMM files render reliably.
- No material memory leaks across repeated loads.
- Feature limitations are documented.

## Risks

### High Risk

- Full native rendering plugin integration may not map cleanly to Unity WebGL.
- Existing GL/GLES renderer code may use APIs or shader features unavailable in WebGL.
- Threaded loading may need significant rework.
- Large IMM files may exceed practical WebGL memory limits.

### Medium Risk

- Third-party image/audio dependencies may need separate Emscripten ports.
- String/path marshalling may need WebGL-specific fixes.
- Unity version upgrades may change bundled Emscripten behavior.

### Low Risk

- C# `DllImport("__Internal")` changes are straightforward.
- Memory-based loading already exists in managed APIs.
- A null-audio first milestone keeps scope contained.

## Implementation Order

1. Add WebGL support notes and support matrix.
2. Add C# WebGL `DllName` conditionals for `ImmStrokeReader`.
3. Add Emscripten build files for `appImmStrokeReader`.
4. Compile importer/core dependencies for WebGL.
5. Add `Plugins/WebGL/libImmStrokeReader.a` and Unity `.meta`.
6. Build a WebGL stroke-reader sample.
7. Validate memory loading and layer/stroke queries.
8. Document WebGL stroke-reader usage.
9. Start full-player feasibility spike only after stroke-reader milestone is complete.

## Definition Of Done For Initial WebGL Support

Initial support should mean `imm-stroke-reader` works in Unity WebGL:

- WebGL build succeeds from a clean checkout after running documented native build steps.
- A browser-hosted Unity WebGL sample can load an IMM byte array.
- The sample can enumerate layers and strokes.
- Unsupported APIs fail with clear messages.
- Existing Windows, macOS, iOS, and Android plugin behavior is not regressed.

Full native-rendered IMM playback should remain explicitly marked experimental until its own acceptance criteria are met.
