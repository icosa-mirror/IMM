# Android Build Notes

All commands run from `code/projects/android`.

## Prerequisites

- **JDK 17** — matches CI and the Android Gradle Plugin (8.5.2) / Kotlin (1.9.22) toolchain. Newer JDKs (21/22) are untested here; the system default `java` may be too new. A Unity-bundled OpenJDK 17 works — point Gradle at it via `JAVA_HOME` (e.g. `JAVA_HOME=".../Unity/Hub/Editor/<ver>/Editor/Data/PlaybackEngines/AndroidPlayer/OpenJDK" ./gradlew ...`) or `org.gradle.java.home`.
- **Android SDK** with **NDK `26.1.10909125`** (pinned in `appImmViewer/build.gradle`) and platform `android-34`.
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

The Vulkan non-VR build selects `piRenderer::API::Vulkan` in the native app and passes the Android `ANativeWindow` into the Vulkan renderer. Use a separate build directory so the default GLES APK remains available.

```bash
./gradlew :libImmCore:assembleDebug :libImmImporter:assembleDebug :libImmPlayer:assembleDebug -PimmBuildDir=build_vulkan
./gradlew :appImmViewer:assembleDebug -PimmNonVr=ON -PimmRendererApi=Vulkan -PimmBuildDir=build_vulkan
```

APK output: `appImmViewer/build_vulkan/outputs/apk/debug/appImmViewer-debug.apk`

With a Vulkan-capable Android device or emulator attached:

```powershell
./run-android-vulkan-smoke.ps1
```

The smoke installs the Vulkan APK, launches `sample1.imm`, captures `logcat`, and requires Vulkan surface/device initialization plus picture and static-paint draw submission markers.

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
| `-PimmRendererApi=GLES/Vulkan` | `GLES` | Selects the non-VR Android renderer backend. Use `Vulkan` with `-PimmBuildDir=build_vulkan` to keep the default GLES APK separate. |

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

Settings: `code/appImmViewer/exe/settings.json` (set `EnableVR: false` for desktop non-VR).
