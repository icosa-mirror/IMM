# Android Viewer Build Plan

This plan outlines the current issues preventing a clean Android build of the IMM viewer and the concrete steps to resolve them. It is based on the current repo state in `C:\Users\andyb\Documents\IMM`.

## Scope
- Target: build a non-VR Android viewer first, using the native app sources in `code/appImmViewer/src/android`.
- Output: a Gradle module that produces an APK with the native library built via CMake/NDK.

## Issues and Solutions

1) Missing Android Gradle module for appImmViewer  
Issue: `code/projects/android/settings.gradle` only includes `libImmCore`, `libImmImporter`, `libImmPlayer`, and `appImmUnity`. The Android viewer sources exist, but no Gradle module references them.  
Solution:
- Create a new module (e.g. `code/projects/android/appImmViewer`) with `build.gradle`, `CMakeLists.txt`, and `src/main` that points to `code/appImmViewer/src/android`.
- Add it to `code/projects/android/settings.gradle`.
- Wire module dependencies so the native build links against `libImmCore`, `libImmImporter`, `libImmPlayer`.

2) No native build configuration for the viewer  
Issue: There is no CMake/NDK build definition for `OvrApp.cpp`, its helpers, or the `appImmViewer/src/viewer` and `appImmViewer/src/settings.cpp` sources required for the Android viewer.  
Solution:
- Add a `CMakeLists.txt` that compiles:
  - `code/appImmViewer/src/android/cpp/*.cpp`
  - `code/appImmViewer/src/viewer/*.cpp`
  - `code/appImmViewer/src/settings.cpp`
  - `code/appImmViewer/src/simpleJSON/*.cpp`
- Export include paths to `libImmCore`, `libImmImporter`, `libImmPlayer`, and appImmViewer headers.
- Link to the static libs produced by `libImmCore`, `libImmImporter`, `libImmPlayer` (same pattern as `code/appImmUnity/Projects/Android/app/CMakeLists.txt`).
- Link Android system libs: `android`, `log`, `EGL`, `GLESv3`, `z`.

3) VR SDK dependencies block a non-VR build  
Issue: `OvrApp.cpp` includes VrApi and OVR Platform headers and expects to link the corresponding libraries.  
Solution (non-VR first):
- Add a non-VR native entrypoint and exclude `OvrApp.cpp` from the build.
- Only wire VrApi/OVR Platform SDKs when enabling the VR build later.

4) Spatial audio is not available on Android in this repo  
Issue: `libImmCore` only builds the NULL audio backend on Android.  
Solution:
- Use the NULL backend for now and switch the renderer to stereo mode as a fallback.
- When Audio360 Android binaries are available, wire them into the build and restore spatial audio.

5) Android app assets are incomplete  
Issue: `Utils.kt` expects `quill-error.imm` and `retail-demo.imm` in the APK assets, but `code/appImmViewer/src/android/assets` only contains signature files.  
Solution:
- Add the required `.imm` assets to `code/appImmViewer/src/android/assets/`.
- Or update `Utils.kt` to point to available sample IMM content (e.g. `exampleImmFiles/sample1.imm`) and ensure it’s packaged as an asset.

6) Manifest, native library name, and Java/Kotlin wiring  
Issue: `AndroidManifest.xml` references `android.app.lib_name=appImmPlayer`, and the Kotlin code loads `System.loadLibrary("appImmPlayer")`. The native library must match this name.  
Solution:
- Ensure the native target name in CMake is `appImmPlayer`.
- Ensure Gradle `externalNativeBuild` uses that target and copies the resulting `.so` into the APK.

7) Storage access and scoped storage changes  
Issue: `OvrApp.cpp` checks `/sdcard/Oculus/quill/default.imm`, while `MainActivity.kt` requests legacy read/write storage permissions. Modern Android/Quest builds may block these paths without scoped storage or `MANAGE_EXTERNAL_STORAGE`.  
Solution:
- Prefer loading from app-private storage and assets; keep external storage paths only as a debug fallback.
- If external paths are required, add a storage access flow (Storage Access Framework) or target a compatible SDK with `requestLegacyExternalStorage` if allowed.

8) Gradle/NDK versions and ABI config  
Issue: The project uses AGP 8.5.2 and NDK 26.1 in CI; the viewer module needs to align with those versions and set ABI filters.  
Solution:
- Set `ndkVersion` in the new module’s `build.gradle` to match CI.
- Set `abiFilters "arm64-v8a"` for Quest.
- Set `minSdkVersion` and `targetSdkVersion` compatible with VrApi requirements.

## Proposed Implementation Steps

1) Add `appImmViewer` module under `code/projects/android`  
- Create `code/projects/android/appImmViewer/build.gradle`.  
- Point `sourceSets` to `code/appImmViewer/src/android` (manifest, java, res, assets).  
- Define `externalNativeBuild` with CMake.  
- Add module to `code/projects/android/settings.gradle`.

2) Add `CMakeLists.txt` for the viewer native library  
- Compile a non-VR native entrypoint plus `viewer` and `settings` implementation files.  
- Include headers from `libImmCore`, `libImmImporter`, `libImmPlayer`, and appImmViewer.  
- Link static libraries and Android system libs.

3) (Later) Integrate Oculus Mobile SDK and Platform SDK  
- Add SDKs under `thirdparty/`.  
- Update include paths and link libraries.  
- Restore VR-specific AndroidManifest metadata.

4) Decide on Audio360 handling  
- Use the NULL backend initially and render in stereo as a fallback.  
- Integrate Audio360 Android binaries when available.

5) Fix assets and default content  
- Add required IMM assets (or update code to point to an existing sample).  
- Verify the asset extraction code populates app-private storage.

6) Validate build and run path  
- Build `libImmCore`, `libImmImporter`, `libImmPlayer`, then `appImmViewer`.  
- Install on Quest and verify startup, entitlement checks, and asset loading.  

## Notes
- The original README references building `appImmViewer` on Android, but this fork currently lacks the Android module definition. The plan above restores that path.
- Once the module exists, CI can be updated to build and upload the Android viewer APK if desired.
