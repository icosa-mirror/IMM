# Linux Product and Validation Plan

## Status

This plan supersedes the original Godot-only Linux plan. Linux support will be
implemented as a sequence of independently verifiable milestones:

1. Godot Linux Vulkan rendering and packaging delivered with explicit null
   audio as an intermediate development milestone.
2. A production Linux audio backend integrated into Godot, followed by complete
   Godot visual, audio and packaging validation.
3. Unity Linux with Vulkan and the production audio backend.
4. A standalone Linux Vulkan viewer with the production audio backend.

Godot remains first because it owns the window, Vulkan surface and presentation
path. It is therefore the smallest product integration and the quickest way to
prove the shared Linux runtime. Rendering is deliberately proven before audio
so failures in the two systems cannot obscure one another. Unity follows after
Godot and the shared audio backend are complete. Standalone comes last because
it must additionally own the Linux window, input, Vulkan surface, swapchain and
application lifecycle.

## Objective

Provide supported Linux x86_64 IMM playback on a glibc-based distribution. Each
supported engine integration must work both from its normal editor Run button
and from an exported player. The standalone viewer must provide equivalent
file-loading and playback behaviour without an engine dependency.

The first renderer is Vulkan. The initial architecture must not prevent later
Linux ARM64, OpenXR or additional graphics backends, but those targets must not
expand the first implementation milestones.

## Current state

Linux is not currently a build, packaging or validation target:

1. `code/appImmGodotGDExtension/SConstruct` accepts only Windows and macOS.
2. The Godot addon manifest contains Windows, macOS, Android and iOS binaries,
   but no Linux shared libraries.
3. The IMM Godot Vulkan bridge is compiled only on Windows and Android.
4. The Vulkan renderer loads `vulkan-1.dll` on Windows or `libvulkan.so` on
   Android, but has no Linux `libvulkan.so.1` path.
5. IMM core has Windows, Android, macOS and iOS platform implementations, but no
   Linux implementations for files, time, threads, logging or system details.
6. IMM has a null sound backend, but no actual Linux audio-output backend.
7. There is no packaged Linux Unity native plugin or Linux player validation.
8. There is no standalone Linux application shell, window or swapchain.
9. The validation workflow and main visual matrix contain no Linux row.

The renderer and compositor designs are already exercised across Windows
Vulkan, Android Vulkan, macOS Metal and iOS Simulator Metal. Linux Godot and
Unity should reuse their existing engine-owned presentation paths. Only the
standalone viewer should create a Linux window, surface and swapchain.

## Supported platform baseline

1. Linux x86_64 on a glibc-based distribution.
2. Vulkan 1.x through the system Vulkan loader.
3. Godot addon compatibility minimum 4.5, tested with an explicitly pinned
   stock Godot release selected for the Linux CI lane.
4. Unity tested with the sample project's pinned Editor version, currently
   `6000.3.18f1`.
5. Explicit null audio for the initial Godot Vulkan rendering milestone.
6. Actual Linux audio output through a supported native backend for completed
   Godot, Unity and Standalone product support.
7. Headless CI audio through a virtual output device using the same production
   backend after the audio milestone begins; null audio must never masquerade as
   a production-audio pass.
8. Self-contained engine packages and exported players that do not require
   users to set `LD_LIBRARY_PATH`.
9. Cloud validation with retained visual, log and status evidence, plus retained
   audio evidence for every audio-enabled milestone.

## Scope deferred until after the initial three products

1. Linux ARM64.
2. Linux OpenXR or other XR runtimes.
3. OpenGL compatibility rendering.
4. Broad multi-distribution certification.
5. Distribution-store packaging.

## Validation principles

1. Compilation proves only source and linker compatibility.
2. An editor import or exported application build proves packaging, not runtime
   rendering or audio.
3. No supported rendering lane passes without an application-generated capture
   accepted by an authoritative tolerant visual contract.
4. Visual contracts must detect blank output, default-engine scenes, sky-only
   output, missing IMM content, displaced cameras, reverse-Z and incorrect
   occlusion without requiring binary pixel equality.
5. Godot and Unity require render-only, full-depth, ordered-overlay and normal
   Run-button evidence.
6. Logs are fast-fail diagnostics and cannot replace visual evidence.
7. Audio support requires the production Linux backend to initialize, consume
   decoded IMM audio, submit non-silent PCM and produce a retained capture or
   equivalent measurable output through a virtual CI sink.
8. Null audio is an intentional and visible condition in the initial Godot
   Vulkan delivery milestone, but it cannot produce a green supported matrix
   cell.
9. Phase 2 development evidence is provisional and diagnostic. Godot remains
   gray in the main matrix until Phase 3 validates the complete product,
   including production audio.
10. The first successful artifact for every product must be manually reviewed
   before its matrix cell is promoted.

## Phase 1: Shared Linux platform foundation

1. Add Linux native builds under `code/projects/linux/` for the shared IMM core,
   importer and player libraries.
2. Add the Linux platform services required by every product:
   file and directory operations, monotonic timing and sleeping, mutexes and
   threads, logging, locale, and minimal system information.
3. Prefer portable C++ or POSIX implementations over copied Apple or Windows
   platform code.
4. Export the existing public C ABI with default ELF symbol visibility.
5. Link importer dependencies reproducibly: zlib, PNG, JPEG, Ogg, Vorbis and
   Opus. Prefer static linkage where licensing and the build structure permit;
   otherwise package and audit every non-system shared dependency.
6. Add unit and ABI tests that run on a clean hosted Linux runner.
7. Wire the existing null sound backend as an explicit early-milestone choice so
   the first Godot rendering work has no undeclared audio dependency.

### Exit criterion

The shared Linux libraries build from a clean checkout and pass ABI/platform
tests. A test explicitly requesting null audio initializes cleanly; no claim of
production audio support is made.

## Phase 2: Godot Linux Vulkan delivery without production audio

1. Enable the Vulkan renderer and Godot Vulkan frame bridge for `__linux__`.
2. Load `libvulkan.so.1`, with any fallback justified by the target distribution
   policy, and resolve `vkGetInstanceProcAddr` through the system loader.
3. Keep the renderer in external-device mode using Godot's Vulkan instance,
   physical device, logical device, graphics queue and intermediate color/depth
   images. Do not create a second surface or swapchain.
4. Preserve the validated depth conventions: IMM intermediate depth is
   normal-Z, Godot scene depth is reverse-Z, and the compositor must retain valid
   far-plane IMM 360 content.
5. Extend the GDExtension SCons build with `platform=linux` and `arch=x86_64`.
6. Build against the addon-compatible `godot-cpp` ABI and a pinned stock Godot
   executable. A custom Godot engine must be used only if a demonstrated
   upstream limitation makes it unavoidable.
7. Exclude Objective-C++ Metal sources and include the shared Vulkan frame
   source.
8. Package `libimm_godot_extension.so`, `libImmGodotPlugin.so` and all required
   dependencies under the addon.
9. Add `linux.debug.x86_64` and `linux.release.x86_64` library/dependency entries
   to `imm_viewer.gdextension`.
10. Use `$ORIGIN`-relative runtime search paths and verify the complete ELF
    dependency graph with `readelf` and `ldd`.
11. Add a Linux export preset and verify that an exported project contains and
    locates every required library without environment-variable workarounds.
12. Configure the normal Linux Godot sample path to request null audio explicitly
    and report that selection in retained evidence.
13. Add Linux to the leading matrix as a gray row with Standalone, Godot and
    Unity cells. Vulkan is implicit for Linux.
14. Add developer smoke tests for native initialization, a non-empty IMM frame,
    editor Run-button startup and exported-player startup.
15. Retain diagnostic images and logs during implementation, but do not treat
    them as authoritative validation evidence or promote the matrix cell.

### Exit criterion

Opening the ordinary sample project in the Linux Godot editor and pressing Run
loads IMM content and renders through Forward+ Vulkan with explicitly reported
null audio. The exported Linux project behaves the same way. This is a delivery
milestone suitable for beginning audio integration, not a completed validation
phase. The Linux Godot matrix cell remains gray and Godot Linux is not yet
reported as supported.

## Phase 3: Production Linux audio and complete Godot validation

1. Select a production Linux output API after a short dependency and deployment
   review. The chosen path must work on current PipeWire-based desktops without
   requiring application-specific PipeWire configuration; PulseAudio
   compatibility or ALSA are acceptable candidates.
2. Implement the existing `piSoundEngineBackend` contract, including device
   enumeration, initialization, PCM submission, pause/resume, shutdown, sample
   rate handling and underrun diagnostics.
3. Preserve the existing IMM decode and spatial/mixing behaviour rather than
   substituting Godot-specific playback.
4. Keep the null backend as an explicit fallback for tests and servers, never as
   a silent replacement for a requested production backend.
5. Connect the production backend to the ordinary Godot Editor Run and exported
   player paths.
6. Run Godot under a documented hosted display environment. Install the Vulkan
   loader, Mesa Vulkan driver and diagnostic tools, and configure X11/Xvfb or a
   different confirmed Godot-compatible display path.
7. Record the Vulkan adapter, driver and API. Mesa Lavapipe is acceptable for
   correctness if Godot actually uses Forward+ Vulkan. Compatibility, OpenGL,
   a dummy renderer or an undeclared fallback fails early.
8. Reuse the established four application phases: render-only, full-depth,
   ordered-overlay and ordinary Run-button, now with production audio selected.
9. Reuse per-phase result sentinels, immediate evidence staging, standardized
   status JSON, artifact manifests and crash/device-loss checks.
10. Apply platform-specific authoritative tolerant visual contracts with
    localized scene-content, orientation, reverse-Z and occlusion probes.
11. Configure CI with a virtual audio sink using the same production backend and
   retain a captured waveform plus measurements for duration, channel count,
   sample rate and non-silence.
12. Export and launch the Linux player in the same Vulkan/audio environment and
    repeat the render-only, Run-button-equivalent and audio checks.
13. Add workflow-contract and report-generation tests for the Linux row.
14. Manually inspect the first complete cloud visual artifact.
15. Add at least one manual real-device listening check before declaring Godot
   Linux fully supported.
16. Promote the Linux Godot cell from gray to green only when the complete
    visual, depth, packaging and production-audio evidence passes. Unity and
    Standalone remain gray.

### Exit criterion

The Linux Godot matrix cell becomes green from retained Vulkan
depth-composition evidence only after its Run-button and exported-player paths
also pass with the production backend, the virtual-sink artifact contains
validated non-silent audio, and a real Linux device check confirms audible
playback. At this point Godot Linux is fully supported.

## Phase 4: Unity Linux Vulkan integration and validation

1. Add a Linux x86_64 build for the Unity native plugin and package the resulting
   `.so` with correct Unity platform metadata.
2. Keep the implementation on the supported Unity Vulkan integration model:
   access Unity-owned resources through the resource-access APIs, record
   same-frame work into Unity's command buffer, and do not submit unsynchronized
   work through `AccessQueue`.
3. Reuse the explicit intermediate render-target and Unity-owned presentation
   design already required for portable Android Vulkan behaviour.
4. Build the ordinary Unity sample project for Linux with Vulkan as the required
   graphics API and fail if Unity falls back to OpenGL.
5. Confirm normal Editor Play behaviour and the exported Linux player.
6. Reuse render-only, full-depth and ordered-overlay contracts, application
   result sentinels, logs and retained captures.
7. Connect and validate the shared Linux audio backend through the Unity product
   path.
8. Add the Unity Linux evidence to the aggregate report and promote the Unity
   cell only after manual review of the first complete artifact.

### Exit criterion

The ordinary Unity sample works in Editor Play and as an exported Linux Vulkan
player, passes retained render and depth-composition contracts, and passes the
production Linux audio gate.

## Phase 5: Standalone Linux Vulkan viewer

1. Define the minimum supported distribution, desktop session, window systems
   and packaging format.
2. Add a Linux application shell that owns the window, input, Vulkan surface,
   swapchain, resize handling and lifecycle. Prefer a maintained windowing layer
   with both X11 and Wayland support unless a narrower dependency is justified.
3. Reuse the shared Vulkan IMM renderer instead of introducing a second Linux
   rendering implementation.
4. Implement file opening, sample loading, playback controls and error handling
   consistent with the existing standalone viewers.
5. Use the shared production Linux audio backend.
6. Package a runnable artifact with deterministic resource and shared-library
   discovery and no private machine dependencies.
7. Add render-only visual validation, startup/crash checks and virtual-sink
   audio validation. Depth composition does not apply to standalone, so a
   successful standalone render is green in the leading matrix.
8. Exercise both X11 and Wayland where hosted infrastructure permits; require at
   least one supported display path for the first release.
9. Promote the Standalone Linux matrix cell only after the first artifact is
   manually reviewed.

### Exit criterion

The packaged standalone viewer starts on the supported Linux environment,
loads and renders the complete IMM scene through Vulkan, plays audio, handles
ordinary resize/close lifecycle events and passes its retained CI contracts.

## Phase 6: Broader Linux coverage

1. Add Linux ARM64 builds where dependencies and runners are available.
2. Add OpenXR to the engine integrations and standalone viewer as separate
   stereo-rendering milestones.
3. Evaluate additional distribution and driver coverage, including at least one
   physical AMD, Intel or NVIDIA Vulkan runner.
4. Add OpenGL only if a real compatibility requirement justifies its maintenance
   and validation cost.
5. Add release packaging and distribution-store integration after the supported
   runtime matrix is stable.

## Product acceptance criteria

### Godot

1. A clean Linux x86_64 checkout builds and packages the addon.
2. Editor Run and exported-player paths render through Forward+ Vulkan.
3. Render-only, full-depth and ordered-overlay contracts pass.
4. The production Linux audio backend passes virtual-sink and real-device tests.
5. The Linux Godot matrix cell is green.

### Unity

1. The native plugin imports into the pinned Unity Editor on Linux.
2. Editor Play and exported-player paths use Vulkan and render correctly.
3. Render-only, full-depth and ordered-overlay contracts pass.
4. The production Linux audio backend passes through the Unity path.
5. The Linux Unity matrix cell is green.

### Standalone

1. The packaged viewer runs without engine dependencies or environment-variable
   workarounds.
2. It renders the complete scene through Vulkan and plays audio.
3. Its application-generated visual contract and lifecycle checks pass.
4. The Linux Standalone matrix cell is green.

### Regression protection

1. Existing Windows, Android, macOS and iOS supported visual lanes remain green.
2. Existing audio gates remain green.
3. Missing Linux runtime, image, depth or audio evidence fails closed and is
   classified accurately rather than reported as a misleading render failure.

## Principal risks

### Linux ABI and distribution portability

Build on the oldest supported glibc baseline and audit symbol/library versions.
Avoid accidental dependencies on the hosted runner image.

### Audio-server fragmentation

Choose a backend that works on current PipeWire desktops through a stable
compatibility API. Test both a CI virtual sink and a real desktop audio device,
and surface fallback explicitly.

### Shared-library packaging

Editor success can conceal exported-player failures. Test every exported product
separately, use `$ORIGIN` where appropriate and audit its ELF dependency closure.

### Software Vulkan differences

Lavapipe is suitable for deterministic correctness evidence but not performance
qualification. Retain tolerant visual contracts and later supplement them with
physical-GPU evidence.

### Accidental renderer fallback

Require runtime evidence for the selected Vulkan adapter and API before visual
classification. Never infer Vulkan from a successful process exit.

### Standalone scope

Do not make Godot or Unity wait for a standalone window-system implementation.
Build and validate the shared runtime first, then add presentation ownership as
a separate product phase.

## Estimated effort

The estimates overlap because later products reuse the platform, renderer and
audio work:

1. Shared Linux platform and null-audio foundation: 2–4 focused days.
2. Godot Vulkan delivery and packaging without production audio: 2–4 focused
   days.
3. Production Linux audio, Godot integration and complete cloud validation:
   3–6 focused days.
4. Unity Vulkan/audio integration, packaging and validation: 2–5 focused days.
5. Standalone Linux windowing, audio, packaging and validation: 3–7 focused
   days.
6. Regression fixes and physical-device review: 2–5 focused days distributed
   across the milestones.

Expected total for all three production-quality Linux products is approximately
14–31 focused engineering days. Godot Vulkan rendering can reach an intermediate
delivery milestone before production audio exists, but it is not a validated or
supported matrix target until Phase 3 passes. The matrix must continue to show
Unity and Standalone as gray until their own complete evidence exists.
