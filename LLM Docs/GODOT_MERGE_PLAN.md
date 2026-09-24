# Merge Plan: `feature/godot-support` → `main`

## Summary

One file conflicts. Everything else merges cleanly. The conflict is in
`code/appImmUnity/src/main.cpp`, where the godot branch refactored the
Unity plugin body to delegate through a new `ImmEngineBridge` shared
layer, while main simultaneously added a full Metal backend to that same
file. The two changes are architecturally compatible — Metal is just
another engine-specific adapter — but they must be reconciled by hand.

**Strategy:** use the godot branch's `ImmEngineBridge` architecture as
the base and graft the Metal additions back in as a Unity-adapter concern,
keeping Metal code in `appImmUnity/src/main.cpp` (where it belongs) rather
than pulling it into the engine-agnostic bridge.

---

## Pre-merge state

| Item | Godot branch | Main |
|---|---|---|
| `code/appImmUnity/src/main.cpp` | ~1200 lines, delegates to `ImmEngineBridge` | ~2000 lines, owns all state, adds Metal |
| `code/appImmShared/` | New — `ImmEngineBridge` | Does not exist |
| `code/appImmGodot/` | New | Does not exist |
| `code/appImmGodotGDExtension/` | New | Does not exist |
| `code/ImmGodotSampleProject/` | New | Does not exist |
| All other files | Untouched on godot branch | Various changes (Metal, iOS, macOS, CI) |

---

## Step 0 — Create the merge branch

```bash
# Working from the IMM repo (currently on main)
git checkout main
git checkout -b feature/godot-support-rebased

# Attempt the merge so Git populates conflict markers
git merge feature/godot-support
# Expected: conflict in code/appImmUnity/src/main.cpp only
# All other files auto-resolve as additions or were untouched by one side
```

---

## Step 1 — Files that require no manual work

Git will auto-resolve all of these. Verify after the merge attempt:

| File | Resolution |
|---|---|
| `code/appImmShared/src/imm_engine_bridge.h` | Added by godot — keep |
| `code/appImmShared/src/imm_engine_bridge.cpp` | Added by godot — keep |
| `code/appImmGodot/**` | Added by godot — keep |
| `code/appImmGodotGDExtension/**` | Added by godot — keep |
| `code/ImmGodotSampleProject/**` | Added by godot — keep |
| `code/projects/windows/imm.sln` | Added by godot — keep |
| `docs/unity-viewer-to-godot-port-plan.md` | Added by godot — keep |
| `code/appImmUnity/appImmUnity.vcxproj` | Godot adds bridge compile entries, main unchanged — keep godot version |
| All other files changed by main | Main-only changes (Metal, iOS, CI, etc.) — keep main version |

---

## Step 2 — Resolve `code/appImmUnity/src/main.cpp`

Open the conflict-marked file and build the final version section by section.
The godot branch version lives in `../IMM-godot/code/appImmUnity/src/main.cpp`
(worktree at `~/Documents/GitHub/IMM-godot`) for reference.

### 2a. Includes block (lines 78–115 of final)

Start from the **godot version's** include list, then insert the Metal
additions from main. Order matters for the compiler:

```cpp
#define VERBOSE 0

#include "appImmShared/src/imm_engine_bridge.h"   // KEEP: godot

#if defined(__APPLE__)                              // ADD: from main
#import <Metal/Metal.h>
#endif

#include "libImmCore/src/libBasics/piStr.h"
#include "libImmPlayer/src/player.h"
#include "libImmImporter/src/document/layerSpawnArea.h"
#if defined(WINDOWS)
// ... Windows-only includes (identical in both)
#endif
#include "IUnityGraphics.h"
#if defined(WINDOWS)
#include "IUnityGraphicsD3D11.h"
#include "IUnityGraphicsD3D12.h"
#endif
#if defined(__APPLE__)                              // ADD: from main
#include "IUnityGraphicsMetal.h"
#include "libImmCore/src/libRender/metal/piMetal_Renderer.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <mutex>
#endif
using namespace ImmCore;
using namespace ImmImporter;
using namespace ImmPlayer;

#if defined(__ANDROID__) || defined(ANDROID)       // identical in both
// ...
#endif
```

### 2b. Windows DLL helpers and `MainRenderReporter`

The Windows `iFileExists`, `iDirName`, `iPreloadSharedRuntimeDependencies`,
and `DllMain` block is **identical in both versions** — keep either.

`MainRenderReporter` was **removed from `main.cpp` by the godot branch**
(moved into `ImmEngineBridge`). Do NOT re-add it. Main still has it locally
but the bridge version is authoritative.

### 2c. `ImmUnityPlugin` struct

Use the **godot version** as the base and add Metal interface pointers from main:

```cpp
struct ImmUnityPlugin
{
    struct
    {
        IUnityInterfaces *mUnityInterfaces = nullptr;
        IUnityGraphics   *mGraphics = nullptr;
        void             *mDevice = nullptr;
#if defined(__APPLE__)                              // ADD: from main
        IUnityGraphicsMetalV2 *mMetalV2 = nullptr;
        IUnityGraphicsMetalV1 *mMetal = nullptr;
        UnityGfxRenderer mRenderer = kUnityGfxRendererNull;
#endif
    } UnityAPI;

    ImmShared::ImmEngineBridge mBridge;             // KEEP: godot

#if defined(__APPLE__)                              // ADD: new — see §2h
    struct {
        int mViewportWidth = 0;
        int mViewportHeight = 0;
    } mMetalCameraViewport[256];
#endif
};
```

The `ImmUnityPlugin.IMM` sub-struct (log, timer, sound, renderer, player)
from main is **entirely replaced** by `mBridge`. Do not re-add it.

### 2d. Global instance and helpers

Keep the **godot version** exactly:

```cpp
static ImmUnityPlugin gImmUnityPlugin;

static Player &iPlayer()
{
    return *gImmUnityPlugin.mBridge.GetPlayer();
}

static piLog &iLog()
{
    return *gImmUnityPlugin.mBridge.GetLog();
}
```

Then add the Metal mutex and globals from main:

```cpp
#if defined(__APPLE__)                              // ADD: from main
static std::recursive_mutex sImmUnityNativeMutex;
#define IMM_UNITY_NATIVE_LOCK() \
    std::lock_guard<std::recursive_mutex> immUnityNativeLock(sImmUnityNativeMutex)
#else
#define IMM_UNITY_NATIVE_LOCK()
#endif
```

Then Metal render-state globals (from main, `__APPLE__`-guarded):

```cpp
static int sRenderEventCount = 0;
static int sUnityMetalRenderReportCount = 0;
static int sUnityMetalRenderBoundaryReportCount = 0;

#if defined(__APPLE__)                              // ADD: from main
static piTexture sUnityMetalOffscreenColor = nullptr;
static piTexture sUnityMetalOffscreenDepth = nullptr;
static piRTarget sUnityMetalOffscreenTarget = nullptr;
static int sUnityMetalOffscreenWidth = 0;
static int sUnityMetalOffscreenHeight = 0;

static bool iEnvFlagEnabled(const char *name) { ... }           // verbatim from main
static bool iEnsureUnityMetalOffscreenTarget(...) { ... }       // verbatim from main
#endif
```

**Note:** `IMM_UNITY_NATIVE_LOCK()` is NOT added back to all the `extern "C"`
API functions (the godot branch correctly removed it from those, as the bridge
handles its own thread safety). It is only used in `iOnGraphicsDeviceEvent`
and `iOnRenderEvent` where Metal-specific Unity API objects are accessed.

### 2e. Android deferred-init block

Both versions have this block; they are functionally equivalent.
Keep the **godot version** (which calls `mBridge.CompleteGraphicsInitialization()`
instead of the old inline renderer init). The godot version is cleaner.

### 2f. `iOnGraphicsDeviceEvent`

Use the **godot version** as the base. Make two changes:

1. Add `IMM_UNITY_NATIVE_LOCK()` at the top (Metal thread safety).
2. Replace the stub Apple/Metal branch with the full interface capture from main:

```cpp
static void UNITY_INTERFACE_API iOnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    IMM_UNITY_NATIVE_LOCK();                        // ADD: from main
    if (eventType == kUnityGfxDeviceEventInitialize)
    {
        UnityGfxRenderer apiType = gImmUnityPlugin.UnityAPI.mGraphics->GetRenderer();
#if defined(__APPLE__)
        gImmUnityPlugin.UnityAPI.mRenderer = apiType;   // ADD: from main (needed by render event)
#endif

        // Windows and Android branches: keep godot version (identical to main)

#else  // Apple
        if (apiType == kUnityGfxRendererOpenGLCore)
        {
            gImmUnityPlugin.UnityAPI.mDevice = nullptr;
        }
        else if (apiType == kUnityGfxRendererMetal)     // REPLACE stub with main's full capture:
        {
            gImmUnityPlugin.UnityAPI.mMetalV2 =
                gImmUnityPlugin.UnityAPI.mUnityInterfaces->Get<IUnityGraphicsMetalV2>();
            gImmUnityPlugin.UnityAPI.mMetal =
                gImmUnityPlugin.UnityAPI.mUnityInterfaces->Get<IUnityGraphicsMetalV1>();
            if (gImmUnityPlugin.UnityAPI.mMetalV2)
                gImmUnityPlugin.UnityAPI.mDevice = gImmUnityPlugin.UnityAPI.mMetalV2->MetalDevice();
            else if (gImmUnityPlugin.UnityAPI.mMetal)
                gImmUnityPlugin.UnityAPI.mDevice = gImmUnityPlugin.UnityAPI.mMetal->MetalDevice();
            else
                gImmUnityPlugin.UnityAPI.mDevice = nullptr;
        }
#endif
    }
    else if (eventType == kUnityGfxDeviceEventShutdown)
    {
    }
}
```

### 2g. `iOnRenderEvent`

This is the most complex section. The godot version collapses the entire
render body to a single `mBridge.RenderCamera()` call. For Metal we need
to wrap that call with the Metal frame-begin/end preamble from main.

**Architecture decision:** Metal-specific frame management stays in
`main.cpp` as a Unity-adapter concern. The bridge call is unchanged.
The Metal `SwapBuffers` happens after the bridge returns.

Structure of the merged function:

```
1. Android deferred-init block          (identical in both — keep either)
2. IMM_UNITY_NATIVE_LOCK()              (add back for Metal safety)
3. cameraID bounds check                (from main — was missing in godot)
4. Metal preamble                       (from main, __APPLE__ guard)
   - detect isUnityMetal
   - get/validate command buffer, encoder, render pass descriptor
   - call BeginExternal*Frame (unless deferUnityMetalFrameBegin)
   - set oldVp from Metal viewport tracking  ← see note below
5. Non-Metal viewport query             (from godot, else branch)
   - renderer->GetViewports(&numVp, oldVp)
6. Android viewport override            (identical in both)
7. res validity check                   (both have this, identical)
8. Android scissor disable              (identical)
9. Build ViewportInfo                   (from godot)
10. eyeID = event_id & 1               (from godot)
11. mBridge.RenderCamera(...)          (from godot)   ← THE KEY CALL
12. Metal performance logging           (from main, __APPLE__ guard)
13. Metal SwapBuffers + boundary log    (from main, __APPLE__ guard)
```

**Viewport for Metal (step 4/5 note):**
In main, `oldVp[2]` and `oldVp[3]` are set from
`gImmUnityPlugin.FromUnity.mCamera[cameraID].mViewportWidth/Height`,
which are populated by `SetCameraViewport()`. In the merged version these
values live in `gImmUnityPlugin.mMetalCameraViewport[cameraID]`
(see §2c). For Metal, skip `GetViewports()` and build the viewport from
the tracked width/height directly, matching what main does.

For the non-Metal path the godot code (`renderer->GetViewports(...)`) is correct.

The exact code for the Metal preamble (steps 4 and 12–13) can be lifted
verbatim from main's `iOnRenderEvent` — approximately lines 531–693
and 858–909 of main's version. No semantic changes needed; just ensure
`gImmUnityPlugin.IMM.mRenderer` references become
`gImmUnityPlugin.UnityAPI.mRenderer` and
`gImmUnityPlugin.IMM.mRenderer->…` becomes
`gImmUnityPlugin.mBridge.GetRenderer()->…`.

**Important:** The `RenderCamera` call in the merged version keeps
`tickSound = true`. The Metal path calls `SwapBuffers` after `RenderCamera`
returns. In main, sound tick happened before SwapBuffers; this order is
preserved because the bridge ticks sound internally as part of `RenderCamera`.

### 2h. `SetCameraViewport`

This function exists in main but was never in the godot branch. It is
required for the Metal render path (viewport dimensions are tracked here
and used in `iOnRenderEvent` to size the Metal render target and set
`oldVp[2/3]`).

Re-add it after `SetMatrices`, storing into `mMetalCameraViewport`:

```cpp
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
SetCameraViewport(int cameraID, int width, int height)
{
    if (cameraID < 0 || cameraID > 255) return;
#if defined(__APPLE__)
    IMM_UNITY_NATIVE_LOCK();
    gImmUnityPlugin.mMetalCameraViewport[cameraID].mViewportWidth = width;
    gImmUnityPlugin.mMetalCameraViewport[cameraID].mViewportHeight = height;
#endif
}
```

The Unity C# side (`ImmNativePlugin.cs`) already calls this function; it
must exist in the merged binary.

### 2i. `Init()` function

Use the **godot version** as the base. The godot branch's Apple section
only handles OpenGL and returns -1 for Metal:

```cpp
// Godot branch (incomplete):
if (gfx != kUnityGfxRendererOpenGLCore)
    return -1;
config.rendererApi = piRenderer::API::GL;
config.initializeRendererOnInit = true;
```

Replace with the full Metal + OpenGL handling from main:

```cpp
// Merged Apple section:
UnityGfxRenderer gfx = gImmUnityPlugin.UnityAPI.mGraphics->GetRenderer();
if (gfx == kUnityGfxRendererMetal)
{
    if (!gImmUnityPlugin.UnityAPI.mMetal || !gImmUnityPlugin.UnityAPI.mDevice)
    {
        // log error (use iLog() — see note)
        return -1;
    }
    config.rendererApi = piRenderer::API::Metal;
    config.graphicsDevice = gImmUnityPlugin.UnityAPI.mDevice;
}
else if (gfx == kUnityGfxRendererOpenGLCore)
{
    config.rendererApi = piRenderer::API::GL;
}
else
{
    // log unsupported renderer error
    return -1;
}
config.initializeRendererOnInit = true;
```

**Note on logging in `Init()`:** The godot version calls the bridge
after config is built, so `iLog()` isn't available until after
`mBridge.Init(config)` succeeds. For pre-init error logging, either
use `fprintf(stderr, ...)` or initialize a temporary log inline (matching
what main did with `gImmUnityPlugin.IMM.mLog.Init(...)`). The cleanest
approach for the merge: keep the `mBridge.Init(config)` call as-is and
add `fprintf(stderr, ...)` for Apple pre-init error messages.

The WINDOWS and ANDROID branches of `Init()` are **identical** between
both versions (godot already had the correct bridge-config form for those
platforms). Keep them unchanged.

### 2j. `UnityLayerInfo` struct and `GetLayerInfoByIndex`

The godot branch silently **broke `GetLayerInfoByIndex`** — it simplified
it to return `Player::LayerInfo` directly, but the Unity C# side expects
the `UnityLayerInfo` struct with UTF-16 name fields and specific memory
layout.

Keep the **main version** of:
- The `UnityLayerInfo` struct definition
- The `iCopyWideToUnityUtf16` helper
- The `GetLayerInfoByIndex` body that fills `UnityLayerInfo`

The godot version's simplification must not be used.

### 2k. Everything else in the C ABI

All remaining `extern "C"` API functions (`Pause`, `Resume`, `Load*`,
`SetDocumentToWorld`, spawn-area functions, exporter API, etc.) are
semantically identical between both versions. The only difference is the
godot branch uses `iPlayer()` / `iLog()` shorthand instead of
`gImmUnityPlugin.IMM.mPlayer` / `gImmUnityPlugin.IMM.mLog`.

Use the **godot version** for all of these — shorter, cleaner, correct.
The `IMM_UNITY_NATIVE_LOCK()` calls that main had in each function were
correctly removed in the godot branch (the bridge handles thread safety
internally).

---

## Step 3 — Verify `ImmEngineBridge` handles Metal init correctly

After resolving `main.cpp`, check that the bridge's `Init()` path
correctly passes the Metal device through to `piRenderer::Initialize()`.

In `code/appImmShared/src/imm_engine_bridge.cpp`, the `InitializeRenderer()`
method calls:
```cpp
mRenderer->Initialize(0, nullptr, 0, true, false, mRenderReporter, false, mConfig.graphicsDevice);
```

For Metal, `mConfig.graphicsDevice` will be the `id<MTLDevice>` pointer
captured in step 2i. Confirm `piMetal_Renderer` accepts this device
pointer in its `Initialize()` implementation. If it does not (it was
being passed directly in main but via a different code path), the bridge's
`InitializeRenderer()` may need a Metal-specific branch.

Check: `code/libImmCore/src/libRender/metal/piMetal_Renderer.mm` —
look at how `Initialize()` consumes the `device` parameter. This is a
**read-only check** and should not require bridge changes if the Metal
renderer already accepts the device via that parameter.

---

## Step 4 — Build verification

After the merge, confirm the following build targets still compile cleanly:

| Target | Expected outcome |
|---|---|
| Windows (DX/GL) Unity plugin | No change in behavior |
| macOS (Metal) Unity plugin | Metal path restored via new adapter code |
| macOS (OpenGL) Unity plugin | GL path unchanged |
| Android Unity plugin | Deferred-init path unchanged |
| iOS Unity plugin | If applicable — verify `Init()` Apple branch covers it |
| Windows Godot skeleton | New target — smoke-init only, no actual render |

---

## Step 5 — Test parity check

Run the macOS Unity sample scene against both the pre-merge binary (from
`main`) and the post-merge binary. The Metal render path must produce
identical output. Specifically:

- IMM document loads and renders
- Performance log messages appear (`IMM_UNITY_METAL_RENDER ...`)
- `sUnityMetalRenderBoundaryReportCount` increments as before
- Env-flag overrides (`IMM_UNITY_METAL_NOOP_EVENT`, etc.) behave identically

---

## What this merge does NOT do

- Does **not** add Metal support to the Godot bridge — that is a future task
  (Phase 2+ of the Godot port plan)
- Does **not** complete the Godot integration (godot branch was mid-Phase 1)
- Does **not** rebase the Godot sample project or GDExtension scaffold

---

## Files changed in the merged commit

```
M  code/appImmUnity/src/main.cpp         # conflict resolved
A  code/appImmShared/src/imm_engine_bridge.h
A  code/appImmShared/src/imm_engine_bridge.cpp
A  code/appImmUnity/appImmUnity.vcxproj  # adds bridge compile entries
A  code/appImmGodot/appImmGodot.vcxproj
A  code/appImmGodot/src/imm_godot_plugin.h
A  code/appImmGodot/src/main.cpp
A  code/appImmGodotGDExtension/README.md
A  code/appImmGodotGDExtension/src/imm_viewer_node.cpp
A  code/appImmGodotGDExtension/src/imm_viewer_node.h
A  code/appImmGodotGDExtension/src/register_types.cpp
A  code/ImmGodotSampleProject/**
A  code/projects/windows/imm.sln         # adds Godot projects
A  docs/unity-viewer-to-godot-port-plan.md
```

Plus all files changed by `main` since the merge base (Metal, iOS, macOS,
CI, Unity binaries, etc.) arrive untouched via the merge.

---

## Estimated effort

| Task | Effort |
|---|---|
| Steps 0–1 (merge + auto-resolve) | 5 min |
| Step 2a–2d (includes, structs, globals) | 30 min |
| Step 2e–2g (device event, render event) | 2–3 hr — render event is the hard part |
| Step 2h–2k (remaining API functions) | 30 min |
| Steps 3–5 (bridge check, build, test) | 1–2 hr |
| **Total** | **~5 hr** |

The render event (step 2g) dominates because the Metal preamble is ~150 lines
and must be correctly integrated around the bridge call with proper variable
scoping and early-return semantics matching what main had.
