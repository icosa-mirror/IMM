# Android Audio - Investigation Findings

## Problem

No audio on Android for both the standalone player (appImmViewer) and the Unity plugin (appImmUnity).

## Architecture Overview

The sound system has a backend abstraction (`piSoundEngineBackend`) with these implementations:

| API Enum | Backend Class | Platform |
|----------|--------------|----------|
| `Null` (0) | `piSoundEngineBackendNULL` | Any (silent) |
| `DirectSound` (1) | `piSoundEngineBackendDS` | Windows |
| `DirectSoundOVR` (2) | `piSoundEngineAudioSDKBackend` | Windows + Oculus |
| `Android` (3) | `piSoundEngineBackendAndroid` | Android (OpenSLES) |

Header: `libImmCore/src/libSound/piSoundEngineBackend.h`
Factory: `libImmCore/src/libSound/piSound.cpp`
Android impl: `libImmCore/src/libSound/android/piSoundEngineAndroid.cpp`

The Android backend uses OpenSLES with a double-buffered callback (`BufferQueueCallback`) that mixes PCM from decoded sounds. Audio content is Opus-encoded and decoded at load time via Android's `AMediaCodec` NDK API, writing to a temp file for the media extractor.

## Issue 1: Unity Plugin - Hardcoded Null Backend

**File:** `appImmUnity/src/main.cpp:625`

```cpp
#if defined(__ANDROID__) || defined(ANDROID)
    gImmUnityPlugin.IMM.mSoundBackend = piCreateSoundEngineBackend(
        piSoundEngineBackend::API::Null, &gImmUnityPlugin.IMM.mLog);
```

The Unity plugin explicitly creates a `Null` sound backend on Android. Windows uses `DirectSoundOVR`. This needs to be changed to `API::Android`.

**Fix:** Change `API::Null` to `API::Android` on line 625.

## Issue 2: Unity Plugin - No Temp Path for Opus Decoding

**File:** `appImmUnity/src/main.cpp:654-656`

```cpp
piSoundEngineBackend::Configuration config;
if (!gImmUnityPlugin.IMM.mSoundBackend->Init(nullptr, deviceID, &config))
```

The `Configuration` struct is default-initialized with `mTempPath = nullptr`. The Opus decoder (`DecodeOpusToPcm` in `piSoundEngineAndroid.cpp:29`) checks:

```cpp
if (!tempDir || !*tempDir || !data || size == 0)
    return false;
```

Without a valid `mTempPath`, all Opus decoding silently fails and creates silent placeholder sounds.

The `tmpFolferName` parameter is already passed into `Init()` from Unity's `Application.temporaryCachePath` (see `ImmPlayerManager.cs:147`), but it's never used for the sound config.

**Fix:** Set `config.mTempPath = tmpFolferName` before calling `Init()` on the sound backend. The parameter is available in scope (line 594).

## Issue 3: Standalone Player - Likely Working (Needs Runtime Verification)

**File:** `appImmViewer/src/android/cpp/OvrApp.cpp:700-724`

The standalone player already uses the correct backend and configuration:

```cpp
immPlayer.soundEngineBackend = piCreateSoundEngineBackend(
    piSoundEngineBackend::API::Android, immPlayer.pLog);
// ...
piSoundEngineBackend::Configuration config;
config.mLowLatency = true;
config.mTempPath = getAssetDirectory();
immPlayer.soundEngineBackend->Init(nullptr, -1, &config);
```

The code path looks correct:
- Backend is `API::Android`
- `config.mTempPath` is set to `getAssetDirectory()` (falls back to `internalDataPath`)
- `Init()` is called, which sets up OpenSLES engine, output mix, audio player, and buffer queue
- `Tick()` is called each frame (line 1173)
- `GetEngine()` is passed to the viewer (line 556)

If the standalone player still has no audio, possible runtime causes:
- **Content has no audio layers** - the loaded IMM file may not contain sound layers
- **Opus decode failing** - `getAssetDirectory()` might return a path without write permissions, causing temp file creation to fail in `DecodeOpusToPcm`
- **OpenSLES init failing silently** - the `Init()` method returns false on failure but `ALOGF` (fatal log) would crash, so if it's running, init succeeded
- Check `adb logcat` for messages from `piSoundEngineAndroid` (log prefix in Opus decoder) or `ImmUnityPlugin`

The `NonVrApp.cpp` (line 341-359) has similar correct setup with additional fallback handling if the backend fails.

## Build Configuration

Both CMake builds already link the required Android audio libraries:

- `appImmUnity/Projects/Android/app/CMakeLists.txt:82-83,100-101` - links `OpenSLES` and `mediandk`
- `projects/android/appImmViewer/CMakeLists.txt:163-164,180-181` - links `OpenSLES` and `mediandk`
- `libImmCore/CMakeLists.txt:35` - compiles `piSoundEngineAndroid.cpp`

No build changes are needed.

## Summary of Required Code Changes

### Unity Plugin (`appImmUnity/src/main.cpp`)

1. **Line 625:** Change `API::Null` to `API::Android`
2. **Line 654-656:** Set `config.mTempPath = tmpFolferName` before calling `Init()`
3. **Line 626:** Update log message from "Sound backend created (Null)" to "Sound backend created (Android)"

### Standalone Player

No code changes identified. If audio is still missing, runtime debugging via `adb logcat` is needed to determine where the pipeline breaks (backend init, Opus decode, or playback).
