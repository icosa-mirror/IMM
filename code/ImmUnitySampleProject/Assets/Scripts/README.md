# IMM Unity Plugin - C# Wrapper

This directory contains C# wrapper classes for the IMM Unity native plugin.

## Files

- **ImmNativePlugin.cs** - Low-level P/Invoke declarations for native DLL functions
- **ImmPlayerManager.cs** - High-level manager singleton for plugin initialization and rendering
- **ImmDocument.cs** - Represents a loaded IMM document with playback controls
- **ImmPlayerExample.cs** - Example component demonstrating usage

## Quick Setup (Automated)

Use the provided helper script to automatically copy all DLLs:

**Windows Batch:**
```batch
cd C:\Users\andyb\Documents\IMM\code\appImmUnity\Scripts
CopyDllsToUnity.bat "C:\Path\To\YourUnityProject"
```

**PowerShell:**
```powershell
cd C:\Users\andyb\Documents\IMM\code\appImmUnity\Scripts
.\CopyDllsToUnity.ps1 "C:\Path\To\YourUnityProject"
```

Then manually copy the C# scripts and proceed to step 3 below.

## Manual Setup

### 1. Copy Files to Unity Project

Copy the `Scripts` folder into your Unity project's `Assets/Plugins` directory:

```
YourUnityProject/
  Assets/
    Plugins/
      ImmPlayer/
        ImmNativePlugin.cs
        ImmPlayerManager.cs
        ImmDocument.cs
        ImmPlayerExample.cs
```

### 2. Copy Native DLLs

Copy the compiled `ImmUnityPlugin.dll` and all dependency DLLs to your Unity project:

```
YourUnityProject/
  Assets/
    Plugins/
      x86_64/
        ImmUnityPlugin.dll
        Audio360.dll
        jpeg62.dll
        libpng16.dll
        ogg.dll
        opus.dll
        opusenc.dll
        vorbis.dll
        vorbisenc.dll
        zlib1.dll
```

**DLL Locations (from build):**
- `ImmUnityPlugin.dll` - From `IMM/code/appImmUnity/exe/`
- `Audio360.dll` - From `IMM/thirdparty/audio360-sdk/Audio360/Windows/x64/`
- `jpeg62.dll` - From `IMM/thirdparty/libjpeg-turbo/bin/`
- `libpng16.dll` - From `IMM/thirdparty/libpng/bin/`
- `ogg.dll` - From `IMM/thirdparty/libogg/bin/`
- `opus.dll` - From `IMM/thirdparty/opus/bin/`
- `opusenc.dll` - From `IMM/thirdparty/libopusenc/bin/`
- `vorbis.dll` - From `IMM/thirdparty/libvorbis/bin/`
- `vorbisenc.dll` - From `IMM/thirdparty/libvorbis/bin/`
- `zlib1.dll` - From `IMM/thirdparty/zlib/bin/`

### 3. Configure DLL Import Settings

In Unity Editor:
1. Select `ImmUnityPlugin.dll` in the Project window
2. In the Inspector, configure:
   - **Platforms**: Check "Editor" and "Standalone"
   - **CPU**: x86_64
   - **Load on startup**: Enabled

## Usage

### Basic Setup

1. Create an empty GameObject in your scene
2. Add the `ImmPlayerManager` component (it will persist across scenes)
3. Configure the settings:
   - **Use Linear Color Space**: Match your project's color space (usually true for modern Unity projects)
   - **Antialiasing Level**: Typically 8
   - **Log File Name**: Path for the native plugin log file

**Note**: The temporary folder is automatically set to Unity's `Application.temporaryCachePath`

### Loading and Playing Documents

#### Option 1: Using the Example Component

1. Create a GameObject in your scene
2. Add the `ImmPlayerExample` component
3. Configure:
   - **Document Path**: Path to your .imm file
   - **Target Camera**: The camera that will render the content
   - **Load On Start**: Check to auto-load
   - **Auto Play**: Check to auto-play

#### Option 2: Using the API Directly

```csharp
using ImmPlayer;

public class MyImmPlayer : MonoBehaviour
{
    private ImmDocument document;

    void Start()
    {
        // Load a document
        document = ImmPlayerManager.Instance.LoadDocument("path/to/file.imm");

        if (document != null)
        {
            // Set volume
            document.SetVolume(0.8f);

            // Position the document
            document.SetTransform(transform);

            // Start playback
            document.Resume();
            document.Show();
        }
    }

    void Update()
    {
        // Update camera matrices each frame
        Camera cam = Camera.main;
        ImmPlayerManager.Instance.SetCameraMatrices(0, cam);
    }

    void OnRenderObject()
    {
        // Issue render event
        if (document != null && document.IsLoaded)
        {
            ImmPlayerManager.Instance.IssueRenderEvent(0);
        }
    }

    void OnDestroy()
    {
        // Clean up
        if (document != null)
        {
            ImmPlayerManager.Instance.UnloadDocument(document);
        }
    }
}
```

### Playback Control

```csharp
// Pause playback
document.Pause();

// Resume playback
document.Resume();

// Restart from beginning
document.Restart();

// Skip chapters
document.SkipForward();
document.SkipBack();

// Hide/show
document.Hide();
document.Show();
```

### Chapter Navigation

```csharp
// Get chapter info
int totalChapters = document.GetChapterCount();
int currentChapter = document.GetCurrentChapter();
```

### Audio Control

```csharp
// Set volume (0.0 to 1.0)
document.SetVolume(0.5f);

// Get current volume
float volume = document.GetVolume();
```

### Spawn Areas

```csharp
// Get spawn area count
int count = document.GetSpawnAreaCount();

// Get spawn area list
int[] areaIds = document.GetSpawnAreaList();

// Get active spawn area
int activeId = document.GetActiveSpawnAreaId();

// Set active spawn area
document.SetActiveSpawnAreaId(areaIds[0]);

// Get spawn area info
var info = document.GetSpawnAreaInfo(areaIds[0]);
if (info.HasValue)
{
    string name = info.Value.GetName();
    Vector3 position = info.Value.transform.GetPosition();
    Quaternion rotation = info.Value.transform.GetRotation();
}
```

### VR/Stereo Rendering

For VR applications with stereo rendering:

```csharp
void Update()
{
    // Get VR camera matrices
    Matrix4x4 world2head = headTransform.worldToLocalMatrix;
    Matrix4x4 world2left = leftEyeTransform.worldToLocalMatrix;
    Matrix4x4 world2right = rightEyeTransform.worldToLocalMatrix;

    Matrix4x4 projHead = headCamera.projectionMatrix;
    Matrix4x4 projLeft = leftEyeCamera.projectionMatrix;
    Matrix4x4 projRight = rightEyeCamera.projectionMatrix;

    // Set stereo matrices
    ImmPlayerManager.Instance.SetStereoCameraMatrices(
        0,
        world2head, projHead,
        world2left, projLeft,
        world2right, projRight,
        ImmPlayerManager.StereoMode.SinglePass
    );
}
```

## Stereo Modes

- **Mono**: Standard mono rendering
- **TwoPass**: Stereo rendering with two passes (left eye, then right eye)
- **SinglePass**: Stereo rendering in a single pass (more efficient)

## Important Notes

1. **Camera Updates**: Call `SetCameraMatrices()` every frame before rendering
2. **Render Events**: Call `IssueRenderEvent()` during `OnRenderObject()` or similar
3. **Cleanup**: Always unload documents when done to free resources
4. **Thread Safety**: All calls must be made from the main Unity thread
5. **Color Space**: Ensure the color space setting matches your Unity project

## Debugging

To enable debug logging from the native plugin:

1. Check the log file specified in `ImmPlayerManager` settings
2. Call `ImmNativePlugin.Debug()` to verify initialization state
3. Monitor the Unity console for C# wrapper messages

## Troubleshooting

### Plugin fails to load
- Ensure DLL is in the correct platform folder (x86_64)
- Check that all dependency DLLs are present
- Verify DLL import settings in Unity Inspector

### Rendering doesn't work
- Verify `SetCameraMatrices()` is called every frame
- Ensure `IssueRenderEvent()` is called during rendering
- Check that the document is loaded and visible

### Audio doesn't play
- Verify audio device initialization in the log file
- Check that document volume is not zero
- Ensure the .imm file contains audio data

## API Reference

See inline documentation in each C# file for detailed API information.
