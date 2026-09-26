# ImmStrokeReader Unity Plugin

A Unity plugin that reads IMM files and exposes raw stroke data (positions, colors, widths, etc.) via a C API. Independent from the rendering plugin.

## Architecture

```
ImmStrokeReader.dll
├── libImmCore.lib (math, logging, memory)
├── libImmImporter.lib (document parsing)
└── zlib.lib (decompression)
```

No rendering, no audio, no player - just document loading and stroke data access.

## Implementation Status

### Complete

| Component | File(s) |
|-----------|---------|
| Stroke Collector Interface | `libImmImporter/src/fromImmersive/strokeCollector.h` |
| Import API Changes | `fromImmersive.h`, `fromImmersive.cpp` |
| Layer Paint Hook | `fromImmersiveLayerPaint.cpp:509-520` |
| Stroke Store | `src/strokeStore.h`, `src/strokeStore.cpp` |
| C API Exports | `src/main.cpp` |
| VS Project | `appImmStrokeReader.vcxproj` |
| Windows DLL | `exe/ImmStrokeReader.dll` (313 KB) |

### Not Yet Done

- [x] Unity C# wrapper (P/Invoke bindings) - `ImmUnitySampleProject/Assets/Scripts/ImmStrokeReader.cs`
- [x] Unity integration testing - Verified: 21 layers, 354 strokes, 25,653 points from logo_animation.imm
- [ ] Android build configuration
- [ ] macOS/iOS build configuration

## C API Reference

### Lifecycle
```c
int  StrokeReader_Init(char* logFileName);      // Returns 0 on success
void StrokeReader_End();
bool StrokeReader_IsInitialized();
```

### Loading
```c
int  StrokeReader_LoadFromFile(char* fileName);       // Returns docId or negative error
int  StrokeReader_LoadFromMemory(void* data, int size);
void StrokeReader_Unload(int docId);
int  StrokeReader_GetDocumentCount();
```

### Querying
```c
int  StrokeReader_GetLayerCount(int docId);
bool StrokeReader_GetLayerInfo(int docId, int layerIdx, StrokeLayerInfoC* info);
int  StrokeReader_GetDrawingCount(int docId, int layerIdx);
int  StrokeReader_GetStrokeCount(int docId, int layerIdx, int drawingIdx);
bool StrokeReader_GetStrokeInfo(int docId, int layerIdx, int drawingIdx, int strokeIdx, StrokeInfoC* info);
bool StrokeReader_GetStrokePoints(int docId, int layerIdx, int drawingIdx, int strokeIdx, StrokePointC* points, int maxPoints);
```

### C Structs

```c
struct StrokeLayerInfoC {
    int id;
    int type;
    int numDrawings;
    wchar_t name[128];
};

struct StrokeInfoC {
    int brushType;
    int visibilityMode;
    int numPoints;
    float bboxMinX, bboxMinY, bboxMinZ;
    float bboxMaxX, bboxMaxY, bboxMaxZ;
};

struct StrokePointC {
    float px, py, pz;      // position
    float nx, ny, nz;      // normal
    float dx, dy, dz;      // view direction
    float r, g, b;         // color (0-1, gamma space)
    float alpha;           // transparency (0-1)
    float width;           // stroke width (quantized, needs biggestStroke to convert)
};
```

## Building

Open `projects/windows/imm.sln` in Visual Studio and build the `appImmStrokeReader` project. The DLL is output to `exe/ImmStrokeReader.dll` and automatically copied to `ImmUnitySampleProject/Assets/Plugins/x86_64/`.

## Usage Example (C#)

### Low-level API (direct P/Invoke)

```csharp
using ImmPlayer;

// Initialize
ImmStrokeReader.StrokeReader_Init(null);

// Load document
int docId = ImmStrokeReader.StrokeReader_LoadFromFile("path/to/file.imm");
if (docId < 0)
{
    Debug.LogError("Failed to load");
    return;
}

// Query layers
int layerCount = ImmStrokeReader.StrokeReader_GetLayerCount(docId);
for (int l = 0; l < layerCount; l++)
{
    if (ImmStrokeReader.StrokeReader_GetLayerInfo(docId, l, out StrokeLayerInfo layerInfo))
    {
        Debug.Log($"Layer {l}: {layerInfo.name}, {layerInfo.numDrawings} drawings");

        // Query drawings and strokes
        for (int d = 0; d < layerInfo.numDrawings; d++)
        {
            int strokeCount = ImmStrokeReader.StrokeReader_GetStrokeCount(docId, l, d);
            for (int s = 0; s < strokeCount; s++)
            {
                if (ImmStrokeReader.StrokeReader_GetStrokeInfo(docId, l, d, s, out StrokeInfo strokeInfo))
                {
                    // Allocate and get points
                    StrokePoint[] points = new StrokePoint[strokeInfo.numPoints];
                    ImmStrokeReader.StrokeReader_GetStrokePoints(docId, l, d, s, points, strokeInfo.numPoints);

                    // Use point data...
                    foreach (var pt in points)
                    {
                        Vector3 pos = pt.Position;
                        Color col = pt.Color;
                    }
                }
            }
        }
    }
}

// Cleanup
ImmStrokeReader.StrokeReader_Unload(docId);
ImmStrokeReader.StrokeReader_End();
```

### High-level API (StrokeReaderDocument wrapper)

```csharp
using ImmPlayer;

// Using the high-level wrapper (handles init/cleanup automatically)
using (var doc = new StrokeReaderDocument())
{
    if (doc.Load("path/to/file.imm"))
    {
        Debug.Log($"Loaded {doc.LayerCount} layers");

        for (int l = 0; l < doc.LayerCount; l++)
        {
            if (doc.GetLayerInfo(l, out StrokeLayerInfo info))
            {
                Debug.Log($"Layer: {info.name}");

                for (int d = 0; d < doc.GetDrawingCount(l); d++)
                {
                    for (int s = 0; s < doc.GetStrokeCount(l, d); s++)
                    {
                        StrokePoint[] points = doc.GetStrokePoints(l, d, s);
                        if (points != null)
                        {
                            // Use points...
                        }
                    }
                }
            }
        }
    }
} // Automatically unloads on dispose
```

### Test Component

Attach `ImmStrokeReaderTest` component to any GameObject and set the `immFilePath` field to test loading. Use context menu options to load, inspect, and unload documents.
