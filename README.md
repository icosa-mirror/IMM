
# About this fork

This fork is a version of the IMM code base with the dependencies vendored in and committed. This is to have a reference snapshot of a reproducible build for the Windows IMM player, as some of the dependencies may become hard to find in the future. 

The dependencies are under the `thirdparty` directory. The table below details where each dependency was sourced from and which version is committed. I moved all the dependencies of libImmCore into a single .props file. For the 3 SDK a full copy of the original .zip will be included in a github "release".

Summary of the dependencies

| Name                      | Version             | Source |
|---------------------------|---------------------|--------|
| Facebook Audio360 SDK¹    | 1.7.12 (2019-12-18) | https://www.angelofarina.it/Public/Facebook-Spatial-Workstation/Download/SDK/ |
| Oculus SDK for Windows    | 32.0 (2021-08-30)   | https://developers.meta.com/horizon/downloads/package/oculus-sdk-for-windows/ |
| Oculus Platform SDK       | 81.0 (2025-10-30)   | https://developers.meta.com/horizon/downloads/package/oculus-platform-sdk/ |
| libogg                    | 1.3.5#1             | vcpkg |
| libvorbis                 | 1.3.7#3             | vcpkg |
| libopusenc                | 0.2.1#3             | vcpkg |
| opus                      | 1.5.2               | vcpkg |
| libjpeg-turbo             | 3.0.4               | vcpkg |
| libpng                    | 1.6.43#3            | vcpkg |
| zlib                      | 1.3.1               | vcpkg |


¹ This SDK is discontinued and Facebook removed all download links from their website. The included copy was archived by Angelo Farina.


Aside from fixing a couple of includes no other change was made to the code base.

To build I used Visual Studio 2022 with Windows SDK 10.0.26100. Build order: libCore > libImmImporter > libImmPlayer > appImmViewer.

Original Readme.md below.

--------------------------------

# Introduction

Immersive media (IMM) is an API-neutral runtime immersive media delivery format. IMM provides an efficient, extensible, interoperable format for the transmission and loading of immersive 3D and 2D animated content of mixed media types (geometry, pictures, 360 panoramas, stroke based paintings, etc).

## Context

With the advent of VR/AR technology and platforms, filmic and animated storytelling can happen in 6 degrees of freedom, in that the viewer is located in the same space where the story is being told. Unlike traditional film or 3D animation where the final delivery format is 2D pixels, for VR storytelling the content needs to stay 3D until the very moment it is presented to the user. This requires the equivalent of a new media file format that can handle true immersion. Unlike depth-based 360 stereo video or light-fields based video, IMM is designed to transmit a true, full 3D description of the film.

This is achieved by honoring the original 3D nature of the content. 3D models, 3D paint strokes, voxel data, and other 3D content all have special containers inside IMM so that the playback engine can produce a true immersive rendition. In addition, IMM has containers for more traditional pieces of information such as 2D pictures (positioned in 3D space), audio, 360 backgrounds, etc. IMM comes also with a scenegraph and an animation timeline, so the playback engine can reproduce the film appropriately. All data types are heavily compressed for quick streaming of the data from the internet to the user's device.

The current IMM repository contains the IMM exporter and importer, as well as a reference playback engine.

IMM has been used to deliver a few dozen films, including the Tribeca film festival nominated "Rebels", the "Tale of Soda Island" series,  “The Remedy”, "Goodbye Mr. Octopus", "4 Stories" and many more.


## IMM Basics

IMM files store binary data. Scenegraph and animation metadata is uncompressed binary for easy streaming and rapid parsing of large files. This usually represents a negligible fraction of a film's file size. On the other hand, all asset data is binary compressed for efficient storage, with a specific compression tailored for each container type. The asset data is readable in random order, and it is recommended the playback engine loads it on demand and streamlined in and out of memory as needed by the scenegraph and animation timeline. The reference player in the IMM repository shows how to do this.

## Versioning

IMM is a living format, and it's expected to evolve rapidly together with the VR animation industry. Because of that, each data container comes with a versioning schema that can be used to keep backwards compatibility as needed.


# Project architecture

## Modules

The IMM project contains a set of libraries, binaries and extra files. You&#39;ll find them all at the root of the IMM/ folder:


|Name|Type|Description|
| --- | --- |--- |
|libImmExporter/ | library| Exporting a scene graph into IMM |
|libImmImporter/ | library| Reading an IMM file into memory |
|libImmPlayer/ | library| Reference player capable of plating IMM files (GL and DX renderers) |
|libCore/ | library| OS services, containers, Rendering, Sound, etc |
|appImmViewer/ | binary | An native IMM player for Windows, base on libImmPlayer |
|appImmUnity/ | binary | A Unity IMM player plugin, based on libImmPlayer |
|appDX11ShaderCompiler/ |binary | A command line utility to compile DX11 shader, needed by libImmPlayer |
|ImmUnitySampleProject/ | project | A Unity project showing how to use appImmUnity
|projects/| project| Contains all the Visual Studio and Android project files to build Imm


## Dependencies

This is the dependency hierarchy for IMM playback solutions

![fig1](/docs/fig1.png)

This is the dependency hierarchy for IMM import and export pipelines. Note, we do recommend NOT importing IMM files for further art authoring, since IMM has already been optimized for storage, transmission and playback. Think of the IMM as a JPG - if you want to modify its content, you probably want to do it in the source PSD or PNG file and re-export to JPG again.

![fig2](/docs/fig2.png)

# Building the Libraries

The ImmViewer works both on Mono and in VR (either with Oculus RIFT or Oculus Quest + Link) or for monoscopic rendering.

## On Android (for Quest)

1. Download Android Studio (version 4.1.0) from developer.android.com.

2. Open folder thirdparty/lpng1637/projects/androidstudio in Android Studio first and build the project.

2. Open projects/android/ in android studio and build libCore, libImmImporte, libImmPlayer, appImmViewer in order.

3. Connect your Quest or Quest 2 Device to your computer and follow the Developer Guide to enable Developer mode on your device. There will be a Quest device showing up on your Android Studio.

4. Add a new Android App runnable in Configurations. Set the module as android.appImmViewer and click the play button on the menu bar.

### Loading IMM files on Android player

The Android player looks for content in this order:

1. `/sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM/default.imm` (if present)
2. `/sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM/default` (folder-based Quill export)
3. The newest `.imm` file in `/sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM/`
4. `sample1.imm` bundled in the APK

To play a different file, copy it to the app’s external files directory:

`/sdcard/Android/data/org.linuxfoundation.imm.player/files/IMM/`

Note: Quest devices load from the app folder as expected. Many Android phones do not allow apps to read files copied into `Android/data` via MTP/adb; for those devices, use the intent flow below.

### Opening .imm files via Android intents

The Android player accepts `ACTION_VIEW` intents for `.imm` files. If a file manager provides a `content://` URI, the player will copy it into the app’s internal files directory and load it from there. This is the most reliable approach on Android phones.
# Local plugin builds (auto-copy into Unity sample project)

This repo auto-copies plugin binaries into the Unity sample UPM packages during local builds.
The destinations are:
- ImmUnity plugin: `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-unity/Plugins/...`
- ImmStrokeReader plugin: `code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/Plugins/...`

### Windows (Visual Studio / MSBuild)

Build the Windows solution:

```powershell
msbuild code/projects/windows/imm.sln /p:Configuration=Release /p:Platform=x64 /m
```

This builds and auto-copies:
- `ImmUnityPlugin.dll` → `Packages/com.immersive-foundation.imm-unity/Plugins/x86_64`
- `ImmStrokeReader.dll` → `Packages/com.immersive-foundation.imm-stroke-reader/Plugins/x86_64`

### macOS (CMake)

```bash
cmake -S code/projects/macos -B build/macos -DIMM_BUILD_VIEWER=OFF
cmake --build build/macos --target ImmStrokeReader --config Release
```

This builds and auto-copies:
- `libImmStrokeReader.dylib` → `Packages/com.immersive-foundation.imm-stroke-reader/Plugins/macOS`

Note: the ImmUnity macOS bundle copy is also wired, but the macOS ImmUnity build is currently disabled in CI.

### iOS (CMake)

```bash
cmake -S code/projects/ios -B build/ios \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/ios --target ImmStrokeReader --config Release
```

This builds and auto-copies:
- `libImmStrokeReader.a` → `Packages/com.immersive-foundation.imm-stroke-reader/Plugins/iOS`

### Android (Gradle)

```bash
cd code/projects/android
./gradlew :appImmUnity:assembleDebug
./gradlew :appImmStrokeReader:assembleDebug
```

This builds and auto-copies:
- `libImmUnityPlugin.so` → `Packages/com.immersive-foundation.imm-unity/Plugins/Android/libs/arm64-v8a`
- `libImmStrokeReader.so` → `Packages/com.immersive-foundation.imm-stroke-reader/Plugins/Android/arm64-v8a`
