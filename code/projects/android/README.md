# Android Build Notes

All commands run from `code/projects/android`.

## Prerequisites

- **JDK 17** — matches CI and the Android Gradle Plugin (8.5.2) / Kotlin (1.9.22) toolchain. Newer JDKs (21/22) are untested here; the system default `java` may be too new. Point Gradle at a JDK 17 install via `JAVA_HOME` or `org.gradle.java.home` when needed.
- **Android SDK** with **NDK `26.1.10909125`** (pinned in `appImmViewer/build.gradle`) and platform `android-34`.
- **Android Godot GDExtension builds** additionally require Python, SCons, `godot-cpp` for Godot 4.5, and NDK `28.1.13356709`.
- **Android Godot export/runtime smoke** requires a Godot 4.5 console executable, Android export templates, and an attached Vulkan-capable Android device or emulator.
- Tell Gradle where the SDK is — either export `ANDROID_SDK_ROOT`, or create `code/projects/android/local.properties` (gitignored):

  ```properties
  sdk.dir=C:\\path\\to\\Android\\Sdk
  ```

## Build (Unity plugin libraries — .so for the UPM packages)

These produce the committed Android plugin binaries consumed by the Unity packages:

| Gradle task | Output (`arm64-v8a`) | Copied into |
|-------------|----------------------|-------------|
| `:appImmUnity:assembleDebug` | `libImmUnityPlugin.so` | `com.immersive-foundation.imm-unity/Plugins/Android/libs/arm64-v8a/` |
| `:appImmStrokeReader:assembleDebug` | `libImmStrokeReader.so` | `com.immersive-foundation.imm-stroke-reader/Plugins/Android/arm64-v8a/` |

```bash
./gradlew :libImmCore:assembleDebug :libImmImporter:assembleDebug :libImmPlayer:assembleDebug \
          :appImmUnity:assembleDebug :appImmStrokeReader:assembleDebug
```

Build the `libImm*` modules first (they are dependencies). Each app module's `copyToUnity` Gradle task copies its `.so` into the matching UPM package automatically, so the committed package binaries are updated in place. Only `arm64-v8a` is built (`abiFilters`).

## Build (Non-VR — phones, tablets)

```bash
./gradlew :appImmViewer:assembleDebug -PimmNonVr=ON
```

APK output: `appImmViewer/build/outputs/apk/debug/appImmViewer-debug.apk`

Install and launch:

```bash
adb install -r appImmViewer/build/outputs/apk/debug/appImmViewer-debug.apk
adb shell am start -n org.linuxfoundation.imm.player/.MainActivity
```

### Vulkan Non-VR build and smoke

The non-VR build defaults to Vulkan and passes the Android `ANativeWindow` into the Vulkan renderer. Use a separate build directory for local smoke runs if you want to keep another APK output around.

```bash
./gradlew :libImmCore:assembleDebug :libImmImporter:assembleDebug :libImmPlayer:assembleDebug -PimmBuildDir=build_vulkan
./gradlew :appImmViewer:assembleDebug -PimmNonVr=ON -PimmBuildDir=build_vulkan
```

APK output: `appImmViewer/build_vulkan/outputs/apk/debug/appImmViewer-debug.apk`

With a Vulkan-capable Android device or emulator attached:

```powershell
./run-android-vulkan-smoke.ps1
```

The smoke resolves `adb` from `-Adb`, the `ADB` environment variable, `PATH`, `local.properties`, or `ANDROID_SDK_ROOT`. It installs the Vulkan APK, launches `sample1.imm`, captures `logcat`, and requires Vulkan surface/device initialization plus picture and static-paint draw submission markers. It fails if the Vulkan renderer logs placeholder draw-submission diagnostics.

Use the generic renderer smoke when checking renderer fallback regressions:

```powershell
./run-android-renderer-smoke.ps1 -RendererApi Vulkan
./run-android-renderer-smoke.ps1 -RendererApi GLES
```

`./run-android-gles-smoke.ps1` is a GLES-specific wrapper. The smoke uses the same non-VR launch path as the manual `adb shell am start -n org.linuxfoundation.imm.player/.MainActivity` command; pass `-UseIntentRendererExtra` only when explicitly testing the `RenderingAPI` intent override. The GLES gate requires GLES selection plus CPU/GPU `sample1.imm` load completion; the Vulkan gate adds Vulkan surface/device/draw-submission markers.

`./run-android-openxr-probe-smoke.ps1` builds and launches the explicit non-VR
OpenXR startup probe (`-PimmXrRuntime=OpenXR`). It validates Android OpenXR
loader initialization, extension enumeration, instance creation, HMD system
query, stereo-view query, and teardown through `IMM_ANDROID_OPENXR_PROBE` log
markers. Quest OS lockscreen, Guardian, and reprojected-dialog focus blockers
are classified before marker checks.

CI installs the Android SDK/NDK through `android-actions/setup-android` and builds this Vulkan APK with `-PimmNonVr=ON -PimmBuildDir=build_vulkan`. Runtime smoke is local/device-gated because it requires an attached Vulkan-capable device or emulator.

## Build and smoke (Godot Android GDExtension)

The Android Godot build stages the native IMM plugin and GDExtension libraries into the sample project's Android addon bin directory:

```powershell
./build-godot-extension-android.ps1 -Configuration Debug -BootstrapGodotCpp -BuildGodotCpp
```

Use `-PreflightOnly` to verify tool discovery without building. The helper resolves the Android SDK/NDK from parameters or environment and can build `godot-cpp` for the pinned Godot 4.5 target.

With the staged Android libraries, a Godot 4.5 console executable, Android export templates, and a Vulkan-capable device or emulator attached:

```powershell
./run-godot-android-vulkan-smoke.ps1 -GodotExe C:\path\to\Godot_v4.5-stable_win64_console.exe
```

The smoke stages `exampleImmFiles/sample1.imm` into the Godot sample project only for export, removes the staged copy afterward, exports the Android debug APK, installs it, launches the Vulkan visual smoke scene, captures `logcat`, pulls the saved PNG from app data, and requires the native Vulkan compositor to report successful picture and static-paint draw submission.

## Build (VR — Quest)

The VR build requires separate library compilation with a dedicated build dir to avoid conflicts:

```bash
./gradlew :libImmCore:assembleDebug :libImmImporter:assembleDebug :libImmPlayer:assembleDebug -PimmBuildDir=build_vr
./gradlew :appImmViewer:assembleDebug -PimmNonVr=OFF -PimmBuildDir=build_vr
```

APK output: `appImmViewer/build_vr/outputs/apk/debug/appImmViewer-debug.apk`

Install and launch on Quest:

```bash
adb install -r appImmViewer/build_vr/outputs/apk/debug/appImmViewer-debug.apk
adb shell am start -n org.linuxfoundation.imm.player/.MainActivity
```

## Build flag reference

| Flag | Default | Description |
|------|---------|-------------|
| `-PimmNonVr=ON/OFF` | `ON` | `ON` = phone/tablet build, `OFF` = Quest VR build |
| `-PimmBuildDir=<dir>` | `build` | Custom build output dir (use `build_vr` for VR to avoid conflicts) |
| `-PimmRendererApi=GLES/Vulkan` | `Vulkan` for non-VR, `GLES` for VR | Selects the Android startup renderer. Non-VR can still request `RenderingAPI` through intent extras before native renderer initialization. |

## Loading IMM content on device

The Android player loads content in this order:

1. `/sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM/default.imm` (if present)
2. `/sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM/default/` (folder-based Quill export)
3. The newest `.imm` file in `/sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM/`
4. `sample1.imm` bundled in the APK

To play a different file, copy it to the app's external files directory:

```bash
adb push myfile.imm /sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM/
```

Note: Quest devices load from the app folder as expected. Many Android phones do not allow apps to read files copied into `Android/data` via MTP/adb; for those devices, use the intent flow below.

## Opening .imm files via Android intents

The Android player accepts `ACTION_VIEW` intents for `.imm` files. If a file manager provides a `content://` URI, the player will copy it into the app's internal files directory and load it from there. This is the most reliable approach on Android phones.

## Windows standalone viewer

From the repository root:

```bash
MSBuild code/projects/windows/imm.sln -t:appImmViewer -p:Configuration=Release -p:Platform=x64 -m
```

Executable output: `code/appImmViewer/exe/appImmViewer_Release.exe`

Settings: `code/appImmViewer/exe/settings.json` defaults to desktop non-VR Vulkan. Use `code/appImmViewer/exe/settings-opengl.json` for the OpenGL fallback/reference path.
