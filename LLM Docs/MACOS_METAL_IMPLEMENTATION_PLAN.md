# macOS Metal Backend Implementation Plan

## Goal

Add first-class macOS Metal rendering support for IMM, using the native standalone macOS player as the required intermediate step before integrating the same Metal renderer into the `ImmUnity` Unity plugin.

This plan covers two hosts:

- Native standalone macOS player: `appImmViewer`
- Unity rendering plugin: `ImmUnity`

The expensive shared work is the Metal implementation of IMM's `piRenderer` abstraction. The native player is the first integration target because it lets IMM own the Metal device, command queue, drawable, render pass, and event loop. Unity integration should begin only after the standalone player proves that the shared renderer, shaders, resource binding, coordinate conventions, and representative IMM playback work outside Unity.

In other words, the sequence is:

```text
shared piRendererMetal skeleton
  -> native standalone macOS player
  -> native standalone IMM playback validation
  -> Unity Metal host adapter
  -> Unity Editor and Unity standalone macOS playback
```

## Original Baseline

At the start of this work, IMM rendering was built around `piRenderer`:

- `code/libImmCore/src/libRender/piRenderer.h`
- `code/libImmCore/src/libRender/piRenderer.cpp`
- `code/libImmCore/src/libRender/opengl4x/*`
- `code/libImmCore/src/libRender/opengles/*`
- `code/libImmCore/src/libRender/directx11/*`

The supported renderer APIs at that point were:

- `piRenderer::API::GL`
- `piRenderer::API::DX`
- `piRenderer::API::GLES`

There was no `piRenderer::API::Metal` and no `libRender/metal` implementation.

macOS-specific pieces already exist:

- Native viewer entrypoint: `code/appImmViewer/src/macos/main.mm`
- Viewer CMake target: `appImmViewer` in `code/projects/macos/CMakeLists.txt`
- Unity plugin target: `ImmUnity` in `code/projects/macos/CMakeLists.txt`
- Unity macOS plugin bundle destination: `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/OSX/ImmUnityPlugin.bundle`

The Unity package currently blocks Metal in managed code:

- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs`

The native Unity plugin also rejects non-OpenGLCore renderers on macOS:

- `code/appImmUnity/src/main.cpp`

The CPU/data-only `ImmStrokeReader` plugin already builds on macOS because it uses the minimal non-rendering core. It is not the main risk for this work.

## Target Architecture

Add a shared Metal renderer backend:

```text
libImmPlayer
  -> piRenderer interface
    -> piRendererGL4X      existing legacy macOS path
    -> piRendererMetal     new primary macOS path
```

Use two host adapters, but build them in order rather than in parallel:

```text
Native appImmViewer host
  -> owns NSWindow / MTKView or CAMetalLayer
  -> owns command queue, drawable, render pass
  -> passes native Metal frame context to piRendererMetal

Unity ImmUnity host
  -> Unity owns device, command scheduling, render target
  -> plugin receives render-thread callbacks
  -> piRendererMetal renders inside Unity-compatible frame context
```

The desired end state:

- `appImmViewer` runs on macOS using Metal.
- Unity Editor on macOS runs `ImmUnity` using Metal.
- macOS standalone Unity builds run `ImmUnity` using Metal.
- Windows D3D11 and Android GLES behavior remains unchanged.
- OpenGLCore remains only as a legacy fallback if it does not complicate the Metal path.

The standalone player is not a detour. It is the shortest route to the Unity goal because it removes Unity's command-buffer ownership, plugin-event timing, package import settings, and managed/native lifecycle from the first Metal renderer bring-up.

## Standalone Player as the Intermediate Step

The native macOS standalone player is the planned intermediate product, not a parallel side quest. Every shared Metal renderer capability should land there first, then move into Unity once it has been proven outside Unity's host constraints.

Treat the standalone player as the midpoint deliverable on the path to Unity:

```text
shared Metal renderer
  -> native macOS standalone player
     - owns Metal device, command queue, drawable, render pass, resize, and app lifecycle
     - proves renderer correctness, content coverage, captures, and failure handling
  -> Unity macOS plugin
     - reuses the proven renderer behavior
     - replaces only host ownership with Unity's Metal device, render-thread events, command-buffer rules, and package/runtime integration
```

Use this ordering for implementation decisions:

1. Implement the shared `piRendererMetal` capability.
2. Exercise it in `appImmViewerMetal` with native macOS ownership of the Metal device, drawable, render pass, and resize lifecycle.
3. Add or expand standalone validation and captures for the affected IMM content type.
4. Only then wire the same renderer capability into `ImmUnity` through Unity's Metal host adapter.

This gives two concrete benefits for the Unity goal:

- Bugs in shaders, resource binding, vertex layouts, culling, depth, transparency, and texture formats can be debugged in a small native host before Unity is involved.
- Unity-specific failures can be isolated to device acquisition, render-thread callbacks, command-buffer/render-pass ownership, managed package settings, or Unity camera/projection conversion.

The standalone player should therefore be treated as the required gate for Unity Metal work. A Unity task is blocked if the equivalent non-Unity renderer behavior is failing in `appImmViewerMetal`, unless the task is inherently Unity-specific.

The practical rule is: if a feature can be expressed through `piRendererMetal` and normal IMM player state, it belongs in the standalone player first. If a feature depends on `IUnityGraphicsMetal`, Unity render events, Unity textures, Unity camera conversion, Unity package import settings, or Editor/player lifecycle, it belongs in the Unity phase after the standalone gate is green.

Audio follows the same rule. Embedded IMM audio must decode and play in the native standalone player before Unity integration is treated as unblocked. The Unity plugin can later adapt host-specific audio policy if needed, but the shared IMM loading/playback path must not rely on silent fallback, missing codec support, rejected play calls, or unproven playback progress.

### Reuse for a Native macOS Standalone Product

Using the standalone player as the intermediate step also preserves almost all work needed for a native macOS standalone player. The following pieces are directly reusable:

- `piRendererMetal`, including buffers, textures, shaders, pipeline state, render targets, depth/stencil, blending, draw submission, and renderer diagnostics.
- IMM player configuration for Metal conventions, including projection, winding, culling, depth, viewport, resize, and render-target recreation behavior.
- Native macOS host ownership of `MTLDevice`, command queue, drawable/render pass, `MTKView` sizing, AppKit lifecycle, and validation exit paths.
- Build targets, CTest entries, capture generation, and playback validation for representative IMM files.
- Runtime error reporting for unsupported Metal paths and shader/pipeline failures.

The standalone product work that is not automatically solved by the Unity goal is split between required playback behavior and host polish:

- App bundle packaging, signing, notarization, icons, menus, file associations, drag/drop, recent files, and user-facing preferences.
- Required audio behavior: embedded IMM audio must decode and play through a native macOS backend, with validation that catches silent fallback, missing codec support, rejected play requests, missing playback progress, startup failure, and clean teardown regressions. Audio is part of standalone readiness, not polish.
- Audio polish beyond the standalone readiness gate: output-device selection, recoverable user-facing error dialogs, and long-running playback controls. Basic document-level volume/mute is now part of the native standalone app surface.
- Input/navigation UX beyond the validation harness.
- Distribution, update, crash-reporting, and release automation.

Unity-specific work remains separate:

- Acquiring Unity's Metal interfaces through `IUnityGraphicsMetal`.
- Rendering inside Unity's render-thread callback and command-buffer/render-pass ownership rules.
- Mapping Unity camera/projection state into IMM player state.
- Managed package guards, Unity import settings, Editor behavior, and macOS standalone Unity build validation.

So the standalone-player phases are not throwaway work. They are the shared renderer and native-host foundation, plus enough macOS app infrastructure to be promoted into a standalone product if desired.

## Current Implementation Status

The repository now has a standalone Metal track that should be treated as the active intermediate milestone before Unity work continues:

- `piRenderer::API::Metal` exists.
- `code/libImmCore/src/libRender/metal/piMetal_Renderer.mm` contains the shared Metal renderer backend.
- `code/appImmViewer/src/macos/metal_player.mm` contains the native macOS standalone Metal host.
- `appImmViewerMetal` is defined in `code/projects/macos/CMakeLists.txt` as a temporary/native bring-up target beside the legacy `appImmViewer`, and now builds as `build/macos/viewer/appImmViewerMetal.app`.
- `validateAppImmViewerMetal` is defined in `code/projects/macos/CMakeLists.txt` and runs the standalone Metal playback, audio decode, longer audio playback, normal-run interactive audio/volume/playback-control/Open Recent/failed-open-restore smoke, command-line contract, capture, native-frame failure, app-bundle, content-path launch, repeated launch/teardown, and in-process reload harnesses.
- `code/appImmViewer/scripts/validate_metal_standalone.sh` runs both static and pretessellated playback validation against `exampleImmFiles/sample1.imm`.

CI no longer runs the temporary standalone validation and baseline-capture jobs. The GitHub workflow now treats the macOS standalone player as a build/package artifact, while renderer validation remains a local development tool. This keeps CI focused on whether the deliverable binaries build and avoids blocking the Unity plugin track on temporary sample-image policy.

Unity Metal integration has now started:

- `code/appImmUnity/src/IUnityGraphicsMetal.h` provides the Unity Metal native interface declarations needed by the plugin without importing Objective-C framework headers into the IMM importer-heavy translation unit.
- `code/appImmUnity/src/main.cpp` can select `piRenderer::API::Metal` on Apple platforms, acquire Unity's Metal device through `IUnityGraphicsMetalV1`, receive Unity render-thread callbacks, end Unity's current encoder, and begin a Metal frame by creating an IMM render encoder from Unity's current command buffer and render-pass descriptor.
- `code/libImmCore/src/libRender/metal/piMetal_Renderer.mm` has external Unity frame paths for both Unity-owned command encoders and Unity-owned command buffers/render-pass descriptors. The Unity render-pass path ends IMM's encoder but does not commit or present Unity's command buffer.
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmPlayerManager.cs` now requires Metal on Apple platforms and passes camera viewport dimensions to the native plugin.
- `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Runtime/ImmNativePlugin.cs` uses `__Internal` for iOS player builds and `ImmUnityPlugin` elsewhere.
- `code/ImmUnitySampleProject/Assets/Editor/BuildAutomation.cs` now has macOS and iOS Unity smoke-build methods that build the non-VR `SampleScene.unity` scene and force Metal for Apple targets.
- `code/ImmUnitySampleProject/ProjectSettings/ProjectSettings.asset` now pins macOS and iOS graphics APIs to Metal instead of relying on Unity's automatic API selection.

The current macOS Unity Metal state is now visually and audibly proven for the first standalone Unity player sample gate. Local evidence on 2026-05-25:

```sh
cmake --build build/macos --target ImmUnity --config Release
/Applications/Unity/Hub/Editor/2022.3.62f2/Unity.app/Contents/MacOS/Unity \
  -batchmode -quit \
  -projectPath code/ImmUnitySampleProject \
  -executeMethod ImmPlayer.Editor.BuildAutomation.BuildMacOSDevelopment \
  -logFile build/unity-macos-player-build.log
```

Both commands passed. The Unity build produced `code/ImmUnitySampleProject/Builds/macOS/IMMUnityTest.app`, and that app contains `Contents/PlugIns/ImmUnityPlugin.bundle` plus `Contents/Resources/Data/StreamingAssets/sample1.imm`.

A short local launch of that macOS Unity player also proved runtime startup:

```sh
code/ImmUnitySampleProject/Builds/macOS/IMMUnityTest.app/Contents/MacOS/IMM\ Unity\ Test \
  -logFile build/unity-macos-player-runtime.log
```

During current launches Unity selects Metal on Apple M1, the managed package initializes the native plugin, `sample1.imm` loads with document ID 0, and Unity enters the native render event on the `UnityGfxDeviceWorker`. With the render event reduced to a no-op, or to begin/end without IMM draw submission, the player remains alive. A raw Metal triangle submitted directly to Unity's current encoder from the native plugin also remains alive, exits cleanly, and writes a 640x360 PNG capture at `build/unity-smoke/macos-unity-triangle-only.png`. That proves Unity render events, Unity's Metal encoder, and the smoke capture path can handle plugin-submitted Metal draw work in this app.

The previous crash initially looked like a post-render Unity Metal submission failure, but isolation showed it was tied to the sample scene's managed post-load layer-refresh path. The specific bug was the Unity-facing layer-info ABI: native `Player::LayerInfo` contains `wchar_t name[128]` and `wchar_t fullName[256]`, but macOS `wchar_t` is 4 bytes while C# `CharSet.Unicode` marshals 2-byte UTF-16. Returning the internal struct directly to C# could overwrite the managed output buffer during `GetLayerInfoByIndex`.

`code/appImmUnity/src/main.cpp` now exports `GetLayerInfoByIndex` through a Unity ABI struct with fixed UTF-16 `char16_t` name fields and copies from the internal `Player::LayerInfo`. After rebuilding `ImmUnity` and the macOS Unity player, the layer-refresh-only probe and the normal all-features startup both exit cleanly without `IMM_UNITY_DISABLE_FEATURE_POST_LOAD`.

The current normal macOS Unity smoke reaches real native GPU load, real IMM render submission, and visible `sample1.imm` framebuffer output. After changing the Unity Metal projection-matrix configuration to `FromZeroToOne`, the native report reaches the full standalone-style `sample1.imm` shape: `drawCalls=38`, `paintDrawCalls=37`, `pictureDrawCalls=1`, `picture360DrawCalls=1`, and `triangles=645802` at a 640x360 viewport. This means normal sample-scene startup is no longer blocked by post-load metadata queries or by broad player-side culling.

Visual correctness inside the macOS Unity standalone framebuffer is now proven for the first sample gate. The latest default-path smoke capture is `build/unity-smoke/macos-current-proof.png`; it visibly contains the expected `sample1.imm` branch, small character, and 360 backdrop, and the smoke log reports full draw counts plus `nonZero=230400` for a 640x360 capture. A simple native Metal triangle drawn after `Player::RenderMono()` in the same Unity render event had previously proven Unity could see plugin drawing after IMM render calls; the normal IMM path now proves the actual generated shaders and content are visible too.

macOS Unity audio is now wired through the same AVFoundation backend proven by the native standalone player. `code/appImmUnity/src/main.cpp` selects `piSoundEngineBackend::API::AVFoundation` on Apple platforms, including the iOS static plugin target. The latest Unity macOS smoke run, `build/unity-macos-current-proof.log` plus `code/ImmUnitySampleProject/Builds/macOS/imm_player_log.txt`, proves that `sample1.imm` decodes three embedded stereo Ogg Opus sounds, accepts three AVFoundation `Play()` calls, reaches `state=playing` for all three, advances playback past the 0.25-second progress marker, cleans up three temp files, and still captures visible rendered output at `build/unity-smoke/macos-current-proof.png`.

One Unity-host-adapter bug has been found and fixed during this stage. The current-Unity-encoder path originally set `piRendererMetal`'s active render-pass descriptor to `nil`; generated IMM draw methods require that descriptor, so the player could count draw submissions while the Metal backend skipped the actual encoder draw work. `BeginExternalCommandEncoderFrame` now receives Unity's current render-pass descriptor and stores it as the active render pass. With that fix, a forced generated-shader clip-space probe is visible in `build/unity-smoke/macos-currentencoder-clipprobe.png`, proving generated IMM shader pipelines can encode visible draws into Unity's framebuffer through the current encoder.

The final macOS Unity visibility issue was a reversed-depth mismatch. Unity's Metal GPU projection uses reversed Z, but the Unity Metal player configuration was still using `DepthBuffer::Linear01`, producing the wrong depth comparison for Unity's depth buffer. The fix is to use `DepthBuffer::Linear10` for Metal in the Unity plugin, matching D3D-style reversed depth. The 360 picture backdrop shader also needed a Unity/external-Metal-device far-depth convention: `z = w` behaves as far for normal-Z standalone Metal, but as near for Unity's reversed-Z path, so the external-device Metal path rewrites the 360 backdrop far depth to clip-space z `0.0`. The default Unity Metal path now uses Unity's current command encoder with deferred frame begin; the V2 plugin-command-buffer path remains opt-in because it still does not produce the verified visible framebuffer output.

The current iOS state is now compile/package-proven and simulator-runtime-proven for the local `sample1.imm` visual/audio gate:

- `code/projects/ios/CMakeLists.txt` builds an iOS `ImmUnity` static library target for Unity player linking.
- The iOS package artifact is a merged static archive, not only the `appImmUnity/src/main.cpp` object archive. The merge step combines the Unity plugin object archive with the player, importer, core, Metal renderer, AVFoundation sound backend, Opus decoder, and required third-party static libraries.
- The iOS target uses the shared Metal renderer and the native AVFoundation/Opus sound path. This is compile/link/package-proven for iPhoneOS and runtime-proven in the arm64 iOS Simulator. Physical-device runtime proof is a later hardware-validation task, not part of the current completion gate.
- `com.immersive-foundation.imm-unity/Plugins/iOS` exists with Unity `.meta` importer settings for `libImmUnityPlugin.a`.
- `com.immersive-foundation.imm-unity/Plugins/iOS/ImmUnityPluginRegister.mm` registers the static iOS native rendering plugin through Unity's `UnityRegisterRenderingPluginV5` path. The managed iOS startup calls this explicit registration before native `Init()`, so iOS static builds receive `IUnityGraphics`/`IUnityGraphicsMetal` before renderer initialization.
- The Unity project is now set to export Apple Silicon iOS Simulator builds (`iOSSimulatorArchitecture: 1`, arm64). The current completion gate uses this arm64 simulator path; physical iPhone/iPad runtime proof follows later when hardware is available.
- `com.unity.ads` was removed from the sample project package manifest because its bundled `UnityAds.framework` provided a device-iOS arm64 slice that blocked arm64 iOS Simulator linking and is unrelated to IMM rendering/audio validation.
- iOS Simulator builds must use arm64 simulator archives for both `ImmUnityPlugin` and `ImmStrokeReader`. Simulator CMake builds no longer overwrite the tracked UPM iPhoneOS package archives; simulator archives are injected into generated simulator Xcode projects only for local smoke testing.
- The iOS compile gate is now:

```sh
cmake -S code/projects/ios -B build/ios \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/ios --target ImmUnity --config Release
cmake --build build/ios --target ImmUnityIOSLinkSmoke --config Release
```

This passed locally on 2026-05-25 and produced `build/ios/unity/libImmUnityPlugin.a`, copied to `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/iOS/libImmUnityPlugin.a`.
The merged package archive is about 4.1 MB; the raw plugin-object archive is kept separately under `build/ios/unity-objects`. The link-smoke target force-loads the merged archive into an iPhoneOS executable with Metal, QuartzCore, Foundation, AVFoundation, zlib, and the bundled codec/static-library dependencies, catching missing-object packaging errors before Unity/Xcode.

The current local arm64 iOS Simulator gate is:

```sh
cmake -S code/projects/ios -B build/ios-simulator \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build/ios-simulator --target ImmUnity --config Release
cmake --build build/ios-simulator --target ImmStrokeReader --config Release
```

On 2026-05-25, the generated Unity simulator Xcode project exported with `ARCHS = arm64`, no `UnityAds` references, and successfully built with unsigned `xcodebuild` for iPhone 16 Simulator. Launching that app creates a Unity Metal device (`Apple iOS simulator GPU`), registers the static IMM rendering plugin, initializes IMM successfully with Unity Metal interfaces present (`renderer enum 16`, `v1=1`, `v2=1`, `device=1`), creates and initializes the Metal renderer, initializes the IMM player, and loads `exampleImmFiles/sample1.imm`. Native logging now writes to `Application.temporaryCachePath`.

The arm64 iOS Simulator now visibly renders `sample1.imm` after the asynchronous import reaches sequence readiness. Console render reports reached `drawCalls=33`, `paintDrawCalls=32`, `pictureDrawCalls=1`, `picture360DrawCalls=1`, and `triangles=498298` at viewport `1179x2556`; the captured screen at `build/ios-simulator-rendering-after-wait.png` shows the character, branch, paint layers, and 360 backdrop. The same native log proves embedded audio decode/playback on simulator: three Ogg Opus sounds decode to temporary PCM WAVs, AVFoundation accepts three `Play()` calls, all three enter `state=playing`, and all three report playback progress past the 0.25-second threshold. This satisfies the current iOS runtime gate; physical-device proof is tracked as later hardware validation.

The local Unity iOS Xcode-project smoke command exists:

```sh
/Applications/Unity/Hub/Editor/2022.3.62f2/Unity.app/Contents/MacOS/Unity \
  -batchmode -quit \
  -projectPath code/ImmUnitySampleProject \
  -executeMethod ImmPlayer.Editor.BuildAutomation.BuildIOSDevelopment \
  -logFile build/unity-ios-xcode-build.log
```

After installing Unity iOS Build Support, this command completed locally on 2026-05-25 and was rerun after enabling the iOS AVFoundation/Opus audio path. It exported `code/ImmUnitySampleProject/Builds/iOS/Unity-iPhone.xcodeproj`; the generated `project.pbxproj` references `Libraries/com.immersive-foundation.imm-unity/Plugins/iOS/libImmUnityPlugin.a` and includes it in the Frameworks phase.

After installing the missing Xcode iOS platform runtime, the generated Xcode project also compiles and links locally as an unsigned iPhoneOS build:

```sh
xcodebuild \
  -project code/ImmUnitySampleProject/Builds/iOS/Unity-iPhone.xcodeproj \
  -scheme Unity-iPhone \
  -configuration Debug \
  -destination generic/platform=iOS \
  CODE_SIGNING_ALLOWED=NO \
  COMPILER_INDEX_STORE_ENABLE=NO \
  CC=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang \
  CXX=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++ \
  build
```

This passed locally on 2026-05-25 and produced `IMMUnityTest.app` under Xcode DerivedData. The build was rerun after refreshing the tracked UPM iOS archive from `build/ios/unity/libImmUnityPlugin.a`; both the package archive and the regenerated Xcode-project copy contain `piSoundEngineAVFoundation.mm.o` plus Opus decoder objects. That leaves iOS Unity package export and native link proven locally; runtime proof for the current gate comes from the arm64 iOS Simulator.

Known nonfatal iOS link warning: because the IMM plugin archive currently carries its own Opus/libpng objects and Unity's generated `libiPhone-lib.a` also contains some codec/libpng objects, Xcode emits duplicate-symbol warnings during the `UnityFramework` link. The refreshed unsigned build still succeeds. This should be cleaned up before calling the iOS package production-ready, either by hiding/renaming bundled codec symbols or by restructuring iOS third-party linkage so Unity and IMM do not export colliding global codec symbols.

CI now builds and uploads macOS and iOS `ImmUnity` artifacts for UPM package assembly, while still avoiding the temporary renderer-image validation jobs.

## Progress Tracker

This tracker is the quickest place to see what is complete, what is partially complete, and what remains unproven. The detailed phase sections below remain the source of truth for scope and acceptance criteria.

| Area | Status | Evidence / Remaining Work |
| --- | --- | --- |
| macOS CMake configure path | Complete | `cmake -S code/projects/macos -B build/macos -DIMM_BUILD_VIEWER=ON` configures the viewer targets. |
| Shared Metal renderer API plumbing | Complete for standalone bring-up | `piRenderer::API::Metal` exists and `piRenderer::Create()` can construct `piRendererMetal`. |
| Metal renderer backend skeleton | Complete for standalone bring-up | `code/libImmCore/src/libRender/metal/piMetal_Renderer.mm` compiles and is linked into macOS targets. Unsupported paths report explicit one-time errors. `piRendererMetal::Report()` now emits a concise standalone-proven and standalone-unsupported feature summary, and helper validation requires both report lines. |
| Native standalone Metal host | Complete for current sample playback | `appImmViewerMetal.app` owns the AppKit/MTKView lifecycle, Metal device, drawable, render pass, resize path, and bundled settings. |
| Interactive standalone launch | Complete for basic use | Terminal and `open ... --args` launches foreground the app window; closing the last window terminates the app. |
| Command-line and bundled-settings contract | Complete | One JSON settings argument and one content path are supported; extra content paths are rejected; content-only launches use bundled Metal settings. |
| App bundle/document metadata | Complete for unsigned local builds | `Info.plist` declares app identity, executable, `.imm` document type, exported UTI, and bundled Metal settings. Finder/open-file launches are accepted before startup. The app has a basic `File > Open...` panel, a deterministic app-local Open Recent `.imm` list that also notifies AppKit's document controller, and `.imm` drag/drop onto the Metal view. In-process document replacement now unloads the current document through a synchronous CPU/SPU/GPU state path, reinitializes the viewer, resets first-frame playback state, updates the window title, and is covered by `validateAppImmViewerMetalReload`. Interactive failed-open handling presents a native alert and attempts to restore the previous document instead of immediately killing the app; the normal-run validation smokes the restore path with alerts suppressed. The interactive audio validation also smokes Open Recent population and cleanup. Signing, notarization, icons, and user preferences remain product polish. |
| Static paint playback | Proven for `sample1.imm` sample playback | Validation now renders the expected branch and character over the 360 backdrop with expected draw/triangle counts and clean teardown. Metal paint uses the existing blue-noise texture and sample-mask alpha coverage path rather than forcing paint fragments opaque. The full-frame hash is intentionally not fixed because alpha coverage now depends on blue-noise and time-driven draw-in state. The thresholded Windows DirectX reference comparison is now part of the configured local aggregate gate. |
| Pretessellated paint playback | Proven for `sample1.imm` sample playback | Validation runs the pretessellated settings path, renders a capture matching the static composition for `sample1.imm`, and catches shader/pipeline/blank-output regressions. Metal paint uses the existing blue-noise texture and sample-mask alpha coverage path rather than forcing paint fragments opaque. The full-frame hash is intentionally not fixed because alpha coverage now depends on blue-noise and time-driven draw-in state. |
| Paint visual correctness baseline | Complete for the current standalone gate | The Windows DirectX frame-240 capture remains the current authoritative non-Metal baseline for `sample1.imm`: a small character standing on a filled mossy branch over a 360 backdrop. The current Metal captures show that same scene composition, and the configured `validateAppImmViewerMetalReferenceCompare` target passes with broad regression thresholds against that Windows reference. Tighter visual-equivalence thresholds can be added after contact-sheet review, but they are not blocking the standalone milestone. |
| 360 equirect picture playback | Complete for the current standalone gate | Validation requires `picture360DrawCalls=1`; `sample1.imm` uses a 360 picture backdrop. Metal forces 360 picture depth to the far plane so the panorama behaves as a backdrop instead of covering previously drawn paint. The optional `validateAppImmViewerMetalAuthoredPicture360` target now requires the more specific `picture360EquirectDrawCalls>=1`; it passes locally with `3990504411070340_MELANCHOLY2021_by_CRYHARDSTUDIOS.imm` and is included in `validateAppImmViewerMetal`/CTest when `IMM_METAL_AUTHORED_360_PICTURE_PATH` is configured. A focused picture-candidate sweep also found multiple additional authored 360 equirect files. Redistributable authored 360 content would improve CI breadth but is not a blocker for the standalone milestone. |
| 2D picture layer playback | Complete for the current standalone gate | Standalone validation draws a deterministic 2D-picture quad with the generated Metal picture shader, texture/sampler binding, layer/display/frame constants, and blue-noise sample-mask alpha path, then reads back pixels and requires `picture2DShader=1`. The optional `validateAppImmViewerMetalAuthoredPicture2D` target validates a local authored 2D-picture IMM file, requires `picture2DDrawCalls>=1` inside the player validation loop before declaring success, passes locally with `896098972954263_Suzanne_by_Starmen tv.imm`, and is included in `validateAppImmViewerMetal`/CTest when `IMM_METAL_AUTHORED_2D_PICTURE_PATH` is configured. Redistributable authored 2D content would improve CI breadth but is not a blocker for the standalone milestone. |
| 360 cubemap picture playback | Smoke-proven; authored content deferred | Metal cubemap shaders now use the same blue-noise/sample-mask alpha coverage convention as GL, and the renderer binds blue noise for cubemap draws. Standalone validation creates a deterministic cube texture, draws it through the generated Metal cubemap shader over the unit-cube helper mesh, reads pixels back, and requires `picture360CubemapShader=1`. Validation also reports `picture360EquirectDrawCalls` and `picture360CubemapDrawCalls`, and the optional `validateAppImmViewerMetalAuthoredPicture360Cubemap` target remains ready if a real cubemap IMM appears. No authored cubemap candidate has been found in the large local cache search, so authored cubemap validation is deferred and is no longer a standalone-readiness blocker. |
| Model layer playback | Out of scope for current standalone milestone | Model layers are not part of the current readiness goal. Existing model renderer code may continue to compile, but authored model-layer coverage is no longer a blocker for standalone macOS player progress. |
| Audio playback | Required; implemented for first standalone sample and one additional authored WAV sample | The standalone Metal host has an AVFoundation backend for macOS and interactive/normal runs select it by default. Uncompressed PCM/WAV-style buffers are routed through AVFoundation temp WAV playback. Embedded Ogg Opus blobs are decoded with the bundled Opus decoder into temporary WAV files for AVFoundation playback. This temp-WAV/`AVAudioPlayer` path is the accepted first standalone milestone policy because it preserves current IMM sound-engine semantics while avoiding a second streaming decoder subsystem during renderer bring-up. Decode or player-creation failure now returns `-1` and fails the sound layer load instead of creating a silent placeholder. The native standalone app exposes document-level mute/volume through the Audio menu and `M` / `=` / `-` keyboard controls, using the existing IMM document-volume path rather than AVFoundation-specific state. The macOS Opus build intentionally disables intrinsics to avoid the upstream runtime-NEON detection path and keep configure/build output clean on Apple Silicon. `validateAppImmViewerMetalAudio` checks that `sample1.imm` decodes three distinct stereo Ogg Opus sounds, reaches accepted AVFoundation `Play()` calls, observes AVFoundation transition into `state=playing`, observes playback reach the configured 1.0-second progress threshold, does not hit an Ogg Opus decode failure or a rejected play request, and reports clean AVFoundation deinit with zero temp-file removal failures and removed temp files covering the validated sound count; the latest run passed with `opusDecoded=3 wavAdded=0 total=3 playCalls=6 playingStates=6 progressMarkers=6 progressThresholdSec=1.0`. `validateAppImmViewerMetalLongAudio` keeps validation alive longer and requires the same Opus sounds to reach a 3.0-second progress threshold; it passes locally with `opusDecoded=3 wavAdded=0 total=3 playCalls=6 playingStates=6 progressMarkers=6 progressThresholdSec=3.0`. `validateAppImmViewerMetalInteractiveAudio` launches without render validation enabled, requires the normal-run AVFoundation backend rather than the null backend, decodes and plays the same three Opus sounds, observes playback progress, runs native volume/mute, playback-control, Open Recent, and failed-open restore smokes, and exits through the normal cleanup path with clean AVFoundation deinit and temp-file cleanup coverage; it passes locally with `opusDecoded=3 wavAdded=0 total=3 playCalls=6 playingStates=6 progressMarkers=6 progressThresholdSec=1.0`. The optional `validateAppImmViewerMetalAuthoredAudio` and `validateAppImmViewerMetalAuthoredLongAudio` targets validate additional authored audio content; they pass locally with `/Users/andrewbaker/Documents/Quill/Snoopy/Snoopy.imm`, requiring three WAV sound objects, accepted AVFoundation `Play()` calls, observed `state=playing` transitions, playback-progress markers, and clean AVFoundation teardown, with latest runs reporting `opusDecoded=0 wavAdded=3 total=3 playCalls=6 playingStates=6 progressMarkers=6 progressThresholdSec=1.0` and `progressThresholdSec=3.0`. Redistributable authored audio would improve CI breadth but is not a blocker for the standalone milestone; output-device UI and a streaming backend for very large/long audio can follow after reliable playback is proven. |
| Transparency/depth/culling visual equivalence | Complete for the current standalone gate | Current sample and helper draws exercise paint alpha coverage, 360 equirect picture alpha coverage, 2D-picture alpha coverage through a deterministic shader smoke draw, cubemap alpha coverage through a deterministic shader smoke draw, depth/write mask, culling, pixel-checked offscreen non-indexed and indexed triangle paths, and unit-cube paths. Local private-cache validation now exercises authored 2D and authored 360 equirect picture playback. The Metal renderer report records the renderer feature surface proven by standalone validation and the currently unsupported/stubbed areas. |
| Resize/render-target recreation | Complete for validation path | Automated resize validation recreates render targets and verifies nonblank 800x600 output. |
| Capture generation | Complete for validation path | `IMM_METAL_VALIDATE_CAPTURE_DIR` writes and validates PPM captures for static, pretessellated, and resize cases by default. Set `IMM_METAL_VALIDATE_CAPTURE_FORMAT=png` for directly inspectable PNG captures. |
| Native-frame failure handling | Complete | Forced native-frame setup failure exits with validation status `2` and clean teardown marker. |
| Repeated launch/teardown stability | Complete for repeated process runs and same-process reload | `validateAppImmViewerMetalRepeat` runs strict `sample1.imm` playback validation repeatedly. `validateAppImmViewerMetalReload` additionally reloads `sample1.imm` inside the same app process after the active document reports loaded, then validates a nonblank post-reload frame plus clean teardown. The Metal renderer now reports live render targets, states, textures, samplers, shaders, buffers, vertex arrays, queries, and retained buffers during deinit; standalone validation fails if any of those counts are nonzero. |
| Legacy macOS `appImmViewer` build | Complete as a compile gate | The legacy viewer target builds, but visual comparison against OpenGL is not automated. |
| `ImmUnity` macOS build | Complete as a package/player-build gate | `cmake --build build/macos --target ImmUnity --config Release` passes with the current Unity Metal host-adapter code. `ImmPlayer.Editor.BuildAutomation.BuildMacOSDevelopment` also builds `Builds/macOS/IMMUnityTest.app` with Metal forced and includes `ImmUnityPlugin.bundle` plus `StreamingAssets/sample1.imm`. |
| `ImmStrokeReader` macOS build | Complete as a compile gate | The stroke-reader target builds through the minimal non-rendering core. |
| Unity Metal host adapter | Complete for macOS standalone `sample1.imm` smoke | Native code acquires Unity Metal V1/V2 interfaces, selects `piRenderer::API::Metal`, receives managed viewport data, and reaches native render events. The default path now renders through Unity's current Metal command encoder; the V2 plugin-command-buffer path is kept opt-in because it is not the visually proven path. The layer-refresh crash was fixed by replacing the direct `Player::LayerInfo` export with a Unity ABI struct that uses fixed UTF-16 `char16_t` strings instead of macOS 4-byte `wchar_t` strings. Unity Metal now uses reversed-depth configuration (`DepthBuffer::Linear10`) and the external-device Metal shader path puts 360 backdrops at reversed-Z far depth. Default macOS Unity player smoke now exits cleanly and captures visible, upright `sample1.imm` content at `build/unity-smoke/macos-current-proof.png`, with `drawCalls=38`, `paintDrawCalls=37`, `pictureDrawCalls=1`, `picture360DrawCalls=1`, `triangles=645802`, and `nonZero=230400`. A batchmode Editor play-mode smoke now enters Play Mode, loads `sample1.imm`, decodes audio, loads GPU resources, unloads, and exits cleanly; batchmode Editor framebuffer capture remains unproven, so visual proof still comes from the macOS Unity standalone player capture. Temporary bring-up probes/logging can be cleaned up as follow-up clutter reduction. |
| Unity macOS audio | Complete for macOS standalone `sample1.imm` smoke | The Unity macOS plugin now selects AVFoundation instead of the null sound backend. Latest local smoke after rebuilding and copying the plugin into `IMMUnityTest.app`: `sample1.imm` decodes three embedded Ogg Opus sounds, accepts three AVFoundation `Play()` calls, reaches `state=playing` for all three sounds, reports playback progress for all three, and tears down with `soundsDestroyed=3`, `tempFilesRemoved=3`, and `tempFileRemoveFailures=0`. Evidence: `code/ImmUnitySampleProject/Builds/macOS/imm_player_log.txt` from the run that also produced `build/unity-smoke/macos-current-proof.png`. |
| iOS `ImmUnity` plugin target | Complete for the current Unity gate: device compile/package/Xcode-link plus arm64 simulator visual/audio runtime | `cmake --build build/ios --target ImmUnity --config Release` passes and produces a merged iPhoneOS `build/ios/unity/libImmUnityPlugin.a`, copied into `Packages/com.immersive-foundation.imm-unity/Plugins/iOS`. `cmake --build build/ios --target ImmUnityIOSLinkSmoke --config Release` force-loads that archive into an iPhoneOS executable and passes. `ImmPlayer.Editor.BuildAutomation.BuildIOSDevelopment` exports `Builds/iOS/Unity-iPhone.xcodeproj`, and an unsigned `xcodebuild` iPhoneOS build succeeds locally with Xcode's default clang and `CODE_SIGNING_ALLOWED=NO`. The Unity project exports arm64 iOS Simulator projects (`iOSSimulatorArchitecture: 1`); with arm64 simulator `ImmUnityPlugin` and `ImmStrokeReader` archives injected into the generated simulator project, unsigned simulator `xcodebuild` succeeds for iPhone 16 Simulator. Simulator launch creates a Unity Metal device, registers the static rendering plugin, initializes IMM and its Metal renderer, loads `sample1.imm`, renders visible paint/character/branch/360 backdrop after sequence readiness, and proves AVFoundation Opus decode/play/progress for the three embedded sounds. Later physical-device runtime validation should cover visible rendering, unload/background behavior, signing/provisioning, and embedded-audio playback on iPhone/iPad hardware. |
| Native standalone product polish | Started for basic local use | The local app bundle has a minimal app menu, `File > Open...`, Open Recent integration, a Playback menu for play/pause, restart, previous, and next, `.imm` drag/drop onto the Metal view, document metadata, in-process single-document reload, recoverable failed-open alerts with previous-document restore, first-pass native audio playback, and basic document-level volume/mute controls. Signing, notarization, icon, user preferences, output-device UI, distribution, and update flow remain outside the renderer/audio readiness gate. |

## Immediate Unity Plugin Work Queue

This is the current ordered path from the working standalone Metal player to a useful Unity plugin for macOS and iOS:

1. Fix normal macOS Unity sample startup:
   - Status: complete locally on 2026-05-25.
   - The crash was isolated to layer refresh and fixed in the Unity native ABI for `GetLayerInfoByIndex`.
   - Acceptance evidence: the macOS Unity standalone player loads `sample1.imm`, starts layer refresh, initial playback state, and initial spawn-area setup, reaches nonzero IMM draw submission, writes a smoke PNG, and exits cleanly without `IMM_UNITY_DISABLE_FEATURE_POST_LOAD`.

2. Prove Unity framebuffer composition:
   - Status: complete locally for the macOS Unity standalone sample gate on 2026-05-25.
   - Root cause: Unity Metal uses reversed-Z projection/depth, while the Unity Metal player configuration and 360 backdrop shader were still using normal-Z assumptions.
   - Fix: Metal Unity configuration now uses `DepthBuffer::Linear10`, the external-device Metal shader path places 360 backdrops at reversed-Z far depth, and the default Unity Metal submission path uses Unity's current command encoder with deferred frame begin.
   - Acceptance evidence: `build/unity-smoke/macos-current-proof.png` visibly contains the `sample1.imm` branch, character, and 360 backdrop, and `build/unity-macos-current-proof.log` reports `drawCalls=38`, `paintDrawCalls=37`, `pictureDrawCalls=1`, `picture360DrawCalls=1`, `triangles=645802`, and `nonZero=230400`.

3. Prove macOS Unity audio:
   - Status: complete locally for the macOS Unity standalone sample gate on 2026-05-25.
   - Fix: the macOS Unity plugin now selects the shared AVFoundation sound backend instead of the null backend.
   - Acceptance evidence: `code/ImmUnitySampleProject/Builds/macOS/imm_player_log.txt` from the latest Unity macOS smoke shows three `Decoded Ogg Opus sound to PCM temp WAV for AVFoundation` lines, three accepted AVFoundation `Play()` calls, three `state=playing` transitions, three playback-progress markers, and clean temp-file teardown. The same run captured visible output at `build/unity-smoke/macos-current-proof.png`.

4. Exercise the Unity Editor path:
   - Status: complete locally for a batchmode Editor lifecycle smoke on 2026-05-25.
   - The smoke opens `SampleScene.unity`, enters Play Mode, loads `sample1.imm`, decodes the sample audio through AVFoundation, reaches GPU load, exits Play Mode, unloads CPU/SPU/GPU state, and exits Unity cleanly.
   - Acceptance evidence: `build/unity-macos-editor-playmode.log` reports `[IMM_EDITOR_SMOKE] passed: native Editor play-mode load/audio smoke completed`, and `code/ImmUnitySampleProject/imm_player_log.txt` reports `Loaded in SPU!`, `Loaded in GPU [0]!`, `Has 1 chapters`, clean unload, and clean AVFoundation deinit.
   - Limitation: the batchmode Editor smoke does not produce a framebuffer PNG. Visual proof remains the macOS Unity standalone player capture.

5. Clean the Unity bring-up scaffolding:
   - Remove temporary triangle-only paths, broad suppress-draw switches, excessive per-frame logs, and isolation-only environment variables once their fixes have landed.
   - Keep only small, documented smoke hooks that are useful for future regressions, such as opt-in framebuffer capture.
   - Acceptance: CI artifacts and logs are about deliverable package/player builds, not temporary renderer investigation clutter.

6. Complete iOS Unity validation:
   - Status: complete locally for the current macOS + arm64 iOS Simulator gate on 2026-05-25.
   - `ImmPlayer.Editor.BuildAutomation.BuildIOSDevelopment` now completes after installing Unity iOS Build Support.
   - The generated Xcode project references `libImmUnityPlugin.a` from the Unity package without manual edits.
   - Full unsigned `xcodebuild` compile/link of the generated iPhoneOS project now succeeds locally after installing the missing Xcode iOS platform runtime and forcing Xcode's clang instead of Homebrew LLVM.
   - Acceptance for the current gate: the arm64 iOS Simulator player initializes the Metal renderer, loads `sample1.imm`, renders visible paint plus the branch/character and 360 backdrop, unloads cleanly, and proves embedded IMM audio decode/play/progress through the native AVFoundation/Opus path.
   - Later hardware gate: run on a Metal-capable iPhone or iPad to prove signing/provisioning, physical-device rendering, audio policy, and background/foreground behavior.

## What To Expect With `sample1.imm`

When you run:

```sh
build/macos/viewer/appImmViewerMetal.app/Contents/MacOS/appImmViewerMetal exampleImmFiles/sample1.imm
```

you should expect a normal foreground macOS window titled:

```text
sample1.imm - IMM Metal Player
```

The current `sample1.imm` scene should render as nonblank IMM playback with:

- a 360 equirectangular forest-style image backdrop;
- a filled mossy branch and small character visibly composited in front of that backdrop;
- no separate 2D picture layers proven by this sample;
- audio output through the macOS AVFoundation backend. `sample1.imm` contains three embedded stereo Ogg Opus sounds, which now decode to PCM for playback.

The bundled default settings file uses `RenderingTechnique: "Static"`. The pretessellated path is also exercised by automation and currently matches the expected `sample1.imm` scene composition.

Basic navigation is available:

- hold left mouse and drag to look around;
- `W`, `A`, `S`, `D` move horizontally/forward/back;
- `Q` and `E` move down/up;
- hold Shift to move faster;
- hold Control to move slower;
- `P` toggles pause/resume;
- `Z`, `X`, `C`, and `V` map to story skip-back, skip-forward, restart, and continue.

What this sample proves in automation is narrower than full visual equivalence. The validation harness proves that `sample1.imm` renders nonblank output, submits paint draw calls, visibly composites static and pretessellated paint over the 360 backdrop, submits one 360-picture draw call, draws the expected triangle count, survives resize validation, tears down cleanly, and decodes the sample's three embedded Ogg Opus sounds through the AVFoundation backend. This sample does not prove authored 2D pictures, cubemap picture layers, broad audio behavior across other files, or broad reference-image matching.

Current full standalone validation command:

```sh
cmake --build build/macos --target validateAppImmViewerMetal --config Release
```

This aggregate target runs playback validation, audio decode validation, longer audio playback validation, normal-run interactive audio/control smoke validation, command-line contract validation, capture validation, native-frame failure validation, app-bundle validation, content-path launch validation, repeated launch/teardown validation, and in-process reload validation. When optional authored audio, authored picture, local content sweep, or reference-image CMake paths are configured, the matching local validation targets are included in the same aggregate gate.

Latest local direct aggregate run: `cmake --build build/macos --target validateAppImmViewerMetal --config Release` passed on 2026-05-25. That run included the thresholded Windows DirectX reference comparison, local content sweep, sample playback/capture, short and long `sample1.imm` Opus audio validation, normal-run interactive audio/control smoke, authored WAV audio validation with `Snoopy.imm`, authored 2D picture validation, authored 360 equirect picture validation, native-frame failure handling, app-bundle/content-path checks, repeated launch/teardown, and in-process reload.

The render validation cases intentionally use the null sound backend so render captures stay deterministic. The aggregate target also runs focused audio contracts. To run only native audio decode/playback wiring for `sample1.imm`, use:

```sh
cmake --build build/macos --target validateAppImmViewerMetalAudio --config Release
```

To run the longer bundled-audio gate, which requires playback progress to reach 3.0 seconds, use:

```sh
cmake --build build/macos --target validateAppImmViewerMetalLongAudio --config Release
```

To run the normal-run audio smoke without enabling render validation, use:

```sh
cmake --build build/macos --target validateAppImmViewerMetalInteractiveAudio --config Release
```

The expected audio signal is three distinct `Decoded Ogg Opus sound to PCM temp WAV for AVFoundation` IDs before `Loaded in SPU!`, with no Ogg Opus decode-failure warning.

Current macOS compile-gate commands:

```sh
cmake --build build/macos --target appImmViewer --config Release
cmake --build build/macos --target ImmStrokeReader --config Release
cmake --build build/macos --target ImmUnity --config Release
```

These targets have been checked after the standalone Metal renderer, native host, resource-lifetime, reload, failed-open recovery, and macOS AVFoundation/Opus audio backend work, so the shared renderer/core/audio changes still compile for the legacy native viewer, the stroke reader, and the Unity plugin bundle.

Latest local compile-gate run on 2026-05-25: all three commands above passed after the full standalone Metal aggregate validation.

Additional focused standalone validation targets:

```sh
cmake --build build/macos --target validateAppImmViewerMetalPlayback --config Release
cmake --build build/macos --target validateAppImmViewerMetalCliContract --config Release
cmake --build build/macos --target validateAppImmViewerMetalCapture --config Release
cmake --build build/macos --target validateAppImmViewerMetalAudio --config Release
cmake --build build/macos --target validateAppImmViewerMetalLongAudio --config Release
cmake --build build/macos --target validateAppImmViewerMetalInteractiveAudio --config Release
cmake --build build/macos --target validateAppImmViewerMetalNativeFrameFailure --config Release
cmake --build build/macos --target validateAppImmViewerMetalBundle --config Release
cmake --build build/macos --target validateAppImmViewerMetalContentOverride --config Release
cmake --build build/macos --target validateAppImmViewerMetalRepeat --config Release
cmake --build build/macos --target validateAppImmViewerMetalReload --config Release
```

Current CTest validation command:

```sh
ctest --test-dir build/macos -R appImmViewerMetalPlayback --output-on-failure
```

Current full standalone Metal CTest command:

```sh
ctest --test-dir build/macos -R appImmViewerMetal --output-on-failure
```

This runs the same playback, audio decode, longer audio playback, normal-run interactive audio/control smoke, command-line contract, capture, native-frame failure, app-bundle, content-path launch, repeated launch/teardown, and in-process reload validation checks through CTest. When optional authored content, content sweep, or reference comparison paths are configured at CMake time, CTest also registers those checks.

Latest local run: `ctest --test-dir build/macos -R appImmViewerMetal --output-on-failure` passed 17/17 tests in 81.68 seconds on 2026-05-25, including the locally configured authored audio, authored 2D picture, authored 360 equirect picture, content-sweep, and thresholded Windows DirectX reference-comparison cases.

If optional authored audio, authored 2D-picture, authored 360-picture, or authored cubemap paths are configured at CMake time, CTest also registers matching `appImmViewerMetalAuthored*` cases so local private-content coverage is visible in the test list instead of living only as manually invoked targets.

Current direct validation command:

```sh
code/appImmViewer/scripts/validate_metal_standalone.sh build/macos/viewer/appImmViewerMetal.app/Contents/MacOS/appImmViewerMetal exampleImmFiles/sample1.imm
```

Current validation capture command:

```sh
  IMM_METAL_VALIDATE_CAPTURE_DIR=build/macos/metal-validation-captures \
  code/appImmViewer/scripts/validate_metal_standalone.sh build/macos/viewer/appImmViewerMetal.app/Contents/MacOS/appImmViewerMetal exampleImmFiles/sample1.imm
```

The same build, run, validation, and capture commands are also documented in `BUILDING.md` under "Quick reference - macOS Metal standalone player" so the native player can be discovered without reading this implementation plan.

The validation harness enables offscreen Metal readback through the standalone player and requires a successful `IMM Metal validation:` report. It currently proves that the native host can load `sample1.imm`, submit Metal draw calls, submit triangles, render nonzero offscreen pixels, render the sample's 360 picture backdrop, visibly composite static and pretessellated paint over that backdrop, and exit cleanly for static, pretessellated, and resize-after-startup paths. The script sets minimum draw-call, picture draw-call, 360 picture draw-call, and triangle thresholds so a clear-only or paint-only frame is not accepted as a playback pass, rejects the known old static backdrop-only hash, rejects the old static and pretessellated opaque-paint hashes from before Metal honored paint alpha coverage, then compares the reported pixel count, nonzero pixel count, draw-call count, and triangle count against the current known-good output.

When `IMM_METAL_VALIDATE_CAPTURE_DIR` is set, the harness writes captures for the validated offscreen frames:

- `static.ppm`, `pretessellated.ppm`, and `resize.ppm` by default;
- `static.png`, `pretessellated.png`, and `resize.png` when `IMM_METAL_VALIDATE_CAPTURE_FORMAT=png`.

The underlying player also accepts `IMM_METAL_VALIDATE_CAPTURE_PATH` for single-run capture. Captures are generated from the same offscreen texture used for hash validation, so they are useful for inspecting camera orientation, scale, depth/culling, transparency, and color regressions before moving a change into the Unity host path.

If `IMM_METAL_VALIDATE_CAPTURE_PATH` ends in `.png`, the standalone Metal player writes an RGB PNG through the shared `piImage`/libpng path. Other extensions, including `.ppm`, use the existing raw PPM writer. This lets the Windows DirectX baseline command and the Metal standalone capture command produce directly inspectable PNG files from the same decoded `C3_11_11_10_FLOAT` render-target readback path.

The validation script canonicalizes `IMM_METAL_VALIDATE_CAPTURE_DIR`, so the capture workflow works both from the repository root and from external working directories. It validates each generated PPM capture header, including magic, dimensions, and max value, and validates PNG captures by signature when `IMM_METAL_VALIDATE_CAPTURE_FORMAT=png`, so a malformed capture is not accepted as a visual-inspection artifact. The `appImmViewerMetalCapture` CTest case exercises the default PPM capture path. Metal shader and pipeline failures are reported through the renderer reporter as well as the returned error string, so standalone validation logs include compile/link failures instead of only writing them to stderr.

The `appImmViewerMetalBundle` CTest case validates that the standalone player is emitted as `appImmViewerMetal.app`, that the generated executable is inside `Contents/MacOS`, and that `Info.plist` has the expected executable name, bundle identifier, package type, app name, `.imm` document metadata, and exported `com.immersivefoundation.imm` UTI metadata. The bundle also includes `Contents/Resources/appImmViewerMetal-settings.json`, a Metal/Static default settings file used when no JSON settings path is supplied.

The `appImmViewerMetalContentOverride` CTest case launches the bundled executable with only the IMM content path from a temporary working directory. This proves the documented sample-content launch path is using the bundled Metal settings file instead of depending on a repo-root relative `code/appImmViewer/exe/settings.json`. It also requires the validation-time window-title marker to report `sample1.imm - IMM Metal Player`, proving the native title path uses the active content file.

The standalone Metal player accepts `IMM_METAL_LOG_PATH` for its `piLog` output. The validation script assigns a private log path per case, which keeps direct script runs, the CMake validation target, and CTest from racing on a shared repo-root `metal_player_debug.txt` file when multiple validation commands run at the same time.

In validation mode, the standalone player pauses MTKView's display link and explicitly pumps `draw` from the main thread until validation succeeds or the script timeout fires. This keeps automated runs deterministic when AppKit does not deliver display-link callbacks reliably under CTest, shell-launched GUI processes, or background execution.

Native frame setup failures are also handled inside the player during validation. Repeated missing render-pass/drawable state or repeated `BeginNativeFrame()` failure now produces an `IMM Metal validation failed:` log and exits with validation status `2` instead of relying only on the outer shell timeout. The `appImmViewerMetalNativeFrameFailure` CTest case covers this path with a validation-only forced native-frame failure switch.

After validation completes, the standalone player now finishes the active Metal frame, terminates through the normal AppKit lifecycle, and returns the validation status from `main`. This keeps the automated gate exercising `applicationWillTerminate` cleanup for the viewer, renderer, render targets, states, buffers, shaders, settings, timer, and log instead of bypassing teardown with an in-frame `exit()` call. The validation script requires the `IMM Metal validation cleanup: done=1 exitCode=0` marker and a zero-count `Metal renderer resource cleanup:` summary, so a playback pass without clean teardown or with leaked Metal renderer objects is not accepted.

Startup and render-target failures now stop the AppKit run loop through the same cleanup path and return a nonzero process status. A forced bad-settings launch has been checked to return exit code `1`, a forced validation-threshold failure has been checked to return exit code `2`, and successful validation returns `0`.

The validation script also enables `IMM_METAL_VALIDATE_HELPER_DRAWS` by default. The player uses that mode to first run pixel-checked offscreen triangle sanity passes: it clears the RG11B10 render target, draws exactly one generated non-indexed triangle, reads the render texture back, and requires the center pixel to be nonzero while corner pixels remain clear. It then clears again and draws an indexed triangle whose index values plus `baseVertex` select a different generated triangle, requiring the center pixel to stay clear while a left-side probe is nonzero. This proves the basic Metal offscreen render-target, pipeline, viewport, clear, non-indexed draw, indexed draw, `vertex_id` behavior for indexed base-vertex draws, color-write, command-buffer, and readback path independently of IMM paint data.

After the single-triangle sanity pass, the player draws the Metal unit quad, a source-alpha blended unit quad, CPU-fallback non-indexed and indexed indirect draws, the position-only unit cube, and the position/normal unit cube helper paths into the native drawable before offscreen IMM playback validation. It also performs offscreen pixel-readback smoke draws for the generated 2D-picture shader and the generated 360-cubemap shader, including texture/sampler bindings, layer/display/frame constants, cube-texture upload/sampling, the unit-cube helper mesh, and the blue-noise sample-mask alpha coverage path. The validation script now requires `picture2DShader=1` and `picture360CubemapShader=1` markers in addition to the non-indexed/indexed triangle markers. It also calls the Metal texture handle/residency methods, harmless fixed-state hint methods, and `RenderMemoryBarrier()` for the validation render texture, requiring a nonzero opaque texture handle, a `memoryBarrier=1` helper marker, and `drawable=1280x720` for the native validation drawable. The same helper pass is wrapped in `StartPerformanceMeasure()` / `EndPerformanceMeasure()`, and the script requires a positive `timingNs` value. This keeps auxiliary geometry, common dynamic blending, indirect draw fallback, texture-handle fallback, fixed-state no-op behavior, render memory-barrier no-op behavior, native drawable sizing in validation mode, CPU-backed timing paths, basic 2D-picture shader bindings, and basic 360-cubemap shader bindings from silently regressing while broader authored 2D picture, authored cubemap, audio, and stroke-geometry equivalence coverage is still limited.

The helper pass also calls `piRendererMetal::Report()` and validation requires two report lines. The `standaloneProven` line documents the backend surface exercised by the standalone gate: native frame ownership, offscreen render targets, clear/viewport/depth/write-mask/culling/blending state, buffers, immediate constants, 2D/cube textures, samplers, render shaders, static and pretessellated paint, 2D picture shader smoke, 360 equirect playback, 360 cubemap shader smoke, indexed/non-indexed/indirect draw paths, unit quad/cube helpers, texture readback, PNG capture, and CPU timing. The `standaloneUnsupported` line documents remaining renderer areas that are intentionally outside the current standalone milestone: external texture wrapping, image load/store, compute shaders, atomic buffers, pixel pack buffers, non-default point size/line width, polygon offset, multi-render-target blend/write-mask handling, and GPU timestamp queries.

Paint wireframe captures can be generated with `IMM_FORCE_PAINT_WIREFRAME=1`. This is a diagnostic switch in the static and pretessellated paint renderers, so picture rendering remains normal while paint raster states are created as wireframe. Some long wireframe edges can be legitimate degenerate triangle-strip connectors, so this diagnostic should still be compared against a known-good backend before being treated as proof of a specific bug.

Paint can also be isolated by brush type with `IMM_FORCE_PAINT_BRUSH_TYPE=0..4`. During the `sample1.imm` investigation, brush type 2 was the dominant visible paint path and reproduced the old bad Metal output by itself, while brush types 0, 1, 3, and 4 contributed little or mostly left the 360 backdrop hash unchanged. That diagnostic remains useful if future paint regressions need to be localized by brush family.

The OpenGL static/pretessellated paint source remains the local source of truth for shader math, chunk offsets, strip topology, vertex/data layout, and transform conventions where behavior is not Metal-host-specific. On the current Apple OpenGL 4.1 runtime, the legacy `appImmViewer` initializes OpenGL but cannot compile the static paint shader because the shader uses `std430` shader storage buffers. That means this machine cannot currently produce a same-machine GL capture for `sample1.imm`; parity work should still compare Metal line-by-line with the GL source and use the Windows DirectX capture as the current runnable visual baseline.

Metal query objects and `StartPerformanceMeasure()` / `EndPerformanceMeasure()` now use a CPU-side nanosecond timer fallback. This is not GPU timestamp support, but it prevents standalone Metal playback from reporting permanently zero frame timings when player performance measurement is enabled.

`RenderMemoryBarrier()` is intentionally a no-op in the current Metal backend. The validated playback path uses command-encoder and command-buffer ordering rather than OpenGL-style memory barriers, so the standalone helper validation only proves that existing IMM calls can reach this API without being reported as unsupported. If future compute, image load/store, or mixed read/write resource paths require explicit Metal fences or encoder boundaries, this method should be revisited.

Unsupported `piRendererMetal` entry points now emit one-time renderer-reporter errors when reached. This keeps missing Metal functionality visible during standalone playback bring-up without spamming logs every frame. The validation harness fails if those unsupported-feature reports appear, and current validation does not hit those unsupported paths.

Current known-good standalone validation values:

| Path | Pixels | Nonzero Pixels | Hash | Draw Calls | Triangles |
| --- | ---: | ---: | ---: | ---: | ---: |
| Static 1280x720 | 921600 | 921600 | time-dependent | 38 | 645802 |
| Pretessellated 1280x720 | 921600 | 921600 | time-dependent | 38 | 645802 |
| Static resize to 800x600 | 480000 | 480000 | time-dependent | 38 | 645802 |

The current `sample1.imm` validation split is `paintDrawCalls=37`, `pictureDrawCalls=1`, `picture2DDrawCalls=0`, and `picture360DrawCalls=1`. The triangle total includes paint geometry plus the 3,072-triangle 360 picture sphere. That means the in-repo sample exercises static/pretessellated paint draw submission plus a 360 picture backdrop, but it still does not prove authored 2D picture playback. The previous backdrop-only hash was `5448870274179528411` and should be treated as a regression if it returns.

The static and pretessellated validation paths use generated 1280x720 settings files so their expected values do not depend on the current display size or macOS window clamping. Their full-frame hashes are intentionally not fixed now that Metal paint honors alpha coverage through animated blue-noise/sample-mask output; validation still checks pixel count, nonzero pixels, draw calls, picture/360 draw calls, triangles, rejects the old backdrop-only hash, and rejects the old opaque-paint hashes `15781045072442920602` and `17258452306413009819`. The resize validation path starts from the static 1280x720 settings, requests an 800x600 resize after startup, verifies that the player logged the validation resize event, and validates the recreated render target by checking the new pixel count, nonzero pixels, draw calls, picture/360 draw calls, and triangles. Its hash is also intentionally not fixed because the sampled frame is time-dependent after the resize. If a legitimate renderer change updates the structural values, rerun `validateAppImmViewerMetal`, inspect the output visually or through a captured reference image, and update `code/appImmViewer/scripts/validate_metal_standalone.sh` with the new expected values. Set `IMM_METAL_VALIDATE_EXPECTED_VALUES=0` only for investigation runs, not for acceptance.

Metal dynamic buffers are now orphaned on `UpdateBuffer()` during an active command buffer and retained until command-buffer completion. This is required for paint chunk constants: without it, multiple encoded draws can observe the final CPU-written chunk offset/scale instead of the per-draw values that were current when each draw was encoded, producing coherent-looking but incorrect stroke geometry.

The Metal renderer now applies `SetWriteMask(..., z)` for depth writes through paired Metal depth states, and `SetDepthState()` enables depth testing for non-GL player paths that select a depth state instead of calling `SetState(piSTATE_DEPTH_TEST, true)`. This is required for `sample1.imm` because paint is rendered before the 360 backdrop; without depth testing, the backdrop can overwrite the strokes even though paint draw calls were submitted. The renderer also supports render-target-0 color writes on/off through cached Metal render-pipeline variants. Broader multi-render-target color channel masks remain a renderer-hardening gap because Metal color write masks are render-pipeline-state properties and need a complete pipeline cache keyed by all relevant state.

The standalone Metal host now honors the existing `Window.FullScreen` setting for interactive runs by using the main screen as the initial frame and entering AppKit full-screen mode after the `MTKView` is installed. Interactive initial drawable sizing uses the view's backing size so Retina/fullscreen playback gets a pixel-sized Metal drawable, while validation mode forces windowed point-size operation so fixed-size automated hashes and resize checks remain deterministic.

Command-line handling preserves the useful standalone override path: one JSON argument selects the settings file, and one non-JSON argument overrides the settings file's `File.Load` content path. If no JSON settings argument is supplied, `appImmViewerMetal` uses the Metal settings JSON bundled in `Contents/Resources`. The app also declares `.imm` as a viewer document type and accepts macOS open-file events before viewer startup, so Finder/open-file launches have the same single-content-path contract. Open-file events after startup, the native `File > Open...` panel, and `.imm` drag/drop onto the Metal view all unload the active viewer document, replace the single loaded content path, reinitialize the viewer, reset first-frame playback state, and update the window title. Additional command-line content paths are rejected with a clear error so multi-file playback remains driven by the settings JSON `File.Load` array rather than an ambiguous argument convention. The `appImmViewerMetalCliContract` and `appImmViewerMetalContentOverride` CTest checks cover launch-time contracts, and `appImmViewerMetalReload` covers the same-process viewer teardown/reinit path with a post-reload render validation. The reload validator waits until the active document reports loaded before triggering the reload and delays readback validation until after the reload completes, avoiding a race between asynchronous load completion and synchronous unload.

The standalone bundle installs a minimal native macOS application menu before entering the AppKit run loop. This provides standard About, Services, Hide, Hide Others, Show All, Quit, `File > Open...`, Open Recent, Playback, and Audio actions for interactive runs. Successfully loaded IMM files are recorded in an app-local `.imm` MRU list outside validation mode and are also reported to `NSDocumentController` where AppKit can use that information. Automated smoke runs skip recent-file registration unless `IMM_METAL_VALIDATE_RECENT_DOCUMENTS=1` is explicitly enabled. The Open Recent smoke validates registration/menu population and then clears both the app-local MRU list and the AppKit recent list so automated runs do not leave test entries behind. The Playback menu exposes play/pause, restart, previous, and next through the existing IMM player command path, and the normal-run interactive validation smokes pause/resume plus restart after audio progress has been observed. If an interactive document replacement fails after the active document has been torn down, the player now presents a native failed-open alert and attempts to restore the previous IMM file; the normal-run smoke validates this restore path with the alert suppressed. The main window title reflects the active IMM filename when one is loaded, including after an open-file reload, which makes command-line, Finder/open-file, drag/drop, and bundled-settings launches easier to identify. Richer document UI remains future standalone-product polish rather than a renderer-readiness gate.

Interactive launches now opt into regular foreground AppKit application behavior and activate the Metal player window after startup, so running the bundled executable from Terminal or `open ... --args` brings up a normal macOS app window. Validation launches keep background-friendly activation behavior because `IMM_METAL_VALIDATE_FRAME` is detected before the AppKit run loop starts.

The current standalone readiness gate is now the native Metal player, `sample1.imm` playback, thresholded Windows DirectX reference comparison, deterministic paint/picture/cubemap helper shader smoke checks, local authored 2D-picture and 360-equirect picture checks when configured, local representative-content sweep when configured, required AVFoundation audio playback, app lifecycle contracts, clean teardown, CTest registration, and macOS compile gates. Audio is part of that readiness gate, not a secondary cleanup item: a standalone player that renders correctly but cannot reliably decode and play embedded IMM audio is not ready for Unity handoff.

The current in-repo asset set only contains `sample1.imm`, so authored 2D picture, authored 360 equirect picture, broader authored audio, broader representative-user-file, and authored cubemap validation are not mandatory in CI unless those assets become redistributable. Local private-cache validation covers authored 2D pictures, authored 360 equirect pictures, authored WAV audio, and representative paint scenes. Authored cubemap validation is explicitly deferred because no real cubemap IMM has been found after a broad local metadata search; the deterministic cubemap shader/runtime smoke remains in the gate so the code path does not silently regress. Model layers are intentionally excluded from the current standalone readiness gate.

### Audio Readiness Gate

Audio is part of standalone readiness, not a Unity-only or post-rendering feature. The current implementation proves the first required path: `sample1.imm` contains three embedded stereo Ogg Opus sounds, the macOS standalone player selects AVFoundation for interactive playback, the bundled Opus decoder converts those Ogg Opus blobs to PCM temp WAV files, and `validateAppImmViewerMetalAudio` fails if those three distinct sounds are not decoded, if AVFoundation `Play()` is not accepted by `AVAudioPlayer`, if playback does not enter `state=playing`, if playback does not reach the configured 1.0-second progress threshold, if a play request is rejected, or if decode/player creation fails. `validateAppImmViewerMetalLongAudio` extends that sample path by keeping validation alive until all six observed play calls reach a 3.0-second progress threshold. `validateAppImmViewerMetalInteractiveAudio` covers the normal-run host path without render validation enabled; it requires AVFoundation rather than the null backend, observes the same decode/play/progress signals, smokes volume/mute, playback-control, Open Recent behavior, and failed-open restore behavior, and checks clean normal-run teardown. Its CMake target gives the process a six-second smoke window so slower local Opus decode still leaves time for playback-progress observation.

The first broader authored-audio check is also in place. `Snoopy.imm` under the local Quill folder declares `Quill.Sound` layers backed by WAV data. When configured with `-DIMM_METAL_AUTHORED_AUDIO_PATH=/Users/andrewbaker/Documents/Quill/Snoopy/Snoopy.imm`, the optional `validateAppImmViewerMetalAuthoredAudio` target runs the standalone player with AVFoundation enabled, relaxed picture requirements, nonblank render validation, and audio requirements of zero Opus decodes plus at least three WAV sound objects, at least one accepted AVFoundation `Play()` call, at least one observed AVFoundation `state=playing` transition, and at least one 1.0-second playback-progress marker. This passed locally with `opusDecoded=0 wavAdded=3 total=3 playCalls=6 playingStates=6 progressMarkers=6 progressThresholdSec=1.0`.

The optional `validateAppImmViewerMetalAuthoredLongAudio` target applies the longer 3.0-second progress threshold to the same configured authored-audio file. This passed locally with `Snoopy.imm` and reported `opusDecoded=0 wavAdded=3 total=3 playCalls=6 playingStates=6 progressMarkers=6 progressThresholdSec=3.0`.

The remaining audio work is future CI/product breadth, not a blocker for the current standalone readiness gate:

1. Add redistributable authored audio coverage to CI if a suitable asset becomes available; until then, the in-repo Opus sample plus private authored WAV validation are the available evidence.
2. Keep the optional authored-audio targets available for local private-cache/Quill validation.
3. Keep product UI such as output-device choice separate from the readiness gate unless it blocks normal playback. Basic document-level mute/volume, playback controls, and failed-open recovery are already present and smoke-validated.
4. Treat a streaming audio backend as post-milestone hardening for very long or memory-heavy authored audio unless a representative standalone-readiness sample proves the temp-WAV policy is insufficient.

### Local Quill Sample Sweep

A recursive local search under `/Users/andrewbaker/Documents/Quill` found these additional `.imm` files:

```text
/Users/andrewbaker/Documents/Quill/UntitledAlpha.imm
/Users/andrewbaker/Documents/Quill/consul-room/Wip10nq-demo.imm
/Users/andrewbaker/Documents/Quill/Snoopy/Snoopy.imm
/Users/andrewbaker/Documents/Quill/Untitled.imm
/Users/andrewbaker/Documents/Quill/taperecorder/taperecorder.imm
/Users/andrewbaker/Documents/Quill/taperecorder/tape-recorder.imm
```

Relaxed standalone Metal validation was run against all six with `IMM_METAL_VALIDATE_HELPER_DRAWS=0` and picture minimums disabled so the files could be classified without inheriting `sample1.imm` expectations. Results:

| File | Result | Draw Coverage |
| --- | --- | --- |
| `consul-room/Wip10nq-demo.imm` | Nonblank Metal render | `drawCalls=30`, `paintDrawCalls=30`, `pictureDrawCalls=0`, `picture2DDrawCalls=0`, `picture360DrawCalls=0`, `modelDrawCalls=0`, `triangles=389808` |
| `Snoopy/Snoopy.imm` | Nonblank Metal render | `drawCalls=21`, `paintDrawCalls=21`, `pictureDrawCalls=0`, `picture2DDrawCalls=0`, `picture360DrawCalls=0`, `modelDrawCalls=0`, `triangles=549690` |
| `taperecorder/taperecorder.imm` | Nonblank Metal render | `drawCalls=4`, `paintDrawCalls=4`, `pictureDrawCalls=0`, `picture2DDrawCalls=0`, `picture360DrawCalls=0`, `modelDrawCalls=0`, `triangles=9962` |
| `taperecorder/tape-recorder.imm` | Nonblank Metal render matching `taperecorder.imm` counters/hash | `drawCalls=4`, `paintDrawCalls=4`, no picture draw calls |
| `UntitledAlpha.imm` | Blank at validation camera/time | `drawCalls=0`, `nonZero=0` after 300 validation frames |
| `Untitled.imm` | Blank at validation camera/time | `drawCalls=0`, `nonZero=0` after 300 validation frames |

PNG captures for the useful nonblank local samples are currently at:

```text
build/macos/quill-validation-sweep-captures/001-Snoopy.imm.png
build/macos/quill-validation-sweep-captures/004-Wip10nq-demo.imm.png
build/macos/quill-validation-sweep-captures/005-tape-recorder.imm.png
build/macos/quill-validation-sweep-captures/006-taperecorder.imm.png
```

The sweep is now reproducible through CMake by configuring the optional local content path:

```sh
cmake -S code/projects/macos -B build/macos \
  -DIMM_BUILD_VIEWER=ON \
  -DIMM_METAL_CONTENT_SWEEP_PATH=/Users/andrewbaker/Documents/Quill \
  -DIMM_METAL_CONTENT_SWEEP_MAX_FILES=100 \
  -DIMM_METAL_CONTENT_SWEEP_MAX_BYTES=50000000 \
  -DIMM_METAL_CONTENT_SWEEP_MAX_FRAME=300 \
  -DIMM_METAL_CONTENT_SWEEP_MIN_PASSED=1 \
  -DIMM_METAL_CONTENT_SWEEP_FAIL_ON_FAILED=0
cmake --build build/macos --target validateAppImmViewerMetalContentSweep --config Release
```

When `IMM_METAL_CONTENT_SWEEP_PATH` is configured, this bounded sweep is included in the aggregate `validateAppImmViewerMetal` target and registered as the `appImmViewerMetalContentSweep` CTest. The sweep requires at least `IMM_METAL_CONTENT_SWEEP_MIN_PASSED` nonblank passed renders, classifies blank-at-camera files as `blank`, and can be made strict with `IMM_METAL_CONTENT_SWEEP_FAIL_ON_FAILED=1`. Set the max-files or max-bytes values to `0` only when an unbounded local sweep is intentional.

The summary table is written to:

```text
build/macos/metal-content-sweep.tsv
```

Logs and PNG captures are written under:

```text
build/macos/metal-content-sweep-logs/
build/macos/metal-content-sweep-captures/
```

These local files are valuable additional paint/scene coverage, especially `Wip10nq-demo.imm` and `Snoopy.imm`. `Snoopy.imm` also exercises authored WAV audio through the optional `validateAppImmViewerMetalAuthoredAudio` and `validateAppImmViewerMetalAuthoredLongAudio` targets. The local Quill sweep still does not exercise 2D picture or 360 cubemap content. These files should not be made mandatory CI inputs unless the assets are added to the repository or otherwise made available to the CI runner.

### Large Local IMM Cache Sweep

A much larger local cache exists at:

```text
/Volumes/andy-desktoppc-1/Imm
```

It is multi-GB and should not be recursively rendered without bounds. The cache was indexed by metadata only, producing:

```text
build/macos/large-imm-cache-index.tsv
```

Current index size:

```text
2443 .imm files
```

The local content sweep script now supports bounded runs for this cache:

```sh
IMM_METAL_SWEEP_MAX_FILES=100 \
IMM_METAL_SWEEP_MAX_BYTES=20000000 \
IMM_METAL_SWEEP_OUTPUT=build/macos/large-cache-100-under20mb-sweep.tsv \
IMM_METAL_SWEEP_LOG_DIR=build/macos/large-cache-100-under20mb-sweep-logs \
  code/appImmViewer/scripts/validate_metal_content_sweep.sh \
  build/macos/viewer/appImmViewerMetal.app/Contents/MacOS/appImmViewerMetal \
  /Volumes/andy-desktoppc-1/Imm
```

For targeted size-ranked sampling from the index:

```sh
awk -F '\t' '$1 <= 20000000 {print $2}' build/macos/large-imm-cache-index.tsv |
  head -100 |
  tr '\n' '\0' |
  xargs -0 env \
    IMM_METAL_SWEEP_OUTPUT=build/macos/large-cache-100-under20mb-sweep.tsv \
    IMM_METAL_SWEEP_LOG_DIR=build/macos/large-cache-100-under20mb-sweep-logs \
    IMM_METAL_SWEEP_MAX_FRAME=180 \
    code/appImmViewer/scripts/validate_metal_content_sweep.sh \
    build/macos/viewer/appImmViewerMetal.app/Contents/MacOS/appImmViewerMetal
```

The first 100-file under-20-MB sample produced:

| Result | Count | Notes |
| --- | ---: | --- |
| Nonblank passed renders | 81 | `drawCalls=734` total, `triangles=26061700` total across passed files |
| Blank at validation camera/time | 19 | `nonZero=0`, `drawCalls=0`; treated as classification results, not renderer crashes |
| Authored 360 picture coverage | 1 | `3990504411070340_MELANCHOLY2021_by_CRYHARDSTUDIOS.imm`: `pictureDrawCalls=1`, `picture360DrawCalls=1` |
| Authored 2D picture coverage | 1 | `896098972954263_Suzanne_by_Starmen tv.imm`: `pictureDrawCalls=1`, `picture2DDrawCalls=1` |

A later focused scan for files containing `Quill.Picture` rendered nine additional cache candidates before one large candidate was terminated for taking too long. Summary path:

```text
build/macos/picture-candidate-sweep.tsv
```

That bounded sweep produced:

| Result | Count | Notes |
| --- | ---: | --- |
| Passed picture-candidate renders | 9 | All nine completed files rendered nonblank. |
| Authored 2D picture coverage | 3 | `235816487842980_Angel_by_Kurt Chang Art.imm` had three 2D picture draw calls; `2679587895593121_Zombie Boys Mysteries...imm` and `2684523675143724_The Circus_by_AyakoArtworks.imm` each mixed 2D and 360 equirect picture draw calls. |
| Authored 360 equirect coverage | 7 | Completed files with `picture360DrawCalls>0` all reported `picture360EquirectDrawCalls>0`. |
| Authored 360 cubemap coverage | 0 | No completed candidate reported `picture360CubemapDrawCalls>0`; authored cubemap validation is deferred unless a real cubemap IMM appears. |
| Terminated candidate | 1 | `3133557443432291_Monster Museum_by_MattSchaeferDesign.imm` did not finish promptly during the bounded pass and was terminated rather than left running. |

An importer-backed metadata scanner now exists so large caches can be searched for picture layer types before launching Metal rendering:

```sh
cmake --build build/macos --target ImmPictureScan --config Release
cmake -S code/projects/macos -B build/macos \
  -DIMM_METAL_PICTURE_SCAN_PATH=/Volumes/andy-desktoppc-1/Imm \
  -DIMM_METAL_PICTURE_SCAN_MAX_FILES=120 \
  -DIMM_METAL_PICTURE_SCAN_SKIP_FILES=0 \
  -DIMM_METAL_PICTURE_SCAN_MAX_SIZE_MB=80 \
  -DIMM_METAL_PICTURE_SCAN_TIMEOUT_SEC=8 \
  -DIMM_METAL_PICTURE_SCAN_NAME_REGEX='(cube|cubemap|skybox|sky|360)' \
  -DIMM_METAL_PICTURE_SCAN_MIN_CUBEMAP_FILES=0
cmake --build build/macos --target scanImmPictureLayers --config Release
```

The underlying script can also be run directly:

```sh
code/appImmViewer/scripts/scan_imm_picture_layers.sh \
  --max-files 120 \
  --skip-files 0 \
  --max-size-mb 80 \
  --per-file-timeout-sec 8 \
  --name-regex '(cube|cubemap|skybox|sky|360)' \
  --min-cubemap-files 0 \
  --output build/macos/imm-picture-layers-desktoppc-bounded.tsv \
  /Volumes/andy-desktoppc-1/Imm
```

The scanner uses the IMM importer `IStrokeCollector::OnPictureLayer` callback and reports `image2D`, `equirect360`, `cubemap360`, `cubemapCross`, and `cubemapVstrip` counts per file. Omit the name regex for blind chunks; include it for targeted filename-hint scans. Recursive scans sort matching `.imm`/`.IMM` paths before applying skip/max limits, so chunked passes are deterministic and match the extension behavior used by the content sweep.

Completed desktop-cache metadata scan evidence so far:

| Scan set | Files scanned | Picture files | 2D-picture files | Equirect-360 files | Cubemap files | Timeouts | Notes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Early partial bounded pass | 65 | 24 | not summarized | not summarized | 0 | 1 | Manually stopped; one row was left as `scan_failed`. |
| Later stopped 300-file pass | about 50 | 17 | not summarized | not summarized | 0 | not summarized | Stopped because arbitrary first-N importer scanning was too slow for this cache. |
| Completed initial chunks, skips 0-324 | 325 | 94 | 49 | 62 | 0 | 11 | Three completed chunks, including the CMake `scanImmPictureLayers` path. |
| Sorted chunks, skips 325-1374 | 1,050 | 303 | 162 | 205 | 0 | 48 | Fourteen deterministic 75-file chunks over the large desktop cache. |
| Sorted chunk, skip 1375 | 75 | 14 | 7 | 9 | 0 | 4 | Written to `build/macos/imm-picture-layers-desktop-cache-sorted-1376-1450.tsv`. |
| Sorted chunk, skip 1450 | 75 | 19 | 8 | 12 | 0 | 2 | Latest deterministic chunk, written to `build/macos/imm-picture-layers-desktop-cache-sorted-1451-1525.tsv`. |
| Bounded filename-hint scan | 45 | 18 | 12 | 10 | 0 | 5 | `--name-regex '(cube|cubemap|skybox|sky|360)'` under the 120-MB bound. |
| Tracked TSV aggregate | 1,570 | 448 | 238 | 289 | 0 | 66 | `scan_imm_picture_layers.sh --summarize-tsv` across the tracked completed TSVs. |

A separate unbounded filename-hinted probe over 48 files matching `cube`, `cubemap`, `skybox`, `sky`, or `360` found 26 picture files, 19 files with 2D pictures, 14 files with equirect 360 pictures, and zero cubemap candidates. This is useful candidate-discovery evidence, not proof that the full multi-GB cache contains no cubemap content.

The scanner summary treats any nonzero `cubemap360`, `cubemapCross`, or `cubemapVstrip` column as cubemap evidence. Recursive scans sort matching `.imm`/`.IMM` paths before applying skip/max limits, so use `--skip-files` to scan later deterministic chunks of a large cache without rescanning the first matching files. Set `--min-cubemap-files 1` or `IMM_METAL_PICTURE_SCAN_MIN_CUBEMAP_FILES=1` when a scan is expected to contain authored cubemap content and should fail otherwise. `IMM_METAL_PICTURE_SCAN_MAX_FILES=0` means unbounded, so keep a positive limit for multi-GB private caches unless a full crawl is intentional.
Use `code/appImmViewer/scripts/scan_imm_picture_layers.sh --summarize-tsv path/to/*.tsv` to summarize accumulated scan outputs without rescanning content.

PNG captures for the authored picture candidates were generated at:

```text
build/macos/large-cache-picture-candidates-captures/001-3990504411070340_MELANCHOLY2021_by_CRYHARDSTUDIOS.imm.png
build/macos/large-cache-picture-candidates-captures/002-896098972954263_Suzanne_by_Starmen_tv.imm.png
```

The focused optional authored 2D picture validation can be run locally with:

```sh
cmake -S code/projects/macos -B build/macos \
  -DIMM_BUILD_VIEWER=ON \
  -DIMM_METAL_AUTHORED_2D_PICTURE_PATH="/Volumes/andy-desktoppc-1/Imm/896098972954263_Suzanne_by_Starmen tv.imm"
cmake --build build/macos --target validateAppImmViewerMetalAuthoredPicture2D --config Release
```

That target requires `picture2DDrawCalls>=1` and writes PNG captures under `build/macos/authored-picture2d-captures`.

The focused optional authored 360 equirect picture validation can be run locally with:

```sh
cmake -S code/projects/macos -B build/macos \
  -DIMM_BUILD_VIEWER=ON \
  -DIMM_METAL_AUTHORED_360_PICTURE_PATH="/Volumes/andy-desktoppc-1/Imm/3990504411070340_MELANCHOLY2021_by_CRYHARDSTUDIOS.imm"
cmake --build build/macos --target validateAppImmViewerMetalAuthoredPicture360 --config Release
```

That target requires `picture360DrawCalls>=1` and writes PNG captures under `build/macos/authored-picture360-captures`.

The filename-targeted probe of likely model files is no longer a standalone readiness blocker. Those files were useful mainly because they confirmed more paint/picture-authored content, not because model layers are part of the current goal.

### Paint Correctness Priority and Baseline

Correct paint-layer rendering is now the highest-priority standalone milestone. The current Metal validation is useful as a regression gate, but it is not a final correctness proof because the static and pretessellated Metal paths can share the same renderer, coordinate, blend, depth, color-space, or resource-binding mistake.

The acceptance standard for paint correctness is:

1. Produce a reference capture from a known-correct non-Metal build for `sample1.imm`, preferably the existing Windows DirectX or other already-trusted non-Metal renderer.
2. Capture the standalone Metal static path from the same camera, viewport, render technique settings, frame, and content file.
3. Compare Metal static against the non-Metal reference with a documented tolerance and inspect the capture manually for stroke placement, opacity, color, depth ordering against the 360 backdrop, and culling.
4. Only after static matches the reference, compare Metal pretessellated against the same reference and then against Metal static to catch tessellation-specific differences.
5. Keep the reference capture and comparison command/script in the repo or document the exact external artifact path so future Metal changes have a stable baseline.

Local macOS OpenGL is not currently that baseline. On this Apple Silicon machine running macOS 15.3, `appImmViewer` creates an OpenGL context reported as `OpenGL 4.1 Metal - 89.3` with `GLSL 4.10`, but the existing paint shader fails before viewer initialization:

```text
IMM OpenGL shader vertex compile failed: VS:WARNING: 0:9: extension 'GL_ARB_shader_draw_parameters' is not supported
ERROR: 0:85: 'buffer' : syntax error: syntax error
```

That failure is expected for this shader stack because Apple OpenGL is capped at 4.1 and does not provide the newer extension/SSBO assumptions used by the current desktop GL paint renderer. Porting the GL paint shaders down to Apple GL 4.1 would be a separate compatibility project and could change the behavior we are trying to use as ground truth. Until proven otherwise, use macOS OpenGL only as a compile/diagnostic path, not as the authoritative visual baseline.

The legacy viewer now has an environment-driven GL validation/readback harness that fails with exit code `2` when validation is requested and viewer initialization fails. This prevents the local GL baseline attempt from being mistaken for a successful reference capture.

The current baseline-image workflow is intentional: CI is now the preferred source of the Windows DirectX reference PNG, and manual Windows execution is a fallback. This changed the earlier manual-first approach so the reference image can be produced by the same Windows build that creates the downloadable viewer artifact.

The same shared viewer validation hook can still be used from the Windows DirectX player to create the non-Metal baseline capture manually. After building or downloading the Windows `ImmViewer-Windows` artifact, run this from the repository root in PowerShell:

```powershell
.\code\appImmViewer\scripts\capture_windows_directx_baseline.ps1
```

The script resolves `code\appImmViewer\exe\appImmViewer_Release.exe`, `code\appImmViewer\exe\settings-baseline-directx-sample1.json`, and `exampleImmFiles\sample1.imm`, creates a temporary runtime settings JSON with `File.Load` pointing at the resolved sample, then writes:

```text
build\baseline-captures\windows-directx-static.png
```

The Windows CI job runs this same script after the Release build and uploads the result as the `ImmViewer-Windows-DirectX-Baseline` artifact. That artifact contains `windows-directx-static.png` plus `settings-baseline-directx-sample1.runtime.json`, which records the exact resolved sample path used by the capture run. Inspect this PNG before relying on numeric comparison results or setting pass/fail thresholds.

The downloaded `ImmViewer-Windows` artifact is self-contained for this baseline capture. It includes the viewer executable, DirectX baseline settings, the capture script, dependent DLLs, and `sample1.imm`. From that folder, run:

```powershell
.\capture_windows_directx_baseline.ps1
```

Equivalent manual command:

```powershell
New-Item -ItemType Directory -Force build\baseline-captures | Out-Null
$settings = Get-Content code\appImmViewer\exe\settings-baseline-directx-sample1.json -Raw | ConvertFrom-Json
$settings.File.Load = @("$PWD\exampleImmFiles\sample1.imm")
$runtimeSettings = "$PWD\build\baseline-captures\settings-baseline-directx-sample1.runtime.json"
$settings | ConvertTo-Json -Depth 16 | Set-Content $runtimeSettings -Encoding UTF8

$env:IMM_VIEWER_VALIDATE_FRAME = "1"
$env:IMM_VIEWER_VALIDATE_MAX_FRAME = "240"
$env:IMM_VIEWER_VALIDATE_MIN_NONZERO = "16"
$env:IMM_VIEWER_VALIDATE_MIN_DRAWCALLS = "1"
$env:IMM_VIEWER_VALIDATE_MIN_PICTURE_DRAWCALLS = "1"
$env:IMM_VIEWER_VALIDATE_MIN_PICTURE360_DRAWCALLS = "1"
$env:IMM_VIEWER_VALIDATE_MIN_TRIANGLES = "1"
$env:IMM_VIEWER_VALIDATE_CAPTURE_PATH = "$PWD\build\baseline-captures\windows-directx-static.png"
code\appImmViewer\exe\appImmViewer_Release.exe $runtimeSettings
```

The capture hook writes PNG when the capture path ends in `.png`, and still supports `.ppm` for raw Metal-style inspection captures. If running from a custom Windows build layout instead of the repo checkout or CI artifact, pass `-ViewerExe`, `-SettingsPath`, `-SamplePath`, or `-OutputPath` explicitly to `capture_windows_directx_baseline.ps1`.

Once the Windows baseline PNG and Metal standalone PNG exist, compare them with:

```sh
python3 code/appImmViewer/scripts/compare_captures.py \
  build/baseline-captures/windows-directx-static.png \
  build/macos/metal-validation-captures/static.png \
  --json-output build/macos/reference-comparison/windows-directx-vs-metal-static.json \
  --diff-output build/macos/reference-comparison/windows-directx-vs-metal-static-diff.png \
  --contact-sheet-output build/macos/reference-comparison/windows-directx-vs-metal-static-contact-sheet.png \
  --diff-scale 8
```

The comparison script supports `.png` and `.ppm`, reports mean absolute channel difference, RMS channel difference, maximum channel difference, and percentage of differing pixels, can write machine-readable metrics with `--json-output`, can write an amplified absolute RGB difference PNG with `--diff-output`, and can write a reference/candidate/diff contact sheet with `--contact-sheet-output`. It can also be given explicit failure thresholds with `--max-mean-abs`, `--max-rms`, `--max-channel-diff`, and `--max-differing-percent`.

The macOS CMake project now exposes the same workflow as an optional validation target. Configure a known-good reference capture plus any accepted thresholds:

```sh
cmake -S code/projects/macos -B build/macos \
  -DIMM_BUILD_VIEWER=ON \
  -DIMM_METAL_REFERENCE_CAPTURE_PATH=/path/to/windows-directx-static.png \
  -DIMM_METAL_REFERENCE_MAX_MEAN_ABS=10 \
  -DIMM_METAL_REFERENCE_MAX_RMS=25 \
  -DIMM_METAL_REFERENCE_MAX_CHANNEL_DIFF=255 \
  -DIMM_METAL_REFERENCE_MAX_DIFFERING_PERCENT=100
cmake --build build/macos --target validateAppImmViewerMetalReferenceCompare --config Release
```

When configured, the target generates a fresh Metal static PNG at:

```text
build/macos/reference-comparison/static.png
```

then compares it against `IMM_METAL_REFERENCE_CAPTURE_PATH`, writes:

```text
build/macos/reference-comparison/reference-vs-metal-static.json
build/macos/reference-comparison/reference-vs-metal-static-diff.png
build/macos/reference-comparison/reference-vs-metal-static-contact-sheet.png
```

and fails the build if any configured threshold is exceeded. The diff PNG and contact sheet are intended for visual review before thresholds are promoted. If `IMM_METAL_REFERENCE_CAPTURE_PATH` is unset, the target is a no-op that prints the configure hint. The aggregate `validateAppImmViewerMetal` target includes this reference comparison only when a reference path is configured, so normal local and CI validation do not depend on unavailable private or downloaded artifacts.

Latest local reference-compare run on 2026-05-25 used `build/baseline-captures/windows-directx-static.png` as the configured reference and passed with the documented loose regression thresholds: `maxMeanAbs=10`, `maxRms=25`, `maxChannelDiff=255`, and `maxDifferingPercent=100`. It generated a fresh Metal capture plus JSON, diff, and contact-sheet outputs under `build/macos/reference-comparison/`. The measured static-frame comparison was `meanAbs=1.984224`, `rms=3.548679`, `maxChannelDiff=167`, and `differingPixels=696948/921600 (75.623698%)`. These thresholds are intentionally broad and catch gross regressions against the Windows DirectX scene composition; tighter visual-equivalence thresholds should follow review of the contact sheet and diff.

The current local comparison between `build/baseline-captures/windows-directx-static.png` and a fresh validation capture at `build/macos/metal-validation-captures/static.png` writes:

```text
build/macos/reference-comparison/windows-directx-vs-metal-static.json
build/macos/reference-comparison/windows-directx-vs-metal-static-diff.png
```

with these metrics:

```text
meanAbs=1.984359 rms=3.549581 maxChannelDiff=167 differingPixels=696946/921600 (75.623481%)
```

These numbers should be treated as recorded evidence for inspection and threshold-setting, not as an acceptance threshold by themselves.

The same fresh validation run also compared Metal static against Metal pretessellated:

```sh
python3 code/appImmViewer/scripts/compare_captures.py \
  build/macos/metal-validation-captures/static.png \
  build/macos/metal-validation-captures/pretessellated.png \
  --json-output build/macos/reference-comparison/metal-static-vs-pretessellated.json \
  --diff-output build/macos/reference-comparison/metal-static-vs-pretessellated-diff.png \
  --diff-scale 8
```

with these metrics:

```text
meanAbs=0.000386 rms=0.116487 maxChannelDiff=73 differingPixels=55/921600 (0.005968%)
```

That is useful evidence that the two Metal paint paths now agree closely for `sample1.imm`, but it is not a substitute for the Windows DirectX reference because shared Metal state can still hide common-mode errors.

The macOS CI job now builds `appImmViewerMetal`, runs the standalone Metal CTest validation suite with deterministic expected-value checks enabled, writes a static Metal PNG capture through `validate_metal_standalone.sh` from the same validation readback path, and uploads one standalone artifact:

- `ImmViewerMetal-macOS`, containing `appImmViewerMetal.app` plus `metal-baseline-captures/metal-static.png` and its validation logs.

Once both platform jobs have run, compare the downloaded CI captures directly:

```sh
python3 code/appImmViewer/scripts/compare_captures.py \
  path/to/ImmViewer-Windows-DirectX-Baseline/windows-directx-static.png \
  path/to/ImmViewerMetal-macOS/metal-baseline-captures/metal-static.png \
  --json-output build/macos/reference-comparison/ci-windows-directx-vs-metal-static.json \
  --diff-output build/macos/reference-comparison/ci-windows-directx-vs-metal-static-diff.png \
  --contact-sheet-output build/macos/reference-comparison/ci-windows-directx-vs-metal-static-contact-sheet.png \
  --diff-scale 8
```

Do not promote the image comparison to a pass/fail CI gate until the Windows DirectX reference has been visually inspected and an acceptable image-difference threshold has been chosen.

## Phase 0 - Baseline and Constraints

### Objectives

Establish reproducible builds and runtime baselines before changing renderer code.

### Tasks

1. Confirm current macOS native builds:
   - `cmake -S code/projects/macos -B build/macos -DIMM_BUILD_VIEWER=ON`
   - `cmake --build build/macos --target ImmStrokeReader --config Release`
   - `cmake --build build/macos --target ImmUnity --config Release`
   - `cmake --build build/macos --target appImmViewer --config Release`

2. Confirm whether `appImmViewer` currently compiles and runs on macOS with the OpenGL backend.

3. Confirm whether `ImmUnity` currently compiles on macOS with OpenGLCore.

4. Capture Unity's current Metal failure mode:
   - Managed error from `ImmPlayerManager.Initialize()`.
   - Native renderer selection path in `Init()`.
   - Plugin load or bundle load errors.

5. Record target platform decisions:
   - Minimum macOS version.
   - Minimum Unity version.
   - Apple Silicon only vs universal `arm64 + x86_64`.
   - Required first-pass content types: paint, pictures, sound, animation.

### Deliverables

- Known-good baseline build commands.
- A sample `.imm` file and validation scene/content list.
- Notes on whether the existing OpenGL macOS viewer is useful as a visual reference.

### Risks

- `appImmViewer` may need unrelated macOS cleanup before it can serve as a test harness.
- `ImmUnity` may not currently compile or load on macOS even before Metal work.

## Phase 1 - Shared Renderer API Plumbing

### Objectives

Add Metal as a first-class renderer API and compile a stub backend shared by both hosts.

### Tasks

1. Extend `piRenderer::API` in `code/libImmCore/src/libRender/piRenderer.h`:
   - Add `Metal`.
   - Audit `static_cast<int>(api)` usages and API-name arrays.

2. Update `piRenderer::Create()` in `code/libImmCore/src/libRender/piRenderer.cpp`:
   - Include a Metal renderer header on Apple platforms.
   - Return `new piRendererMetal()` when `type == API::Metal`.

3. Add initial Metal renderer files:
   - `code/libImmCore/src/libRender/metal/piMetal_Renderer.h`
   - `code/libImmCore/src/libRender/metal/piMetal_Renderer.mm`

4. Implement a stub `piRendererMetal` that satisfies the full `piRenderer` interface and logs intentional "not implemented" failures.

5. Update `code/projects/macos/CMakeLists.txt`:
   - Enable Objective-C++ as needed.
   - Add Metal renderer sources to `libImmCore`.
   - Link `Metal`, `MetalKit`, `QuartzCore`, `Cocoa`, and `Foundation`.
   - Keep OpenGL linked only while the OpenGL backend remains in the build.

6. Add a minimal host/context abstraction for Metal ownership:
   - Native host can provide drawable/render pass state.
   - Unity host can later provide Unity-owned render state.
   - Avoid baking Unity assumptions into `piRendererMetal`.

### Deliverables

- `appImmViewer`, `ImmUnity`, and `ImmStrokeReader` compile with a stub Metal backend.
- The codebase can select `piRenderer::API::Metal` without compile errors.

### Risks

- `piRenderer` is broad, so the stub class still needs many methods.
- Objective-C++ may require splitting platform-specific implementation out of existing `.cpp` files.

## Phase 2 - Native Standalone Metal Host

### Objectives

Create a native macOS Metal host for `appImmViewer` so IMM can own a clean Metal frame before dealing with Unity interop. This phase turns the standalone player into the renderer bring-up harness for every later Metal feature.

### Tasks

1. Update the native macOS viewer entrypoint:
   - Start from `code/appImmViewer/src/macos/main.mm`.
   - Introduce an `NSApplication`/`NSWindow` plus `MTKView` or `CAMetalLayer`.
   - Preserve command-line file loading behavior where possible.

2. Decide viewer host shape:
   - Preferred: `MTKView` for resize, drawable, and render loop management.
   - Alternative: `CAMetalLayer` for lower-level control.

3. Add a Metal frame context type that `piRendererMetal` can consume:
   - `id<MTLDevice>`
   - `id<MTLCommandQueue>`
   - current drawable texture
   - depth texture
   - render pass descriptor
   - viewport size and backing scale

4. Update viewer initialization to choose `piRenderer::API::Metal` on macOS.

5. Implement resize handling:
   - Color drawable size.
   - Depth texture recreation.
   - Multisample texture recreation if needed.

6. Add a native smoke render path:
   - Clear to a known color.
   - Draw a simple triangle or quad through `piRendererMetal`.
   - Present drawable.

7. Keep the native viewer path independent from Unity:
   - No Unity headers.
   - No Unity render-event assumptions.
   - No Unity-owned command buffer assumptions.

### Deliverables

- `cmake --build build/macos --target appImmViewer --config Release` produces a native Metal-capable viewer, or a temporary `appImmViewerMetal` target exists while the legacy viewer entrypoint is preserved.
- Running the standalone viewer with `exampleImmFiles/sample1.imm` opens a Metal-backed window.
- Before IMM playback works, the viewer can at least clear and draw test geometry.
- Unity work remains limited to keeping the existing plugin buildable; no Unity Metal device or render-event integration is started yet.

### Risks

- Existing viewer code may assume the old platform/window abstraction.
- Input, camera, and event-loop behavior may need enough repair to validate playback.

## Phase 3 - Metal Renderer Core

### Objectives

Implement enough `piRendererMetal` to create resources, set state, and issue basic draw calls in the native standalone viewer. Treat the standalone player as the acceptance environment for the shared renderer API.

### Tasks

1. Define internal Metal handle structs for all opaque `piRenderer` types:
   - `piShader`
   - `piTexture`
   - `piVertexArray`
   - `piRTarget`
   - `piSampler`
   - `piBuffer`
   - `piRasterState`
   - `piBlendState`
   - `piDepthState`
   - `piQuery`

2. Implement format mapping:
   - `piRenderer::Format` to `MTLPixelFormat`.
   - `piRArrayDataType` / `ArrayLayout2` to `MTLVertexFormat`.
   - `IndexArrayFormat` to `MTLIndexType`.
   - `PrimitiveType` to `MTLPrimitiveType`.

3. Implement buffers:
   - Vertex buffers.
   - Index buffers.
   - Constant buffers.
   - Structured buffers where used by player shaders.
   - Dynamic update paths for `UpdateBuffer()`.

4. Implement textures:
   - 2D textures.
   - 2D array textures.
   - Cube textures if required by picture layers.
   - Mipmap generation.
   - Texture updates from CPU data.
   - Sampler state creation and binding.

5. Implement render state:
   - Viewports.
   - Depth test/write.
   - Culling and front-face winding.
   - Blending.
   - Color/depth write masks.
   - Multisample state.

6. Implement draw paths:
   - `DrawPrimitiveIndexed`.
   - `DrawPrimitiveNotIndexed`.
   - Instanced draws.
   - Defer indirect draws until a scene requires them, unless runtime checks show they are used.

7. Implement render-target behavior:
   - Native default framebuffer from `MTKView`/`CAMetalLayer`.
   - Optional offscreen render targets if IMM uses them for intermediate passes.
   - Blit path if required.

8. Implement performance/query methods:
   - Start with no-op timing if needed.
   - Add Metal timestamp support only after correctness is stable.

### Deliverables

- Native `appImmViewer` can clear and draw through `piRendererMetal`.
- Resource creation/destruction works across repeated window open/close and content reload.
- The renderer API surface needed by the standalone player is implemented without Unity-specific assumptions.

### Risks

- The `piRenderer` interface has GL/DX-shaped calls that need Metal-side state caching.
- Metal pipeline state creation requires shader, render-target format, blend/depth state, and vertex layout information together.

## Phase 4 - Shader Strategy and Native Playback

### Objectives

Provide Metal shaders and validate real IMM playback in the native standalone viewer. This is the main readiness gate before Unity integration.

### Current Shader Sources

Layer renderers currently have GLSL, GLES GLSL, and HLSL shader files:

- `code/libImmPlayer/src/layerRenderers/layerRendererPaint/static/*`
- `code/libImmPlayer/src/layerRenderers/layerRendererPaint/pretessellated/*`
- `code/libImmPlayer/src/layerRenderers/layerRendererPicture/*`

### Recommended Strategy

Start with manual MSL for the minimum active renderer path, then decide whether a shader cross-compilation toolchain is worth adding.

The first useful native playback target is a functional standalone viewer, not a silent render harness and not a visual-only renderer demo. Its required surface is:

- Static paint layers.
- Pretessellated paint.
- Picture layers needed by representative content.
- Native audio decode/playback for embedded IMM audio, including failure on silent fallback and proof that playback actually advances.
- Then optional/debug paths.

### Tasks

1. Inventory which shaders are required by `appImmViewer` and by the Unity default:
   - `Drawing::PaintRenderingTechnique::Static`
   - Picture layers.
   - Overdraw/debug shaders if needed.

2. Create Metal shader files beside existing shaders:
   - `*.metal`, or
   - generated `*_msl.inc` files if following the existing include-generation pattern.

3. Define explicit resource binding conventions:
   - Vertex buffers.
   - Constant buffers.
   - Textures.
   - Samplers.
   - Structured buffers.

4. Adapt `LayerRenderer*` code if needed:
   - Avoid GL-only uniform-location paths.
   - Prefer explicit constant-buffer binding for Metal.
   - Avoid runtime shader string compilation if precompiled Metal libraries are used.

5. Add CMake shader build steps:
   - Compile `.metal` into a default Metal library, or
   - Embed Metal source and compile at runtime during development.

6. Validate shader math in native viewer:
   - Clip-space depth should be zero-to-one.
   - Projection matrices should match Metal conventions.
   - Front-face winding should be correct.
   - Linear/gamma color should match existing playback as closely as possible.

### Deliverables

- `appImmViewer` renders `exampleImmFiles/sample1.imm` with Metal.
- Native playback validates transforms, depth, culling, color, and transparency.
- Native playback decodes and plays embedded IMM audio through AVFoundation, with validation that fails on silent fallback or missing Ogg Opus support.
- Metal shader errors are reported through IMM logs.
- A short list of renderer features still missing for Unity is documented. Unity integration should not start until the missing items are either implemented or explicitly judged non-blocking for the first Unity milestone.

### Risks

- Shader binding assumptions are likely the largest non-host-specific risk.
- Existing player code has GL/DX conditionals for depth, clip space, viewport flip, and blue-noise resource handling; Metal needs explicit behavior.

## Phase 5 - Player Configuration and Coordinate System

### Objectives

Make IMM's player configuration correct for Metal in the native player first, then carry the validated conventions into Unity.

### Tasks

1. Audit `Player::Init()` and render paths:
   - `code/libImmPlayer/src/player.cpp`
   - Find `renderer->GetAPI() == GL`, `GLES`, and `DX` conditionals.
   - Add Metal behavior explicitly.

2. Define Metal values for:
   - `DepthBuffer`
   - `ClipSpaceDepth`
   - projection matrix convention
   - front-face winding

3. Apply those values in native `appImmViewer` first and verify them with real playback.

4. After native validation, apply equivalent values in Unity `Init()`:
   - `code/appImmUnity/src/main.cpp`

5. Audit matrix conversion:
   - Native viewer camera matrices.
   - Unity `iUnityToPilibs()`.
   - Unity `iUnityToTrans3d()`.
   - Managed `GL.GetGPUProjectionMatrix()` calls in `ImmPlayerManager.cs`.

6. Validate:
   - Mono camera in native viewer.
   - Mono camera in Unity.
   - Stereo/two-pass only after mono is correct.
   - Single-pass only if the target Unity/macOS XR path requires it.

### Deliverables

- Native Metal playback has correct position, orientation, depth, and winding.
- Unity uses the same renderer conventions with only host-specific matrix adaptation.
- Any difference between native Metal and Unity Metal projection behavior is isolated to the Unity host adapter.

### Risks

- Native and Unity projection conventions may not be identical.
- Existing comments about DX-to-GL conversion must be revisited for Metal rather than copied forward.

## Phase 6 - Unity Metal Integration

### Objectives

Integrate the already-proven `piRendererMetal` backend into Unity's native plugin lifecycle. This phase starts only after the standalone player can render representative IMM content with Metal.

Current status: started for macOS. The native plugin now compiles with a first Metal adapter that acquires Unity's Metal device and attempts to render through Unity's current command buffer/encoder. The next proof point is not another standalone validation pass; it is opening the Unity sample project on macOS with Metal selected and confirming that `Init()`, render events, and a rendered IMM frame all occur inside Unity.

### Tasks

0. Confirm the standalone readiness gate:
   - The native standalone player opens a Metal window.
   - Static paint playback works.
   - Pretessellated paint playback works for the same representative content.
   - Native audio playback works for embedded IMM audio on at least one representative file.
   - Audio validation passes as a required standalone gate, including decode, accepted play calls, observed playing state, playback progress, and clean teardown.
   - At least one representative IMM file renders without Unity.
   - Model-layer playback is not part of the current standalone readiness gate.
   - Renderer feature gaps are known and either fixed or accepted for the first Unity milestone.

1. Add Unity's Metal graphics interface header:
   - Prefer the official `IUnityGraphicsMetal.h` matching the Unity native plugin API version already used by this repo.
   - Place it beside existing Unity headers in `code/appImmUnity/src/` if needed.
   - Current implementation uses an opaque-pointer declaration in `code/appImmUnity/src/IUnityGraphicsMetal.h` to avoid Objective-C framework imports colliding with IMM importer symbols. Verify the GUIDs/API shape against the target Unity version before treating runtime loading as proven.

2. Update `iOnGraphicsDeviceEvent()` in `code/appImmUnity/src/main.cpp`:
   - On `kUnityGfxRendererMetal`, acquire `IUnityGraphicsMetal`.
   - Store Unity's Metal device pointer.
   - Store command-buffer/current-render-pass access separately if required by the Unity API version.

3. Define Unity host ownership:
   - Unity owns the `MTLDevice` lifetime.
   - Unity owns command scheduling and the active render target.
   - IMM owns buffers, textures, samplers, pipeline states, and renderer state caches.

4. Create a Unity Metal frame adapter:
   - Converts Unity's render-thread context into the same frame context shape used by the native viewer where possible.
   - Keeps Unity-specific calls out of general `piRendererMetal` logic.

5. Ensure all rendering work happens on Unity's render thread:
   - Continue using `GL.IssuePluginEvent` / `CommandBuffer.IssuePluginEvent`.
   - Rename or generalize GL-specific comments in managed code.
   - Avoid creating or committing command buffers contrary to Unity's plugin API.

6. Define render-target handling:
   - Preferred: render into Unity's current render target through Unity-provided Metal context.
   - Fallback: render into a Unity-owned texture and composite through Unity.

7. Update managed macOS guard in `ImmPlayerManager.cs`:
   - Accept `GraphicsDeviceType.Metal`.
   - Optionally allow OpenGLCore only behind a legacy flag.
   - Update error messaging.

8. Update `Init()` in `code/appImmUnity/src/main.cpp`:
   - Select `piRenderer::API::Metal` for `kUnityGfxRendererMetal`.
   - Use Metal-specific player configuration validated in native viewer.

9. Validate command-encoder ownership in Unity:
   - Confirm whether drawing into `CurrentCommandEncoder()` is accepted for the target Unity version.
   - If Unity requires plugins to end the current encoder before custom work, switch the adapter to use `EndCurrentCommandEncoder()` plus a renderer-owned encoder over `CurrentRenderPassDescriptor()`.
   - Do not let `piRendererMetal` commit or present Unity-owned command buffers.

10. Validate Unity render-target/depth behavior:
   - Confirm color output appears in Game view and Scene view.
   - Confirm depth/culling matches the standalone player for `sample1.imm`.
   - Decide whether Unity should supply a depth target or whether IMM rendering should be treated as a color-only overlay for the first milestone.

### Deliverables

- Unity loads `ImmUnityPlugin.bundle` with Metal selected.
- Native initialization retrieves Unity's Metal device.
- Unity render event invokes the Metal renderer.
- A simple Metal test draw works in Unity before full IMM playback is enabled.

### Risks

- Unity's Metal plugin API differs by Unity version.
- Command encoder ownership is the central Unity-specific risk. IMM cannot freely end, replace, or commit Unity's encoder unless Unity's API permits that flow.

## Phase 7 - Unity Package and Build Integration

### Objectives

Package the Metal-enabled Unity plugin cleanly for the sample project on macOS first, then iOS.

### Tasks

1. Update `code/projects/macos/CMakeLists.txt`:
   - Build `ImmUnity` with Metal sources.
   - Produce `ImmUnityPlugin.bundle`.
   - Copy the bundle to:
     - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/OSX/ImmUnityPlugin.bundle`

2. Confirm Unity plugin import settings:
   - macOS Editor enabled.
   - macOS Standalone enabled.
   - CPU architecture includes Apple Silicon and Intel as required.

3. Decide binary distribution format:
   - Apple Silicon only.
   - Universal `x86_64;arm64`.
   - Separate architecture builds.

4. Add/update `.meta` files if Unity needs changed plugin settings.

5. Update package docs:
   - `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/README.md`
   - Root `BUILDING.md`
   - Root `README.md` macOS section if needed.

6. Add CMake options:
   - `IMM_ENABLE_METAL=ON`
   - `IMM_ENABLE_OPENGL_LEGACY=OFF` or similar, only if useful.

7. Add iOS native Unity plugin output:
   - Extend `code/projects/ios/CMakeLists.txt` beyond `ImmStrokeReader`.
   - Add an `ImmUnity` static library or framework target that links the importer/player/core/rendering sources required by `code/appImmUnity/src/main.cpp`.
   - Compile Objective-C++ sources and link the iOS Metal/Foundation/UIKit frameworks needed by the renderer/host path.
   - Build for `arm64` device first; simulator support can follow if needed.
   - Current status: complete as a compile, native-link, Unity-generated Xcode, and arm64 simulator runtime gate. The target builds a merged archive at `build/ios/unity/libImmUnityPlugin.a`; `ImmUnityIOSLinkSmoke` force-loads it into an iPhoneOS executable; Unity-generated iPhoneOS and arm64 simulator Xcode projects compile/link locally; the simulator app renders and plays audio for `sample1.imm`.

8. Add iOS Unity package files:
   - Create `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/iOS`.
   - Copy the iOS `ImmUnity` output there from the CMake build.
   - Add Unity `.meta` importer settings that enable the plugin for iOS only and mark it as preloaded if required.
   - Keep the existing managed `__Internal` DllImport path for iOS player builds.
   - Current status: complete for the current gate. The plugin artifact and `.meta` file are present in the package; Unity import/link behavior is proven by generated iPhoneOS and arm64 simulator Xcode builds.

9. Update CI only after the iOS target exists:
   - Build the iOS `ImmUnity` target.
   - Upload the iOS plugin artifact if it is useful for package assembly.
   - Keep renderer-image validation out of CI unless redistributable reference assets and stable acceptance thresholds are agreed.
   - Current status: CI builds and uploads macOS and iOS `ImmUnity` artifacts for UPM package assembly; temporary standalone image validation remains out of CI.

### Deliverables

- One documented command builds the native viewer and Unity Metal plugin.
- The built Unity bundle is copied into the Unity package.
- Unity imports the bundle without manual file moves.
- One documented command builds the iOS Unity plugin artifact.
- The Unity package contains platform-correct macOS and iOS native plugin artifacts.

### Risks

- Bundle signing/quarantine issues on macOS.
- Unity `.meta` platform settings may need careful updates.

## Phase 8 - Validation Matrix

### Objectives

Prove the backend works first in native standalone, then in Unity, and does not regress existing platforms.

### Test Content

Use at least:

- `exampleImmFiles/sample1.imm`
- A Quill/stroke-heavy IMM file.
- An IMM file with picture layers.
- An animated IMM file with sound, if available.

### Native macOS Tests

1. Compile:
   - `cmake -S code/projects/macos -B build/macos -DIMM_BUILD_VIEWER=ON`
   - `cmake --build build/macos --target appImmViewerMetal --config Release`
   - `cmake --build build/macos --target validateAppImmViewerMetal --config Release`
   - `cmake --build build/macos --target appImmViewer --config Release`
   - `cmake --build build/macos --target ImmStrokeReader --config Release`

2. Run:
   - `build/macos/viewer/appImmViewerMetal.app/Contents/MacOS/appImmViewerMetal exampleImmFiles/sample1.imm`
   - `build/macos/viewer/appImmViewer exampleImmFiles/sample1.imm`

3. Automated standalone Metal validation:
   - Run `validateAppImmViewerMetal` before beginning Unity Metal work.
   - Treat failures here as shared renderer or standalone host blockers, not Unity blockers.
   - Expand `code/appImmViewer/scripts/validate_metal_standalone.sh` as new representative IMM files are added.
   - Configure `IMM_METAL_CONTENT_SWEEP_PATH` for bounded local representative-content coverage; when configured, `validateAppImmViewerMetal` and CTest include the sweep automatically.
   - For visual inspection, set `IMM_METAL_VALIDATE_CAPTURE_DIR=build/macos/metal-validation-captures` and optionally `IMM_METAL_VALIDATE_CAPTURE_FORMAT=png`.
   - Keep the resize case in the validation script passing; it exercises render-target destruction/recreation after startup.

4. Visual validation:
   - Camera orientation.
   - Scale.
   - Winding/culling.
   - Transparency.
   - Depth.
   - Linear/gamma color.
   - Resize behavior.

5. Native audio validation:
   - Decode embedded Ogg Opus content in `sample1.imm`.
   - Decode at least one additional authored audio file when available locally.
   - Fail on silent fallback, missing codec support, rejected `Play()` calls, missing `state=playing`, missing playback progress, or audio backend cleanup failure.
   - Keep render-validation captures deterministic by using focused audio tests rather than enabling live audio in every render test.

6. Stability validation:
   - Repeated load/unload.
   - Window resize.
   - App quit.
   - GPU resource cleanup.

### Unity macOS Tests

1. Compile:
   - `cmake --build build/macos --target ImmUnity --config Release`

2. Unity import and player build:
   - `/Applications/Unity/Hub/Editor/2022.3.62f2/Unity.app/Contents/MacOS/Unity -batchmode -quit -projectPath code/ImmUnitySampleProject -logFile build/unity-macos-import.log`
   - `/Applications/Unity/Hub/Editor/2022.3.62f2/Unity.app/Contents/MacOS/Unity -batchmode -quit -projectPath code/ImmUnitySampleProject -executeMethod ImmPlayer.Editor.BuildAutomation.BuildMacOSDevelopment -logFile build/unity-macos-player-build.log`
   - Confirm the generated app contains `Contents/PlugIns/ImmUnityPlugin.bundle` and `Contents/Resources/Data/StreamingAssets/sample1.imm`.

3. Unity standalone macOS runtime smoke:
   - `code/ImmUnitySampleProject/Builds/macOS/IMMUnityTest.app/Contents/MacOS/IMM\ Unity\ Test -logFile build/unity-macos-player-runtime.log`
   - Confirm Unity selects Metal.
   - Confirm the managed package logs `IMM Player Initialized Successfully`.
   - Confirm `sample1.imm` loads.
   - Confirm the native log reaches renderer initialization, GPU load, and `Has 1 chapters`.
   - Confirm the native log reports a Unity Metal render frame with nonzero draw calls, including paint and 360-picture draw calls.
   - Current status: this smoke reaches nonzero IMM render submission locally and reports `drawCalls=38`, `paintDrawCalls=37`, `pictureDrawCalls=1`, `picture360DrawCalls=1`, `triangles=645802` after initial spawn-area setup, the Unity Metal projection-matrix mode fix, the reversed-depth fix, and the external-device 360-backdrop depth fix. The previous normal-scene crash was isolated to layer metadata refresh and fixed by changing the Unity native layer-info ABI from internal macOS `wchar_t` strings to fixed UTF-16 strings for C# marshalling.
   - Current default-path capture evidence is `build/unity-smoke/macos-current-proof.png`, which visibly contains the `sample1.imm` branch, character, and 360 backdrop.
   - Additional isolation: playback-state-only and spawn-area-only probes were clean with layer refresh disabled. Layer-refresh-only crashed before the ABI fix and exits cleanly after it.

4. Unity standalone macOS audio smoke:
   - Launch the macOS Unity player with `sample1.imm` and inspect `code/ImmUnitySampleProject/Builds/macOS/imm_player_log.txt`.
   - Confirm the plugin selected AVFoundation, not the null backend.
   - Confirm the sample's three embedded Ogg Opus sounds decode, receive accepted AVFoundation `Play()` calls, enter `state=playing`, report playback progress, and clean up temp files on teardown.
   - Current status: complete locally on 2026-05-25 after the Unity plugin sound backend switch. The latest smoke reports three Ogg Opus decodes, three accepted play calls, three playing-state transitions, three progress markers, and clean teardown with `soundsDestroyed=3`, `tempFilesRemoved=3`, `tempFileRemoveFailures=0`.

5. Unity framebuffer capture:
   - `IMM_UNITY_SMOKE_CAPTURE_PATH=/absolute/path/to/macos-sample1.png IMM_UNITY_SMOKE_FRAMES=240 IMM_UNITY_SMOKE_QUIT=1 code/ImmUnitySampleProject/Builds/macOS/IMMUnityTest.app/Contents/MacOS/IMM\ Unity\ Test -logFile build/unity-macos-player-runtime.log`
   - `Assets/Scripts/ImmUnityRuntimeSmoke.cs` installs this opt-in capture hook only when `IMM_UNITY_SMOKE_CAPTURE_PATH` is present.
   - Current status: the hook writes PNGs for real IMM submission during normal startup. The latest default-path capture, `build/unity-smoke/macos-current-proof.png`, is visual playback proof for the macOS Unity standalone sample gate.

6. Unity Editor:
   - Open sample project.
   - Confirm Metal is selected.
   - Load sample IMM.
   - Enter/exit play mode.
   - Load/unload documents.
   - Current status: batchmode Editor play-mode lifecycle smoke passes locally. It proves load/audio/GPU-load/unload cleanup in the Editor path. It does not prove Editor framebuffer visual output; visual output is proven by the macOS Unity standalone player capture.

7. Unity standalone macOS visual validation:
   - Run on Apple Silicon.
   - Confirm the Unity framebuffer visibly renders the same `sample1.imm` branch, character, and 360 backdrop as the standalone Metal player.
   - Run on Intel if universal support is required.

8. Unity-specific validation:
   - Camera matrix match.
   - Render order.
   - Command buffer path.
   - Domain reload behavior.
   - Device shutdown/reinitialize callbacks.

### Unity iOS Tests

1. Compile:
   - `cmake -S code/projects/ios -B build/ios -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64`
   - `cmake --build build/ios --target ImmUnity --config Release`

2. Unity build:
   - Open the sample project.
   - Switch target platform to iOS.
   - Confirm Metal is selected.
   - Build an Xcode project.
   - Confirm the generated Xcode project links the iOS `ImmUnity` native library/framework without manual linker edits.
   - Current local status: `ImmPlayer.Editor.BuildAutomation.BuildIOSDevelopment` passes and exports `Builds/iOS/Unity-iPhone.xcodeproj`. The generated Xcode project includes `Libraries/com.immersive-foundation.imm-unity/Plugins/iOS/libImmUnityPlugin.a` in the Frameworks phase.
   - Follow-up local status: unsigned `xcodebuild` of the generated project now succeeds for generic iPhoneOS after installing the missing Xcode iOS platform runtime and forcing Xcode's clang. It produces `IMMUnityTest.app` in DerivedData. The earlier Homebrew LLVM `-index-store-path` failure is avoided by passing Xcode's `CC`/`CXX`.
   - Simulator local status: the Unity project setting `iOSSimulatorArchitecture: 1` exports arm64 simulator Xcode projects. `BuildIOSSimulatorDevelopment` exports `Builds/iOSSimulator/Unity-iPhone.xcodeproj`; the generated project has `ARCHS = arm64`, no `UnityAds` link references, and links successfully for iPhone 16 Simulator when the generated project contains arm64 simulator builds of both `libImmUnityPlugin.a` and `libImmStrokeReader.a`.

3. Later physical-device runtime:
   - Run on an iPhone or iPad with Metal support.
   - Confirm `Init()` uses `piRenderer::API::Metal`.
   - Load `sample1.imm`.
   - Confirm paint and 360 backdrop render.
   - Confirm load/unload and app background/foreground do not crash.
   - Current local status: no physical iOS device is visible to Xcode on this machine (`xcrun devicectl list devices` reports no devices; `xcrun xctrace list devices` only lists the Mac and iOS 18.6 simulators). This is a later hardware-validation task and is not part of the current completion gate.
   - The unsigned local build output is:

     ```text
     /Users/andrewbaker/Library/Developer/Xcode/DerivedData/Unity-iPhone-bheyjkhqjitkfhaehovdqpyetwle/Build/Products/Debug-iphoneos/IMMUnityTest.app
     ```

   - The generated app bundle id is:

     ```text
     com.Immersive-Foundation.IMM-Unity-Test
     ```

   - Once a provisioned device is attached, the remaining runtime smoke should be run from the generated Xcode project with a real destination and signing settings, then logs should be collected from the device. The pass/fail evidence should include native plugin initialization, Metal renderer selection, `sample1.imm` load, GPU load, visible frame output, clean unload, background/foreground survival, and audio decode/play/progress markers.

4. Current arm64 iOS Simulator runtime:
   - Run on an Apple Silicon iOS Simulator only; do not add or depend on x86_64 simulator support.
   - Confirm the simulator Xcode project is arm64 and uses simulator archives, not iPhoneOS archives, for `ImmUnityPlugin` and `ImmStrokeReader`.
   - Current local status: unsigned arm64 simulator Xcode build succeeds and app launch creates a Unity Metal device (`Apple iOS simulator GPU`). Native log creation was fixed by passing a full log path under `Application.temporaryCachePath`. Static iOS rendering-plugin registration was fixed by adding `ImmUnityPluginRegister.mm` and calling `ImmUnityRegisterRenderingPlugin()` before native `Init()`. The current native log proves Unity Metal interface acquisition, Metal renderer creation/initialization, IMM player initialization, `sample1.imm` load, three Opus decodes, three accepted AVFoundation play calls, three `state=playing` transitions, and three playback-progress markers. After waiting for async import/sequence readiness, render events report paint and 360 picture draw calls, and `build/ios-simulator-rendering-after-wait.png` visibly shows the sample scene.

4. iOS audio policy:
   - Current status: native iOS audio is enabled in the plugin through the shared AVFoundation/Opus backend and is compile/link/package-proven for iPhoneOS.
   - Simulator runtime proof is complete for the current gate: `sample1.imm` decodes embedded sounds, accepts playback, advances playback, and tears down cleanly.
   - Physical-device playback remains a later hardware-validation task.

5. Native archive link smoke:
   - `cmake --build build/ios --target ImmUnityIOSLinkSmoke --config Release`
   - This is not a substitute for a Unity-generated Xcode build, but it proves the packaged static archive contains the plugin, player, importer, core, renderer, and third-party object code needed for a force-loaded iPhoneOS link.

### Cross-Platform Regression Tests

1. Windows:
   - Build `code/projects/windows/imm.sln` in Release x64.
   - Confirm D3D11 path still initializes.

2. Android:
   - Build `:appImmUnity:assembleDebug`.
   - Confirm GLES path still initializes.

3. Stroke reader:
   - Rebuild macOS, iOS, Windows, and Android stroke reader targets if touched indirectly.

### Deliverables

- Native viewer screenshots or captures from macOS Metal playback.
- Native audio validation logs proving decode, accepted playback, observed playing state, and playback progress.
- Unity screenshots or captures from macOS Metal playback.
- Native logs from successful initialization and shutdown in both hosts.

## Phase 9 - Cleanup and Hardening

### Objectives

Turn the initial Metal backend and both host integrations into maintainable production code.

### Tasks

1. Remove temporary test triangle paths and debug-only logs.

2. Add clear errors for unsupported Metal features.

3. Add resource lifetime tracking:
   - Buffers.
   - Textures.
   - Pipeline states.
   - Render targets.
   - Samplers.

4. Cache pipeline states by shader, vertex layout, blend state, depth state, render target format, and multisample count.

5. Add shader compilation error reporting to IMM logs.

6. Document unsupported renderer features, if any:
   - Compute.
   - Full GPU indirect draws. CPU fallback exists for shared command buffers.
   - Dynamic blending beyond disabled and source-alpha blending for render target 0.
   - Cross-API texture wrapping through `CreateTextureFromID`; Metal texture residency is a no-op and `GetTextureHandle()` returns an opaque Metal texture pointer.
   - Dynamic point size and non-default line width. `SetPointSize(false, ...)` and `SetLineWidth(1.0)` are no-ops.
   - Explicit render memory barriers. `RenderMemoryBarrier()` is currently a validated no-op because standalone playback relies on Metal encoder/command ordering.
   - GPU timestamp queries. CPU-side timing fallback exists for standalone performance measurement.

7. Remove OpenGLCore macOS recommendation from user-facing Unity errors.

8. Keep OpenGLCore fallback only if it remains buildable and useful. Otherwise, make Metal the only supported macOS renderer for `appImmViewer` and `ImmUnity`.

### Deliverables

- Maintainer-facing Metal renderer notes.
- Clear native and Unity runtime errors.
- Stable repeated load/unload behavior.

## Suggested Milestones

Milestones 1 through 5 are the standalone-player track. Unity work in those milestones is limited to keeping existing targets compiling. Milestones 6 and 7 are the macOS Unity track and should consume the renderer behavior proven by the standalone player. Milestone 8 extends the same Unity plugin work to iOS packaging, linking, and device smoke validation.

### Milestone 1 - Shared Metal Skeleton

Scope:

- Add `piRenderer::API::Metal`.
- Add compiling stub `piRendererMetal`.
- Update macOS CMake to link Metal frameworks.
- Keep both `appImmViewer` and `ImmUnity` compiling.

Success criteria:

- macOS builds complete with the Metal backend present.
- Selecting Metal reaches the stub backend and fails intentionally.

### Milestone 2 - Native Metal Window

Scope:

- Update `appImmViewer` to create a Metal-backed native window.
- Add native host frame context.
- Clear and present a Metal drawable.

Success criteria:

- `appImmViewer` opens a Metal window and presents frames.
- No Unity integration is involved.

### Milestone 3 - Native Metal Test Draw

Scope:

- Implement minimal Metal buffers, shaders, pipeline state, and draw calls.
- Draw simple geometry through `piRendererMetal`.

Success criteria:

- `appImmViewer` draws a known shape through the shared renderer backend.

### Milestone 4 - Native Static Paint Playback

Scope:

- Implement renderer and shader support needed for static paint layers.

Success criteria:

- A stroke-heavy IMM renders correctly enough in `appImmViewer` to validate transforms, depth, and color.

### Milestone 5 - Native Full Representative Playback

Scope:

- Add picture-layer support.
- Treat native macOS audio decode/playback as required standalone functionality, not polish.
- Broaden native macOS audio playback coverage beyond the first Ogg Opus sample, or explicitly document why private authored-audio samples are the only available evidence.
- Harden resource lifetime and resize behavior.
- Document which `piRendererMetal` features are proven by native playback and which are still stubbed.
- Keep model-layer playback out of the current readiness target unless a later product requirement explicitly moves it back in.

Success criteria:

- Representative IMM files render paint and picture layer types in `appImmViewer`, and IMM audio plays through the native standalone macOS host.
- Audio validation is a required pass/fail gate, not supporting evidence. It fails on silent fallback, missing Ogg Opus support, rejected `Play()` calls, failure to reach observed playback progress, or dirty audio teardown; at least one additional authored audio sample is identified or the lack of redistributable audio coverage is explicitly documented.
- This milestone is the go/no-go gate for starting Unity Metal integration.

### Milestone 6 - Unity Metal Plugin Loads

Scope:

- Add Unity Metal device detection.
- Add Unity frame adapter.
- Update managed macOS guard.

Success criteria:

- Unity macOS Metal project loads `ImmUnityPlugin.bundle`.
- `Init()` selects Metal and render events reach the Metal backend.
- The Unity path reuses the renderer and frame-context concepts proven in the standalone player.

### Milestone 7 - Unity Metal Playback

Scope:

- Adapt Unity render-target/command-buffer integration.
- Apply validated Metal player configuration.
- Validate sample IMM playback in Unity.
- Fix normal sample-scene managed/native feature queries after `IsSequenceReady`, especially layer metadata refresh, initial playback state, and spawn-area metadata/viewpoint setup.
- Prove visible Unity framebuffer composition against the standalone `sample1.imm` expectation: branch, character, and 360 backdrop.
- Enable macOS Unity embedded audio through the proven AVFoundation backend.

Success criteria:

- macOS Unity Editor and macOS standalone player render IMM content with Metal.
- The macOS Unity standalone player runs without diagnostic bypasses such as `IMM_UNITY_DISABLE_FEATURE_POST_LOAD`. Complete locally on 2026-05-25 for the standalone player smoke after the layer-info ABI fix.
- A Unity framebuffer capture visibly contains the expected `sample1.imm` IMM content, not only Unity's default sky/ground.
- The Unity Metal path uses the correct `FromZeroToOne` projection-matrix configuration for Unity's Metal GPU projection matrices, reversed-depth configuration, and external-device 360-backdrop far-depth convention. Complete locally on 2026-05-25; together these fixes produce the visible, upright `sample1.imm` Unity framebuffer capture.
- The macOS Unity standalone player decodes and plays `sample1.imm` embedded audio through AVFoundation. Complete locally on 2026-05-25 for the standalone player smoke: three Ogg Opus decodes, three accepted `Play()` calls, three playing-state transitions, progress markers, and clean temp-file teardown.

### Milestone 8 - Unity iOS Plugin Build and Device Smoke

Scope:

- Add an iOS `ImmUnity` native plugin target.
- Package the iOS native plugin into the Unity package.
- Use `__Internal` DllImport for iOS player builds.
- Build an iOS Unity player that links the native plugin.
- Smoke-test loading, rendering, and audio playback for `sample1.imm` in the arm64 iOS Simulator. Physical-device smoke testing follows later when hardware is available.

Success criteria:

- `cmake --build build/ios --target ImmUnity --config Release` produces the iOS native plugin artifact. Complete locally on 2026-05-25.
- `cmake --build build/ios --target ImmUnityIOSLinkSmoke --config Release` links the merged static archive into an iPhoneOS executable. Complete locally on 2026-05-25.
- Unity's iOS build exports a generated Xcode project that references `libImmUnityPlugin.a` without manual native-library edits. Complete locally on 2026-05-25.
- Full unsigned Xcode compile/link of that generated project succeeds for generic iPhoneOS with `CODE_SIGNING_ALLOWED=NO`. Complete locally on 2026-05-25 after installing the missing Xcode iOS platform runtime.
- Unity's generated iOS Simulator project is arm64 on Apple Silicon and unsigned Xcode compile/link succeeds for iPhone 16 Simulator when generated with arm64 simulator plugin archives. Complete locally on 2026-05-25.
- The arm64 iOS Simulator app registers the static rendering plugin, initializes IMM with Unity Metal interfaces, creates/initializes the Metal renderer, initializes the IMM player, and loads `sample1.imm`. Complete locally on 2026-05-25.
- The arm64 iOS Simulator app renders visible `sample1.imm` content after async import/sequence readiness, including paint, character/branch geometry, and the 360 backdrop. Complete locally on 2026-05-25; capture: `build/ios-simulator-rendering-after-wait.png`.
- Embedded IMM audio uses the native AVFoundation/Opus backend on iOS. Simulator runtime proof now shows three Opus decodes, three accepted AVFoundation play calls, three `state=playing` transitions, and three playback-progress markers. Complete for the current simulator gate.
- Later physical-device evidence should come from a real iPhone/iPad. That follow-up should prove physical-device rendering, audio policy, background/foreground behavior, thermals, permissions, signing, and provisioning behavior.

## Open Questions

1. What Unity version is the target?

2. Is Apple Silicon only acceptable, or does the plugin need a universal `arm64 + x86_64` bundle?

3. Is OpenGLCore fallback worth preserving after Metal works?

4. Which non-model IMM layer types are required for the first useful macOS release?

5. Normal embedded IMM audio playback through the native standalone host is required for the first Metal milestone. Does the first standalone release also need stereo/XR spatial audio behavior beyond that baseline?

6. Is XR/stereo support required for Unity macOS, or can it follow after mono playback?

7. Should shader source be manually maintained in MSL, or should a shader cross-compilation toolchain be introduced?

8. Should the native viewer become a supported deliverable, or only a renderer validation harness?

9. What product-level audio controls are required beyond reliable AVFoundation playback, such as device selection, mute/volume UI, and recoverable decode/playback errors?

## Recommended First PR

The first PR should establish the shared skeleton and keep scope tight:

1. Add `piRenderer::API::Metal`.
2. Add a compiling stub `piRendererMetal`.
3. Update macOS CMake to compile Objective-C++ Metal renderer files and link Metal frameworks.
4. Add a minimal native Metal host/context abstraction, but do not implement full rendering yet.
5. Confirm `appImmViewer`, `ImmUnity`, and `ImmStrokeReader` still compile.

This de-risks the shared renderer surface before touching Unity-specific Metal interop. The second PR should make `appImmViewer` open a Metal window and present a clear color, which gives a clean standalone loop for implementing the real renderer.

## Implementation Rule

Do not treat Unity as the first visual test bed. For each new Metal renderer capability, prove it in the native standalone player first unless the capability is inherently Unity-specific, such as acquiring Unity's Metal device, rendering inside Unity's command-buffer lifecycle, or package import behavior. Once the standalone player validates the shared renderer behavior, Unity integration should be a host-adapter problem rather than a renderer bring-up problem.
