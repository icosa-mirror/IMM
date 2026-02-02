# Android (Quest) Build Notes

## Build (VR)

From `code/projects/android`:

```bash
./gradlew :libImmCore:assembleDebug :libImmImporter:assembleDebug :libImmPlayer:assembleDebug -PimmBuildDir=build_vr
./gradlew :appImmViewer:assembleDebug -PimmNonVr=OFF -PimmBuildDir=build_vr
```

APK output:

`code/projects/android/appImmViewer/build_vr/outputs/apk/debug/appImmViewer-debug.apk`

## Loading IMM content on device

The Android player loads content in this order:

1. `/sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM/default.imm` (if present)
2. `/sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM/default` (folder-based Quill export)
3. The newest `.imm` file in `/sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM/`
4. `sample1.imm` bundled in the APK

To play a different file, copy it to the app’s external files directory:

`/sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM/`

Note: Quest devices load from the app folder as expected. Many Android phones do not allow apps to read files copied into `Android/data` via MTP/adb; for those devices, use the intent flow below.

## Opening .imm files via Android intents

The Android player accepts `ACTION_VIEW` intents for `.imm` files. If a file manager provides a `content://` URI, the player will copy it into the app’s internal files directory and load it from there. This is the most reliable approach on Android phones.
