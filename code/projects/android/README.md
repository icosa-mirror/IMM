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

The Android VR player loads content in this order:

1. `/sdcard/IMM/default.imm` (if present)
2. `/sdcard/IMM/default` (authoring folder)
3. The newest `.imm` file in `/sdcard/IMM/`
4. `sample1.imm` bundled in the APK

To play a different file, copy it to `/sdcard/IMM/`. The player uses `default.imm` if it exists, otherwise it picks the newest `.imm` in that directory.
