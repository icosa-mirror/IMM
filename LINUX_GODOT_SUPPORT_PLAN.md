# Linux Godot Support Plan

## Objective

Add supported Linux x86_64 rendering for the IMM Godot addon using Godot 4.5,
Forward+ and Vulkan. A normal user must be able to open the sample project in
the Linux Godot editor and press Run, and an exported Linux project must behave
the same way.

The first supported scope is non-VR rendering with the null audio backend.
Linux audio, ARM64 and XR are follow-up work and must not delay the initial
renderer integration.

## Current state

Linux is not presently a build or packaging target. This is more than a missing
manifest entry:

- `code/appImmGodotGDExtension/SConstruct` accepts only Windows and macOS.
- The Godot addon manifest has Windows, macOS and Android libraries, but no
  Linux shared libraries.
- The IMM Godot Vulkan bridge is compiled only on Windows and Android.
- The Vulkan renderer dynamically loads Windows `vulkan-1.dll` or Android
  `libvulkan.so`, but does not load Linux `libvulkan.so.1`.
- IMM core has Windows, Android, macOS and iOS platform implementations, but no
  Linux implementations for files, timing, threading, logging and system
  information.
- CI has no Linux Godot build, exported-player or visual-validation lane.

The rendering architecture itself should transfer. Linux Godot can use the
same external-device Vulkan path and the same two-pass RenderingDevice depth
composition now validated on Windows Vulkan and macOS Metal. Godot owns the
window, surface and presentation, so the IMM plugin should not require its own
X11 or Wayland surface or swapchain implementation.

## Supported first milestone

- Linux x86_64 using a glibc-based CI and distribution build.
- Godot 4.5.
- Forward+ with Vulkan confirmed at runtime.
- Editor Run-button behavior.
- Exported Linux player behavior.
- Render-only, full-depth and ordered-overlay validation.
- Null audio backend.
- Self-contained Godot addon packaging with correctly located `.so` files.

## Explicit non-goals for the first milestone

- Linux ARM64.
- Linux audio playback.
- OpenGL compatibility rendering.
- Linux XR or OpenXR.
- A standalone Linux IMM viewer.
- A Unity Linux plugin.
- Broad validation across multiple Linux distributions.

These may be added after the x86_64 Vulkan Godot lane is visually accepted.

## Implementation plan

### Phase 1: Linux platform foundation

1. Add a Linux native build under `code/projects/linux/` for:
   - `libImmCore`;
   - `libImmImporter`;
   - `libImmPlayer`;
   - `ImmGodotPlugin`.
2. Build only the components needed by the external-device Vulkan Godot path.
   Do not add a native Linux IMM window or swapchain merely to support Godot.
3. Add Linux platform implementations for the IMM core services used by the
   bridge:
   - file and directory access;
   - monotonic timing and sleeping;
   - mutexes and threads;
   - logging;
   - minimal system information.
4. Use the existing null sound backend. The plugin should load and render even
   when no Linux audio backend is present.
5. Prefer small Linux/POSIX implementations over copying Apple-specific APIs.
   In particular:
   - use a monotonic POSIX or C++ clock rather than Mach time;
   - use pthreads or standard C++ threading without Mach thread IDs;
   - use portable file copying rather than Apple's `copyfile`;
   - obtain minimal system information without CoreGraphics or `sysctlbyname`.
6. Link third-party importer dependencies reproducibly: zlib, PNG, JPEG, Ogg,
   Vorbis and Opus. Prefer static linkage inside the native IMM library where
   licensing and the existing build structure permit it, reducing exported
   project dependency problems.

Exit criterion: `libImmGodotPlugin.so` builds on a clean Linux x86_64 runner and
exports the complete existing `ImmGodot_*` C ABI.

### Phase 2: Linux external Vulkan support

1. Enable the Vulkan renderer and Godot Vulkan frame bridge for `__linux__`.
2. Dynamically load `libvulkan.so.1` and resolve `vkGetInstanceProcAddr`.
3. Keep the renderer in external-device mode:
   - use Godot's Vulkan instance, physical device, logical device and graphics
     queue;
   - render into Godot-provided intermediate color/depth images;
   - record and synchronize through the existing Godot integration;
   - do not create a Linux surface or swapchain.
4. Audit platform guards so Linux selects Vulkan in automatic renderer mode.
5. Ensure exported C symbols have default visibility on Linux.
6. Preserve the validated depth conventions:
   - IMM intermediate depth is normal-Z;
   - Godot scene depth is reverse-Z;
   - the full-depth compositor writes a complete merged color target before
     copying it back to Godot color;
   - valid far-plane IMM 360 content is not treated as an untouched pixel.

Exit criterion: the native bridge initializes against Godot's Linux Vulkan
device and successfully records a non-empty IMM frame without owning the
display surface.

### Phase 3: Linux GDExtension and addon packaging

1. Extend the GDExtension SCons build with `platform=linux` and `arch=x86_64`.
2. Build `godot-cpp` for the pinned Godot 4.5 Linux ABI.
3. Exclude the Objective-C++ Metal source from the Linux build and include the
   shared Vulkan frame source.
4. Link the Linux GDExtension against `libImmGodotPlugin.so`, `dl` and required
   threading libraries.
5. Set an `$ORIGIN`-relative runtime search path so exported projects locate
   the native IMM library beside the GDExtension.
6. Add these addon manifest entries:
   - `linux.debug.x86_64`;
   - `linux.release.x86_64`;
   - matching `libImmGodotPlugin.so` dependencies.
7. Add or update the Godot Linux export preset so both shared libraries are
   included automatically.
8. Verify library dependency resolution with `ldd` in CI and fail on missing
   non-system dependencies.

Exit criterion: both the editor and an exported Linux project load the addon
without manually setting `LD_LIBRARY_PATH`.

### Phase 4: Local and cloud functional validation

Add a Linux Godot Vulkan lane to the cloud validation workflow. The hosted
runner should install a known Vulkan implementation and report the actual
adapter and API used. Mesa Lavapipe is acceptable for correctness validation if
Godot really runs Forward+ Vulkan; a compatibility renderer, dummy renderer or
OpenGL fallback is not.

The lane must perform these checks:

1. **Build and load preflight**
   - build all Linux native libraries from source;
   - verify expected exported C ABI symbols;
   - verify addon manifest paths and ELF dependencies;
   - require Godot to report Forward+ and Vulkan.
2. **Run-button equivalent**
   - launch `project.godot` without a scene or script override;
   - exercise the configured main scene exactly as pressing Run does;
   - require a clean native log and a visually correct capture.
3. **Render-only visual smoke**
   - capture the expected IMM scene;
   - use the existing renderer-tolerant baseline comparison;
   - reject blank, default-Godot, sky-only, missing-stroke, displaced-camera and
     reverse-Z negative cases.
4. **Full-depth composition**
   - require the IMM scene and 360 background;
   - require the visible magenta and yellow probes;
   - require cyan to be occluded inside the character region;
   - require the compositor's intermediate color, intermediate depth, merged
     color and final copy stages;
   - make the strict image comparison authoritative.
5. **Ordered-overlay composition**
   - require the intended before/after ordering;
   - require the cyan rear probe to remain occluded;
   - reject the known foreground-cyan failure fixture.
6. **Exported player smoke**
   - export the Linux sample project;
   - launch it in the same software/hardware Vulkan environment;
   - repeat at least the render-only strict visual check.

Log and text checks may fail early, but no lane may pass without an accepted
visual capture.

### Phase 5: Report integration and regression protection

1. Add Linux as a row in the report's leading visual matrix.
2. Treat Vulkan as the implicit renderer for the Linux Godot cell.
3. Leave Linux standalone and Unity cells gray until those products are
   separately implemented and tested.
4. Apply the existing matrix semantics:
   - green: required Linux Godot depth composition visually passes;
   - yellow: rendering passes but required depth evidence is absent;
   - red: rendering or attempted depth composition fails;
   - gray: genuinely not tested or out of scope.
5. Add workflow-contract and report-generation tests covering the Linux row.
6. Include Linux captures, strict metric JSON, status JSON and logs in the
   combined validation evidence artifact.
7. Manually inspect the first successful cloud artifact before declaring Linux
   supported.

## Acceptance criteria

Linux Godot support is complete only when all of the following are true:

- A clean Linux x86_64 checkout builds without private machine dependencies.
- Opening the sample project in Godot 4.5 and pressing Run renders IMM content.
- An exported Linux project loads the addon without environment-variable
  workarounds.
- Runtime diagnostics confirm Forward+ Vulkan with no API fallback.
- Render-only, full-depth and ordered-overlay images all pass their strict
  visual contracts.
- The full-depth image preserves both Godot host color and IMM far-plane 360
  content and has correct cyan occlusion.
- The Linux Godot matrix cell is green based on visual depth-composition
  evidence.
- Existing Windows, Android and macOS visual lanes do not regress.

## Risks and mitigations

### Native platform portability

The repository has no Linux core-platform implementation today. Keep the first
port narrow and use the null audio backend. Avoid implementing a Linux window,
surface or presentation layer because Godot already owns those concerns.

### Shared-library packaging

Linux editor success can hide exported-player dependency failures. Test the
exported player separately, use an `$ORIGIN` rpath and inspect every ELF
dependency in CI.

### Software Vulkan differences

Hosted runners may use Lavapipe rather than a physical GPU. Require the actual
driver/API in evidence and retain tolerant visual comparisons rather than
binary image equality. A later physical-GPU Linux lane can supplement this,
but is not necessary for the first correctness milestone if the software
Vulkan capture is visually valid.

### Accidental renderer fallback

Godot can start with a different renderer when Vulkan setup fails. The lane
must fail before image classification unless the runtime reports Forward+ and
Vulkan.

### Scope expansion

Audio, ARM64, XR, standalone and Unity support are separate milestones. Adding
them during the initial Godot port would obscure whether the core Linux Vulkan
integration is correct.

## Estimated effort

For one engineer familiar with this repository:

- build and POSIX platform foundation: 1–2 focused days;
- Linux Vulkan bridge and GDExtension integration: 1–2 focused days;
- editor/export packaging: about 1 focused day;
- cloud visual validation, report integration and fixes from real results:
  1–3 focused days.

Expected total: **4–8 focused engineering days** for production-quality Linux
x86_64 Godot Vulkan support. A first render-only prototype may be possible in
**1–3 days**, but it is not support until depth composition, exported-player
packaging and cloud visual validation also pass.

Linux audio, ARM64 or broad multi-distribution support could extend the work
toward two weeks or more.
