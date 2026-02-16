# Android Build Notes

All commands run from `code/projects/android`.

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
