# libImmPlayer WebAssembly Feasibility Report

**Date:** 2026-01-04
**Subject:** Feasibility Assessment for Compiling libImmPlayer to WebAssembly
**Status:** Moderately Feasible with Significant Engineering Effort Required

---

## Executive Summary

Compiling libImmPlayer to WebAssembly (WASM) is **moderately feasible** but will require **significant engineering effort** (estimated 2-12 weeks depending on scope). The codebase has favorable characteristics including platform abstraction, multiple rendering backends (including OpenGL ES which maps to WebGL), and optional threading. However, challenges include platform-specific dependencies, threading model adaptation, file I/O, and wide character string usage.

**Key Finding:** Threading is NOT critical and can be removed or simplified for WASM, significantly reducing porting complexity.

---

## Project Overview

### What is libImmPlayer?

libImmPlayer is a reference playback engine for Immersive Media (IMM) files - an API-neutral runtime format for delivering 3D/VR animated content. It supports:

- Mixed media types (3D models, paint strokes, 360 panoramas, pictures, audio)
- Full 6DOF immersive playback
- Scenegraph and animation timeline
- Heavy compression for streaming
- VR platforms (Oculus Quest, Rift)

### Current Platform Support

- **Windows**: Primary platform (DirectX 11 + OpenGL 4.x)
- **Android**: Quest support (OpenGL ES)
- **Build System**: Visual Studio projects (.vcxproj)

### Module Architecture

```
libImmCore/         - OS services, rendering, sound, containers
  ├── libBasics/    - Platform abstraction (windows/, android/ dirs)
  ├── libRender/    - Multiple backends (directx11/, opengl4x/, opengles/)
  ├── libSound/     - Audio engines (includes NULL backend)
  └── libVR/        - VR SDK integration (Oculus, Vive)

libImmImporter/     - Reading IMM files
libImmPlayer/       - Playback engine and rendering
libImmExporter/     - Exporting to IMM format
```

---

## Current Technical Architecture

### Rendering Backends

| Backend | Path | WASM Compatibility |
|---------|------|-------------------|
| DirectX 11 | `libRender/directx11/` | ❌ Not available |
| OpenGL 4.x | `libRender/opengl4x/` | ⚠️ Desktop GL (not WebGL) |
| OpenGL ES | `libRender/opengles/` | ✅ Maps to WebGL |

**Key Advantage:** Already has OpenGL ES backend suitable for WebGL adaptation.

### Shader Pipeline

- **HLSL shaders** compiled with custom DX11ShaderCompiler tool
- **GLSL shaders** (desktop GL)
- **GLSL ES shaders** (.es.glsl files) ready for WebGL
- Build-time shader compilation generates `.inc` files

### Platform Abstraction

Platform-specific implementations exist in separate directories:

```
libBasics/
├── windows/
│   ├── piThread.cpp      - Uses CreateThread()
│   ├── piMutex.cpp       - Windows mutex
│   ├── piTimer.cpp       - Windows timers
│   ├── piFileOS.cpp      - Win32 file I/O
│   └── piLog.cpp
└── android/
    ├── piMutex.cpp       - pthread mutexes
    ├── piTimer.cpp       - POSIX timers
    └── piFileOS.cpp      - POSIX file I/O
```

**Implication:** Adding a `wasm/` or `emscripten/` directory is the natural extension pattern.

### Third-Party Dependencies

| Library | Version | WASM Port Available? |
|---------|---------|---------------------|
| libjpeg-turbo | 3.0.4 | ✅ Yes (vcpkg/emscripten) |
| libpng | 1.6.43 | ✅ Yes |
| zlib | 1.3.1 | ✅ Yes |
| libogg | 1.3.5 | ✅ Yes |
| libvorbis | 1.3.7 | ✅ Yes |
| opus | 1.5.2 | ✅ Yes |
| libopusenc | 0.2.1 | ✅ Yes |
| Facebook Audio360 SDK | 1.7.12 | ❌ Proprietary, optional |
| Oculus SDK | 32.0 | ❌ Not needed for rendering |
| Oculus Platform SDK | 81.0 | ❌ Not needed for rendering |

**Assessment:** Core dependencies have WASM ports. VR SDKs are optional.

---

## Threading Architecture Analysis

### Threading Usage Patterns

Threading is used in **two primary areas**:

#### 1. Asynchronous Asset Loading

**Location:** `libImmPlayer/src/document.cpp:230`

```cpp
std::thread loadingThread([this, log, soundEngine, ...]()
{
    if (!iLoadCPU(...)) {
        mState.mLoadingState = LoadingState::UnloadingCompleted;
        mState.mErrorState = ErrorState::FailedCPU;
    }
    else {
        mState.mLoadingState = LoadingState::LoadingSPU;
    }
});

loadingThread.detach();  // Fire-and-forget
```

**Purpose:**
- Loads IMM file data in background (geometry, textures, paint strokes)
- Prevents frame stutters during loading
- Progressive asset streaming during playback

**Communication:**
- Atomic state machine via `LoadingState` enum
- Mutex-protected state reads/writes (`mState.mMutex`)

**Also in:** `libImmImporter/src/fromImmersive/fromImmersive.cpp:326` (async asset streaming)

#### 2. Main/Render Thread Synchronization

**Location:** `libImmPlayer/src/player.cpp:726-809`, `player.h:349`

```cpp
// player.h:349
std::mutex mMutex; // to synch the main thread and the render thread.
                   // TODO: remove it - use double buffered rendering
```

```cpp
// player.cpp:726
mMutex.lock();
{
    // Update scenegraph, process commands, animation
    for (uint64_t currDocId = 0; currDocId < num; currDocId++) {
        // ... update document state ...
    }
}
mMutex.unlock();
```

**Purpose:**
- Protects scenegraph from concurrent access during rendering
- Separates main thread (updates) from render thread (drawing)

**Critical Insight:** The TODO comment indicates **threading was always meant to be optional**. The API design supports both patterns:

```cpp
// Main thread functions:
void GlobalWork(bool enabled, uint32_t microsecondsBudget);
void GlobalRender(const trans3d & vr_to_head, ...);

// Render thread functions (player.h:49):
// "this are the only functions that can be called from the render thread"
void RenderMono(const ivec2 & pixelResolution, int eyeID);
void RenderStereoSinglePass(...);
```

### Threading Criticality Assessment

| Aspect | Impact Without Threading | Workaround Difficulty |
|--------|-------------------------|----------------------|
| **Core Rendering** | ✅ No impact - works fine | N/A - not needed |
| **Asset Loading** | ⚠️ Blocks main thread during load | 🟢 Easy - show loading screen |
| **User Experience** | ⚠️ Freeze during load (100ms-2s) | 🟡 Medium - chunk loading |
| **Code Complexity** | ✅ Much simpler | N/A |

**Conclusion:** Threading is purely an optimization, not a requirement.

---

## WASM Feasibility Assessment

### ✅ Favorable Factors

1. **OpenGL ES Rendering Backend**
   - Path: `code/libImmCore/src/libRender/opengles/`
   - Already has WebGL-compatible GLSL ES shaders
   - Can be adapted to WebGL 1.0 or 2.0

2. **Platform Abstraction Layer**
   - Existing pattern: `windows/` and `android/` directories
   - Natural extension: add `wasm/` or `emscripten/` directory
   - Well-defined abstraction interfaces

3. **NULL Audio Backend**
   - `piSoundEngineNULL` available for initial port
   - Can stub out audio, add Web Audio API later

4. **Third-Party Dependency Support**
   - All core codecs have Emscripten ports
   - VR SDKs are optional for basic playback

5. **Optional Threading**
   - Can be removed entirely (see analysis above)
   - Simplifies WASM port significantly

6. **Clean API Surface**
   - Well-documented player interface
   - Separation of concerns (load, update, render)

### ⚠️ Challenging Factors

#### 1. Threading Model (Priority: Low - Solvable)

**Current Implementation:**
- Windows: `CreateThread()` API (code/libImmCore/src/libBasics/windows/piThread.cpp:39)
- Android: pthread
- Uses `std::mutex` and detached threads

**WASM Solutions:**

| Approach | Complexity | Performance | Recommendation |
|----------|-----------|-------------|----------------|
| Remove entirely | 🟢 Low | Blocking loads | ✅ Start here |
| Emscripten pthreads | 🔴 High | Best | Phase 2 only if needed |
| Web Workers (manual) | 🟡 Medium | Good | If loads are too slow |
| Frame-chunked loading | 🟡 Medium | Good | Progressive enhancement |

**Recommended:** Start with synchronous loading, optimize later if needed.

#### 2. File I/O System (Priority: Medium)

**Current Implementation:**
- Platform-specific `piFile` class
- Direct filesystem access expected
- Binary file streaming (`piIStreamFile`)

**WASM Solutions:**

| Method | Use Case | Implementation |
|--------|----------|----------------|
| Emscripten MEMFS | Small files in memory | ✅ Built-in |
| Emscripten IDBFS | Persistent storage | ✅ Built-in |
| Fetch API | HTTP streaming | Custom wrapper |
| Preloaded files | Bundle with app | Emscripten `--preload-file` |

**Recommended:** Start with MEMFS + preloaded files, add HTTP streaming later.

#### 3. Wide Character Strings (Priority: Medium)

**Current Usage:**
- Extensive `wchar_t` throughout codebase
- File paths, layer names, logging (player.h:204-205)
- WASM has limited wide char support

**Solutions:**
- UTF-8 conversion layer at API boundaries
- Emscripten's UTF8/UTF16 helpers
- Or: ifdef to `char*` for WASM builds (breaking change)

**Estimated Effort:** 2-3 days to add conversion layer

#### 4. Shader Compilation Pipeline (Priority: Low)

**Current System:**
- Build-time HLSL compilation via custom DX11ShaderCompiler
- Generates shader variants with preprocessor defines
- Output: `.inc` files embedded in binary

**For WASM:**
- Skip HLSL entirely, use GLSL ES variants
- Existing `.es.glsl` files already present
- May need runtime shader variant selection

**Solution:** Modify build to only include GLSL ES shaders for WASM target.

#### 5. Dependency Build System (Priority: High)

**Current:**
- Visual Studio .vcxproj files
- MSBuild-based
- Windows-centric paths and tools

**Needed for WASM:**
- CMake build system (Emscripten standard)
- Cross-platform dependency management
- Emscripten toolchain integration

**Estimated Effort:** 1-2 weeks for full CMake conversion

---

## Recommended Implementation Approach

### Phase 1: Minimum Viable WASM Port (2-4 weeks)

**Goal:** Get basic IMM playback working in browser

**Scope:**
1. ✅ Use OpenGL ES renderer only
2. ✅ Stub out audio (NULL backend)
3. ✅ Remove threading (synchronous loading)
4. ✅ Use MEMFS for file I/O
5. ✅ Create `libBasics/wasm/` platform implementations
6. ✅ Add CMake build for Emscripten
7. ✅ Basic HTML5 Canvas integration

**Deliverables:**
- libImmPlayer.wasm + .js
- Simple HTML viewer demo
- Loading screen for file loads

**Tradeoffs:**
- ❌ No audio
- ❌ Blocks during load (acceptable for small files)
- ❌ No progressive streaming

### Phase 2: Feature Completion (4-8 weeks)

**Audio Integration:**
- Implement Web Audio API backend
- Spatial audio if needed (TBE360 SDK replacement)

**Improved Loading:**
- Frame-chunked loading to prevent freezes
- Progress callbacks
- HTTP streaming for large files

**Performance:**
- WebGL optimizations
- Memory management (Emscripten heap tuning)
- Shader variant optimization

**API Enhancements:**
- JavaScript-friendly API wrapper
- TypeScript definitions
- NPM package

### Phase 3: Advanced Features (Optional, 4-8 weeks)

**Threading (if needed):**
- Emscripten pthread emulation
- Web Workers for asset loading
- SharedArrayBuffer support

**WebXR Integration:**
- VR mode via WebXR API
- Replace Oculus SDK calls with WebXR

**Streaming:**
- Progressive IMM file loading
- HTTP range request support
- Adaptive quality based on bandwidth

---

## Technical Implementation Details

### Platform Abstraction: WASM Directory Structure

```
libBasics/wasm/
├── piThread.cpp      - Stub or single-threaded
├── piMutex.cpp       - No-op locks
├── piTimer.cpp       - performance.now()
├── piFileOS.cpp      - Emscripten FS API
├── piLog.cpp         - console.log()
└── piSystemInfo.cpp  - Browser detection
```

### Threading Removal Strategy

**Option A: Complete Removal (Recommended for Phase 1)**

```cpp
// document.cpp:230 - Before
std::thread loadingThread([...]() {
    iLoadCPU(...);
});
loadingThread.detach();

// After - Direct call
iLoadCPU(...);  // Blocks, but simple
```

**Option B: Frame-Chunked Loading**

```cpp
// Break loading into chunks, yield to browser
void ChunkedLoad() {
    static int chunk = 0;
    if (chunk < totalChunks) {
        LoadChunk(chunk++);
        emscripten_async_call(ChunkedLoad, nullptr, 16); // Next frame
    }
}
```

**Option C: Web Worker (Complex)**

```javascript
// main.js
const worker = new Worker('loader.js');
worker.postMessage({cmd: 'load', file: 'scene.imm'});
worker.onmessage = (e) => {
    if (e.data.progress) updateProgress(e.data.progress);
    if (e.data.done) onLoadComplete();
};
```

### Mutex Simplification

```cpp
// piMutex.cpp - WASM implementation
#ifdef __EMSCRIPTEN__
bool piMutex::Init() { return true; }
void piMutex::End() {}
void piMutex::Lock() {}    // No-op
void piMutex::UnLock() {}  // No-op
#endif
```

### File I/O Adaptation

```cpp
// piFileOS.cpp - WASM implementation
bool piFile::Open(const wchar_t* name, const wchar_t* mode) {
    // Convert wchar_t to UTF-8
    char utf8_name[1024];
    wcstombs(utf8_name, name, sizeof(utf8_name));

    // Use Emscripten FS
    mHandle = emscripten_fs_open(utf8_name, mode);
    return mHandle != nullptr;
}
```

---

## Build System Strategy

### CMakeLists.txt Structure

```cmake
# Root CMakeLists.txt
project(libImmPlayer)

# Platform detection
if(EMSCRIPTEN)
    set(PLATFORM_DIR "wasm")
    set(RENDERER_BACKEND "opengles")
else()
    # ... Windows/Android ...
endif()

# Platform-specific sources
file(GLOB PLATFORM_SOURCES
    "libImmCore/src/libBasics/${PLATFORM_DIR}/*.cpp")

# Renderer backend
file(GLOB RENDERER_SOURCES
    "libImmCore/src/libRender/${RENDERER_BACKEND}/*.cpp")

# Emscripten-specific flags
if(EMSCRIPTEN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
        -s USE_WEBGL2=1 \
        -s FULL_ES3=1 \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s MODULARIZE=1 \
        -s EXPORT_NAME='ImmPlayer'")
endif()
```

### Emscripten Build Commands

```bash
# Configure
emcmake cmake -B build-wasm \
    -DCMAKE_BUILD_TYPE=Release \
    -DPLATFORM=wasm

# Build
cmake --build build-wasm

# Output: libImmPlayer.wasm, libImmPlayer.js
```

---

## Effort Estimation

### Breakdown by Phase

| Phase | Component | Estimated Time | Complexity |
|-------|-----------|---------------|------------|
| **Phase 1** | Platform abstraction (wasm/) | 3-5 days | Medium |
| | CMake build system | 5-7 days | Medium-High |
| | OpenGL ES → WebGL adaptation | 2-3 days | Low |
| | Threading removal | 1-2 days | Low |
| | File I/O (MEMFS) | 2-3 days | Low |
| | Wide char conversion | 2-3 days | Medium |
| | HTML5 integration & testing | 3-5 days | Low-Medium |
| | **Phase 1 Total** | **2-4 weeks** | |
| **Phase 2** | Web Audio API backend | 5-7 days | Medium |
| | Progressive loading | 3-5 days | Medium |
| | Performance optimization | 5-10 days | Medium-High |
| | API wrapper & documentation | 3-5 days | Low |
| | **Phase 2 Total** | **4-8 weeks** | |
| **Phase 3** | Emscripten pthreads | 7-10 days | High |
| | WebXR integration | 10-15 days | High |
| | Advanced streaming | 5-7 days | Medium |
| | **Phase 3 Total** | **4-8 weeks** | |

### Developer Requirements

**Ideal skill set:**
- Strong C++ experience
- Emscripten/WASM expertise
- WebGL knowledge
- Understanding of threading models
- Build system experience (CMake)

**Can be learned on the job:**
- IMM format specifics
- libImmPlayer architecture

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Threading issues | Low | Medium | Already analyzed as optional |
| Performance problems | Medium | High | Start with optimizations early |
| Memory constraints | Medium | Medium | Emscripten heap tuning, streaming |
| Browser compatibility | Low | Medium | Target modern browsers, polyfills |
| Build system complexity | High | Medium | Incremental CMake migration |
| Shader compatibility | Low | Low | GLSL ES already exists |
| Third-party dep issues | Low | Medium | Well-supported libraries |

---

## Success Criteria

### Phase 1 MVP
- ✅ Loads and renders IMM files in browser
- ✅ Playback controls work (play, pause, seek)
- ✅ Runs at 30+ fps for typical scenes
- ✅ No crashes or memory leaks
- ✅ Works in Chrome, Firefox, Safari

### Phase 2 Complete
- ✅ Audio playback functional
- ✅ Smooth loading (< 100ms freeze)
- ✅ 60 fps performance
- ✅ JavaScript API documented
- ✅ NPM package available

### Phase 3 Advanced
- ✅ WebXR VR mode
- ✅ Large file streaming (>100MB)
- ✅ Multi-threaded loading
- ✅ Production-ready

---

## Conclusion

Porting libImmPlayer to WebAssembly is **feasible** and **recommended** given:

1. ✅ OpenGL ES backend already exists
2. ✅ Platform abstraction in place
3. ✅ Threading is optional (major simplification)
4. ✅ Dependencies have WASM ports
5. ✅ Clean architecture supports adaptation

**Primary challenges** are build system conversion and platform layer implementation, not fundamental architectural barriers.

**Recommended path:** Start with Phase 1 MVP (2-4 weeks) to prove viability, then assess whether Phase 2/3 features are needed based on use case requirements.

The codebase is well-structured for this port - the existence of Android support proves cross-platform abstraction works, and the threading TODO comment confirms the developers anticipated alternative execution models.

---

## References

### Key Source Files Analyzed

- `code/libImmPlayer/src/player.h` - Main player API
- `code/libImmPlayer/src/player.cpp` - Threading and rendering
- `code/libImmPlayer/src/document.cpp` - Asset loading
- `code/libImmPlayer/libImmPlayer.vcxproj` - Build configuration
- `code/libImmCore/src/libRender/opengles/` - WebGL-compatible renderer
- `code/libImmCore/src/libBasics/windows/` - Platform abstraction reference
- `README.md` - Project overview and dependencies

### External Resources

- Emscripten Documentation: https://emscripten.org/
- WebGL Specification: https://www.khronos.org/webgl/
- WebXR Device API: https://www.w3.org/TR/webxr/
- Web Audio API: https://www.w3.org/TR/webaudio/

---

**Report prepared by:** Claude (Anthropic)
**Codebase analyzed:** IMM (Immersive Media Format)
**Repository:** C:\Users\andyb\Documents\IMM
