# ANGLE Integration Plan for macOS Standalone Player

## Overview
Use Google's ANGLE to run your existing GLES 3.2 renderer (currently used on Android) on macOS via Metal backend. This eliminates deprecated OpenGL usage while keeping your rendering code unchanged.

---

## Phase 1: ANGLE Build Integration

### 1.1 Add ANGLE as Third-Party Dependency

**Option A: Pre-built ANGLE (Recommended for faster setup)**
- Download ANGLE binaries from Chromium releases or build separately
- Place in `thirdparty/angle/`
  - `include/` - GLES 3.2 headers (gl32.h, egl.h)
  - `lib/macos/` - libEGL.dylib, libGLESv2.dylib

**Option B: Build ANGLE from Source**
```bash
# Add to your build scripts
git clone https://chromium.googlesource.com/angle/angle thirdparty/angle-src
# Build with Metal backend enabled
gn gen out/mac --args='is_debug=false angle_enable_metal=true'
ninja -C out/mac libEGL libGLESv2
```

### 1.2 Update CMakeLists.txt

```cmake
# In code/projects/macos/CMakeLists.txt

# Remove OpenGL framework
# BEFORE: target_link_libraries(libImmCore ... "-framework OpenGL")
# AFTER: Replace with ANGLE libs

# Add ANGLE library
add_library(angle_egl SHARED IMPORTED)
set_target_properties(angle_egl PROPERTIES
    IMPORTED_LOCATION ${THIRDPARTY_DIR}/angle/lib/macos/libEGL.dylib
)
add_library(angle_glesv2 SHARED IMPORTED)
set_target_properties(angle_glesv2 PROPERTIES
    IMPORTED_LOCATION ${THIRDPARTY_DIR}/angle/lib/macos/libGLESv2.dylib
)

# Update libImmCore linking
target_link_libraries(libImmCore
    PUBLIC
        angle_egl
        angle_glesv2
        # ... other libs
        "-framework Metal"      # ANGLE needs Metal framework
        "-framework MetalKit"   # For CAMetalLayer
        "-framework CoreGraphics"
        "-framework Cocoa"
        # Remove: "-framework OpenGL"
)

# Update include directories
target_include_directories(libImmCore
    PUBLIC
        ${THIRDPARTY_DIR}/angle/include
        # ... other includes
)
```

### 1.3 Update Deployment

```cmake
# Copy ANGLE dylibs to app bundle
add_custom_command(TARGET appImmViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${THIRDPARTY_DIR}/angle/lib/macos/libEGL.dylib
        ${CMAKE_BINARY_DIR}/viewer/libEGL.dylib
    COMMAND ${CMAKE_COMMAND} -E copy
        ${THIRDPARTY_DIR}/angle/lib/macos/libGLESv2.dylib
        ${CMAKE_BINARY_DIR}/viewer/libGLESv2.dylib
)
```

---

## Phase 2: Window & Context Refactoring

### 2.1 Replace NSOpenGLView with Regular NSView

**File:** `code/libImmCore/src/libBasics/macos/piWindow.mm`

The current window creates an NSOpenGLView. Replace with a plain NSView backed by a CAMetalLayer (required by ANGLE).

```objc
// Add to imports
#import <MetalKit/MetalKit.h>

// BEFORE: @interface ImmWindowView : NSOpenGLView
// AFTER:
@interface ImmWindowView : NSView
{
    iMacWindow *mOwner;
    CAMetalLayer *metalLayer;
}
// ... rest unchanged
@end

@implementation ImmWindowView

- (id)initWithFrame:(NSRect)frame owner:(iMacWindow*)owner
{
    self = [super initWithFrame:frame];
    if (self) 
    {
        mOwner = owner;
        
        // Create Metal layer for ANGLE
        metalLayer = [CAMetalLayer layer];
        metalLayer.device = MTLCreateSystemDefaultDevice();
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.framebufferOnly = YES;
        metalLayer.frame = frame;
        [self setLayer:metalLayer];
        [self setWantsLayer:YES];
    }
    return self;
}

// ... rest of event handling unchanged

@end
```

### 2.2 Remove Pixel Format & OpenGL Context Creation

**In piWindow_init():**

```objc
// BEFORE:
NSOpenGLPixelFormatAttribute attrs[] = {
    NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
    NSOpenGLPFAAccelerated,
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAColorSize, 24,
    NSOpenGLPFADepthSize, 24,
    NSOpenGLPFAStencilSize, 8,
    0
};
NSOpenGLPixelFormat *format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
ImmWindowView *view = [[ImmWindowView alloc] initWithFrame:rect pixelFormat:format owner:me];
// ...
me->context = [view openGLContext];
me->cglContext = [me->context CGLContextObj];

// AFTER:
ImmWindowView *view = [[ImmWindowView alloc] initWithFrame:rect owner:me];
// ... no format/context creation
me->view = view;
// me->context = nil; // No NSOpenGLContext
// me->cglContext = nil; // No CGL context
```

### 2.3 Create ANGLE EGL Context for macOS

**New File:** `code/libImmCore/src/libRender/opengles/macos/piGLES_RenderContextOS.cpp`

```cpp
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <MetalKit/MetalKit.h>
#include "../piGLES_Renderer.h"

namespace ImmCore {

struct piGLES_RenderContextOS
{
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int mNumWindows;
    int mActualWindow;
    CAMetalLayer *metalLayers[8];
};

bool piRendererGLES::Initialize(int id, const void **hwnd, int num, bool disableVSync, 
                                  bool disableErrors, piReporter *reporter, 
                                  bool createDevice, void *device)
{
    // hwnd contains CAMetalLayer pointers from piWindow
    piGLES_RenderContextOS *me = new piGLES_RenderContextOS();
    if (!me) return false;
    
    // Initialize EGL with Metal backend
    EGLAttrib platformAttribs[] = {
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE,
        EGL_NONE
    };
    
    me->display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, 
                                           EGL_DEFAULT_DISPLAY, 
                                           platformAttribs);
    if (me->display == EGL_NO_DISPLAY) {
        // Try default display as fallback
        me->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }
    
    EGLint major, minor;
    if (!eglInitialize(me->display, &major, &minor)) {
        return false;
    }
    
    // Select GLES 3.2 config
    EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    
    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(me->display, configAttribs, &config, 1, &numConfigs)) {
        return false;
    }
    
    // Create context for GLES 3.2
    EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION_KHR, 2,
        EGL_NONE
    };
    
    me->context = eglCreateContext(me->display, config, EGL_NO_CONTEXT, contextAttribs);
    if (me->context == EGL_NO_CONTEXT) {
        return false;
    }
    
    // Create window surfaces from Metal layers
    me->mNumWindows = num;
    for (int i = 0; i < num; i++) {
        me->metalLayers[i] = (CAMetalLayer*)hwnd[i];
        
        // Create EGL surface from Metal layer
        EGLAttrib surfaceAttribs[] = {
            EGL_WIDTH, (EGLAttrib)me->metalLayers[i].drawableSize.width,
            EGL_HEIGHT, (EGLAttrib)me->metalLayers[i].drawableSize.height,
            EGL_NONE
        };
        
        me->surface = eglCreatePlatformWindowSurfaceEXT(me->display, config, 
                                                        me->metalLayers[i], 
                                                        surfaceAttribs);
        if (me->surface == EGL_NO_SURFACE) {
            return false;
        }
    }
    
    eglMakeCurrent(me->display, me->surface, me->surface, me->context);
    
    // Disable vsync if requested
    if (disableVSync) {
        eglSwapInterval(me->display, 0);
    }
    
    mContext = me;
    return true;
}

void piRendererGLES::Deinitialize()
{
    piGLES_RenderContextOS *me = (piGLES_RenderContextOS*)mContext;
    if (!me) return;
    
    eglMakeCurrent(me->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(me->display, me->context);
    eglDestroySurface(me->display, me->surface);
    eglTerminate(me->display);
    
    delete me;
    mContext = nullptr;
}

void piRendererGLES::SetActiveWindow(int id)
{
    piGLES_RenderContextOS *me = (piGLES_RenderContextOS*)mContext;
    if (!me || id < 0 || id >= me->mNumWindows) return;
    me->mActualWindow = id;
    // Would need per-window surfaces in multi-window setup
}

void piRendererGLES::Enable()
{
    piGLES_RenderContextOS *me = (piGLES_RenderContextOS*)mContext;
    if (!me) return;
    eglMakeCurrent(me->display, me->surface, me->surface, me->context);
}

void piRendererGLES::Disable()
{
    // No-op or save/restore previous context
}

void piRendererGLES::SwapBuffers()
{
    piGLES_RenderContextOS *me = (piGLES_RenderContextOS*)mContext;
    if (!me) return;
    eglSwapBuffers(me->display, me->surface);
}

} // namespace ImmCore
```

---

## Phase 3: Renderer Selection

### 3.1 Modify CMakeLists.txt to Use GLES Renderer on macOS

```cmake
# BEFORE: macOS uses opengl4x
set(IMMCORE_SOURCES
    # ...
    ${CODE_DIR}/libImmCore/src/libRender/opengl4x/piGL4X_Renderer.cpp
    ${CODE_DIR}/libImmCore/src/libRender/opengl4x/piGL4X_Ext.cpp
    ${CODE_DIR}/libImmCore/src/libRender/opengl4x/macos/piGL4X_RenderContextOS.cpp
)

# AFTER: macOS uses opengles with ANGLE
set(IMMCORE_SOURCES
    # ...
    ${CODE_DIR}/libImmCore/src/libRender/opengles/piGLES_Renderer.cpp
    ${CODE_DIR}/libImmCore/src/libRender/opengles/piGLES_Ext.cpp  # if exists
    ${CODE_DIR}/libImmCore/src/libRender/opengles/macos/piGLES_RenderContextOS.cpp
)
```

### 3.2 Update Viewer to Use GLES Renderer

**File:** `code/appImmViewer/src/mymain.cpp` or wherever renderer is instantiated

```cpp
// BEFORE: Uses piRendererGL4X for macOS
piRenderer *renderer = new piRendererGL4X();

// AFTER: Uses piRendererGLES with ANGLE on all platforms
piRenderer *renderer = new piRendererGLES();
```

---

## Phase 4: Shader Compatibility

### 4.1 Verify Shader Compatibility

GLES 3.2 shaders should work as-is. Key differences from GL 4.1:
- GLES uses `in`/`out` instead of `attribute`/`varying`
- GLES requires explicit precision qualifiers
- Texture sampling functions differ slightly

Your Android GLES shaders in `code/libImmPlayer/src/layerRenderers/` should work unchanged.

### 4.2 Handle Shader Version Strings

If shaders have platform-specific version directives:

```glsl
// Add to shader preprocessor if needed
#ifdef __APPLE__
    #define GLES_VERSION "#version 320 es\n"
#else
    #define GLES_VERSION "#version 320 es\n"
#endif
```

---

## Phase 5: Build & Test

### 5.1 Build Commands

```bash
cd code/projects/macos
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

### 5.2 Runtime Testing Checklist

1. **Window Creation**: Verify Metal layer is created without errors
2. **EGL Initialization**: Check EGL context creation succeeds
3. **Basic Rendering**: Test clear color, simple draws
4. **Shader Compilation**: Verify existing GLES shaders compile
5. **Texture Loading**: Test 2D textures and rendering
6. **Full Playback**: Load and play .imm files
7. **Performance**: Compare FPS vs old OpenGL build
8. **Memory**: Verify no leaks during playback

### 5.3 Debugging ANGLE Issues

Enable ANGLE debug logging:

```bash
export ANGLE_DEBUG=1
export ANGLE_LOG_LEVEL=debug
./appImmViewer
```

Common issues:
- **Black screen**: Metal layer not properly sized or pixel format mismatch
- **EGL init fails**: ANGLE libs not in rpath or wrong architecture (x64 vs arm64)
- **Shader compile errors**: Version string or precision qualifiers

---

## Phase 6: App Bundle Distribution

### 6.1 Create Proper macOS App Bundle

```cmake
set_target_properties(appImmViewer PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_LIST_DIR}/Info.plist.in
)

# Embed ANGLE libs in bundle
set_target_properties(appImmViewer PROPERTIES
    XCODE_EMBED_LIBRARIES "libEGL.dylib;libGLESv2.dylib"
    XCODE_EMBED_LIBRARIES_CODE_SIGN_ON_COPY TRUE
)
```

### 6.2 Code Signing & Notarization

ANGLE libraries must be signed:
```bash
codesign --sign "Developer ID" --force --deep --entitlements entitlements.plist appImmViewer.app
```

---

## Summary of Changes

| Component | Before | After |
|-----------|--------|-------|
| Graphics API | OpenGL 4.1 Core | GLES 3.2 via ANGLE |
| Renderer | piGL4X_Renderer | piGLES_Renderer |
| Context | CGL/NSOpenGLContext | EGL via ANGLE |
| Window | NSOpenGLView | NSView + CAMetalLayer |
| Frameworks | OpenGL | Metal, MetalKit, ANGLE libs |
| Binary Size | ~2MB smaller | +~10-15MB for ANGLE |
| Shaders | GL 4.1 | GLES 3.2 (already exist for Android) |

---

## Timeline Estimate

- **Phase 1** (ANGLE setup): 1-2 days
- **Phase 2** (Window refactoring): 2-3 days  
- **Phase 3** (Renderer integration): 1 day
- **Phase 4-5** (Testing & debugging): 3-5 days
- **Phase 6** (Distribution): 1-2 days

**Total: 1-2 weeks** for full macOS ANGLE migration with testing.

---

## References

- ANGLE Project: https://chromium.googlesource.com/angle/angle
- ANGLE Metal Backend: https://bugs.chromium.org/p/angleproject/issues/list?q=component:Metal
- EGL on macOS with Metal: https://angleproject.org

---

## Notes for Other Agents

This plan assumes:
1. Existing GLES 3.2 renderer works on Android
2. Shaders are already GLES 3.2 compatible
3. CMake build system for macOS
4. Target: macOS 10.15+ (Catalina and later)

Key files to modify:
- `code/projects/macos/CMakeLists.txt` - Build configuration
- `code/libImmCore/src/libBasics/macos/piWindow.mm` - Window creation
- `code/libImmCore/src/libRender/opengles/macos/piGLES_RenderContextOS.cpp` - **NEW FILE** for EGL context
- `code/appImmViewer/src/mymain.cpp` - Renderer instantiation

Created: 2026-02-01
