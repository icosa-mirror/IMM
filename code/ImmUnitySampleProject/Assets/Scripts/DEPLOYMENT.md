# IMM Unity Plugin - Deployment Guide

## Required DLLs for x64 Unity Projects

### Main Plugin
- **ImmUnityPlugin.dll** - The main IMM player plugin for Unity

### Audio & Spatial Audio
- **Audio360.dll** - Facebook Audio360 spatial audio library

### Image Codecs
- **jpeg62.dll** - JPEG image codec (libjpeg-turbo)
- **libpng16.dll** - PNG image codec

### Audio Codecs
- **ogg.dll** - Ogg container format
- **opus.dll** - Opus audio codec
- **opusenc.dll** - Opus encoder
- **vorbis.dll** - Vorbis audio codec
- **vorbisenc.dll** - Vorbis encoder

### Compression
- **zlib1.dll** - zlib compression library

## DLL Source Locations

All DLLs are located in the IMM build output and thirdparty directories:

```
IMM/
├── code/
│   └── appImmUnity/
│       └── exe/
│           └── ImmUnityPlugin.dll ...................... Main plugin DLL
│
└── thirdparty/
    ├── audio360-sdk/
    │   └── Audio360/Windows/x64/
    │       └── Audio360.dll ............................ Spatial audio
    │
    ├── libjpeg-turbo/bin/
    │   └── jpeg62.dll .................................. JPEG codec
    │
    ├── libpng/bin/
    │   └── libpng16.dll ................................ PNG codec
    │
    ├── libogg/bin/
    │   └── ogg.dll ..................................... Ogg container
    │
    ├── opus/bin/
    │   └── opus.dll .................................... Opus codec
    │
    ├── libopusenc/bin/
    │   └── opusenc.dll ................................. Opus encoder
    │
    ├── libvorbis/bin/
    │   ├── vorbis.dll .................................. Vorbis codec
    │   └── vorbisenc.dll ............................... Vorbis encoder
    │
    └── zlib/bin/
        └── zlib1.dll ................................... Compression
```

## Unity Project Deployment Structure

Copy all DLLs to the Unity project's plugin directory:

```
YourUnityProject/
└── Assets/
    └── Plugins/
        ├── x86_64/                    ← All DLLs go here
        │   ├── ImmUnityPlugin.dll
        │   ├── Audio360.dll
        │   ├── jpeg62.dll
        │   ├── libpng16.dll
        │   ├── ogg.dll
        │   ├── opus.dll
        │   ├── opusenc.dll
        │   ├── vorbis.dll
        │   ├── vorbisenc.dll
        │   └── zlib1.dll
        │
        └── ImmPlayer/                 ← C# scripts go here
            ├── ImmNativePlugin.cs
            ├── ImmPlayerManager.cs
            ├── ImmDocument.cs
            └── ImmPlayerExample.cs
```

## DLL Dependencies

The dependency chain is:

```
ImmUnityPlugin.dll
├── Audio360.dll
├── jpeg62.dll
├── libpng16.dll
│   └── zlib1.dll
├── ogg.dll
├── opus.dll
├── opusenc.dll
│   └── opus.dll
├── vorbis.dll
│   └── ogg.dll
└── vorbisenc.dll
    ├── vorbis.dll
    └── ogg.dll
```

## Deployment Methods

### Method 1: Automated (Recommended)

Use the provided helper script:

**Windows Command Prompt:**
```batch
cd C:\Users\andyb\Documents\IMM\code\appImmUnity\Scripts
CopyDllsToUnity.bat "C:\Path\To\YourUnityProject"
```

**PowerShell:**
```powershell
cd C:\Users\andyb\Documents\IMM\code\appImmUnity\Scripts
.\CopyDllsToUnity.ps1 "C:\Path\To\YourUnityProject"
```

### Method 2: Manual Copy

1. Create the directory `Assets/Plugins/x86_64` in your Unity project
2. Copy all 10 DLLs listed above to that directory
3. Create the directory `Assets/Plugins/ImmPlayer`
4. Copy all 4 C# scripts to that directory

## Unity Configuration

After copying the DLLs:

1. Open your Unity project
2. Select each DLL in the Project window
3. In the Inspector, configure:
   - **Select platforms for plugin**: Check "Editor" and "Standalone"
   - **CPU**: x86_64
   - **Load on startup**: Enabled
4. Click "Apply"

## Build Settings

When building your Unity project:

1. **Architecture**: Set to x64 (not x86)
2. **Platform**: Windows Standalone
3. **Scripting Backend**: Mono or IL2CPP (both supported)

## Verification

After deployment, verify all DLLs are present:

```powershell
# PowerShell command to list all plugin DLLs
Get-ChildItem "YourUnityProject\Assets\Plugins\x86_64\*.dll" | Select-Object Name

# Expected output (10 DLLs):
# Audio360.dll
# ImmUnityPlugin.dll
# jpeg62.dll
# libpng16.dll
# ogg.dll
# opus.dll
# opusenc.dll
# vorbis.dll
# vorbisenc.dll
# zlib1.dll
```

## Troubleshooting

### Missing DLL Errors

If you get "DllNotFoundException":
1. Verify all 10 DLLs are in `Assets/Plugins/x86_64`
2. Check DLL names match exactly (case-sensitive on some platforms)
3. Ensure Unity import settings are configured for x64

### Loading Errors

If the plugin fails to load:
1. Check Unity console for specific error messages
2. Verify your Unity project is set to x64 architecture
3. Ensure you're using a compatible Unity version
4. Try deleting the Library folder and reimporting

### Runtime Errors

If the plugin loads but crashes:
1. Check the IMM player log file (configured in ImmPlayerManager)
2. Verify color space settings match your Unity project
3. Ensure Graphics API is DirectX 11 or OpenGL Core

## Platform Support

Currently supported:
- **Windows Standalone x64** - Full support
- **Unity Editor (Windows)** - Full support

Not yet supported:
- Android (requires GLES implementation and arm64 DLLs)
- macOS (requires macOS build)
- Linux (requires Linux build)

## File Sizes

Approximate DLL sizes (Release build):

| DLL | Size |
|-----|------|
| ImmUnityPlugin.dll | ~2-5 MB |
| Audio360.dll | ~1-2 MB |
| jpeg62.dll | ~500 KB |
| libpng16.dll | ~200 KB |
| ogg.dll | ~50 KB |
| opus.dll | ~500 KB |
| opusenc.dll | ~100 KB |
| vorbis.dll | ~200 KB |
| vorbisenc.dll | ~50 KB |
| zlib1.dll | ~100 KB |
| **Total** | ~5-9 MB |

## License Considerations

Ensure you comply with the licenses of all included libraries:
- libjpeg-turbo - BSD-style license
- libpng - PNG Reference Library License
- zlib - zlib License
- Ogg/Vorbis - BSD-style license
- Opus - BSD license
- Audio360 - Check Facebook's license terms

See `ThirdPartyNotices.txt` in each library's directory for details.
