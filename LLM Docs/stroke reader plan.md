# IMM Stroke Reader Unity Plugin Plan

## Goal
Create a separate Unity plugin (`ImmStrokeReader.dll`) that reads IMM files and exposes raw stroke data (positions, colors, widths, etc.) to Unity via a C API. This is independent from the rendering plugin.

## Architecture Overview

```
ImmStrokeReader.dll (new)
├── libImmCore.lib (math, logging, memory)
├── libImmImporter.lib (document parsing)
└── zlib.lib (decompression - required by importer)
```

No rendering, no audio, no player - just document loading and stroke data access.

## Implementation Approach

### Option A: Stroke Collector Interface (from imm_stroke_export_plan.md)
Thread a collector through the import path to capture stroke data during decode.

**Pros:** Reuses existing decode logic, no duplication
**Cons:** Requires modifying libImmImporter signatures, more invasive

### Option B: Direct File Reading (Recommended)
Create a standalone stroke reader that uses libImmImporter's existing decode functions but stores raw data instead of tessellating.

**Pros:** No changes to existing importer, cleaner separation
**Cons:** Some code structure similarity to importer

### Chosen: Hybrid Approach
1. Add a simple stroke collector interface to libImmImporter
2. Hook it at the point where Elements are decoded (fromImmersiveLayerPaint.cpp:509)
3. New plugin calls import with collector to gather stroke data
4. Expose data via C API

## Key Files to Modify/Create

### New Files
1. `libImmImporter/src/fromImmersive/strokeCollector.h` - Collector interface
2. `appImmStrokeReader/appImmStrokeReader.vcxproj` - New project
3. `appImmStrokeReader/src/main.cpp` - C API exports
4. `appImmStrokeReader/src/strokeStore.h/.cpp` - Storage for collected strokes
5. `projects/windows/ImmStrokeReaderPropertySheet.props` - Minimal dependencies

### Modified Files
1. `libImmImporter/src/fromImmersive/fromImmersive.h` - Add collector parameter
2. `libImmImporter/src/fromImmersive/fromImmersive.cpp` - Thread collector through
3. `libImmImporter/src/fromImmersive/fromImmersiveLayer.cpp` - Pass collector
4. `libImmImporter/src/fromImmersive/fromImmersiveLayerPaint.cpp` - Call collector at line 509
5. `projects/windows/imm.sln` - Add new project

## Detailed Design

### 1. Stroke Collector Interface (`strokeCollector.h`)

```cpp
namespace ImmImporter {

struct CollectedPoint {
    float px, py, pz;      // position
    float nx, ny, nz;      // normal
    float dx, dy, dz;      // view direction
    float r, g, b;         // color
    float alpha;           // transparency
    float width;           // stroke width
};

class IStrokeCollector {
public:
    virtual ~IStrokeCollector() = default;
    virtual void OnBeginLayer(uint32_t layerId, const wchar_t* name) = 0;
    virtual void OnBeginDrawing(uint32_t drawingId) = 0;
    virtual void OnStroke(
        uint32_t strokeId,
        uint8_t brushType,
        uint8_t visibilityMode,
        uint32_t numPoints,
        const Point* points,
        const bound3& bbox
    ) = 0;
    virtual void OnEndDrawing() = 0;
    virtual void OnEndLayer() = 0;
};

} // namespace ImmImporter
```

### 2. Import API Changes

```cpp
// fromImmersive.h - add overloads with collector
bool ImportFromDisk(
    Sequence* sq, piLog* log, const wchar_t* filename,
    Drawing::ColorSpace colorSpace,
    Drawing::PaintRenderingTechnique renderingTechnique,
    IStrokeCollector* collector = nullptr  // NEW
);

bool ImportFromMemory(
    piTArray<uint8_t>* data, Sequence* sq, piLog* log,
    Drawing::ColorSpace colorSpace,
    Drawing::PaintRenderingTechnique renderingTechnique,
    IStrokeCollector* collector = nullptr  // NEW
);
```

### 3. Hook Point in fromImmersiveLayerPaint.cpp

At line 509, after `ele.Compute(biggestStroke)` and before `dr->Add()`:

```cpp
ele.Compute(biggestStroke);

// NEW: Forward to collector if present
if (collector) {
    collector->OnStroke(
        j,                      // strokeId
        static_cast<uint8_t>(ele.GetBrush()),
        static_cast<uint8_t>(ele.GetVisibleMode()),
        ele.GetNumPoints(),
        ele.GetPoints(),
        ele.GetBBox()
    );
}

dr->Add(&ele, colorSpace, flipped);
```

### 4. Unity Plugin C API (`appImmStrokeReader/src/main.cpp`)

```cpp
// Lifecycle
extern "C" int UNITY_INTERFACE_EXPORT StrokeReader_Init(char* logFileName);
extern "C" void UNITY_INTERFACE_EXPORT StrokeReader_End();

// Loading
extern "C" int UNITY_INTERFACE_EXPORT StrokeReader_LoadFromFile(char* fileName);
extern "C" int UNITY_INTERFACE_EXPORT StrokeReader_LoadFromMemory(void* data, int size);
extern "C" void UNITY_INTERFACE_EXPORT StrokeReader_Unload(int docId);

// Querying
extern "C" int UNITY_INTERFACE_EXPORT StrokeReader_GetLayerCount(int docId);
extern "C" bool UNITY_INTERFACE_EXPORT StrokeReader_GetLayerInfo(int docId, int layerIdx, StrokeLayerInfoC* info);
extern "C" int UNITY_INTERFACE_EXPORT StrokeReader_GetDrawingCount(int docId, int layerIdx);
extern "C" int UNITY_INTERFACE_EXPORT StrokeReader_GetStrokeCount(int docId, int layerIdx, int drawingIdx);

// Stroke data access
extern "C" bool UNITY_INTERFACE_EXPORT StrokeReader_GetStrokeInfo(
    int docId, int layerIdx, int drawingIdx, int strokeIdx,
    StrokeInfoC* info  // out: brushType, visMode, numPoints, bbox
);
extern "C" bool UNITY_INTERFACE_EXPORT StrokeReader_GetStrokePoints(
    int docId, int layerIdx, int drawingIdx, int strokeIdx,
    StrokePointC* points,  // out: array, caller allocates
    int maxPoints
);
```

### 5. C Structs for Unity Interop

```cpp
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
    float px, py, pz;
    float nx, ny, nz;
    float dx, dy, dz;
    float r, g, b;
    float alpha;
    float width;
};
```

## Implementation Order

1. **Create strokeCollector.h** interface in libImmImporter
2. **Modify import path** to thread collector through (default nullptr = no change)
3. **Add hook** in fromImmersiveLayerPaint.cpp
4. **Create appImmStrokeReader project** with minimal deps
5. **Implement StrokeStore** class that implements IStrokeCollector
6. **Implement C API** exports
7. **Test** with sample IMM file

## Verification

1. Build appImmStrokeReader.dll successfully
2. Load a test IMM file via StrokeReader_LoadFromFile
3. Query layer/stroke counts and verify they match expected values
4. Extract point data and verify positions/colors are reasonable
5. Test from Unity C# with P/Invoke

## Platform Support

The plugin code is platform-agnostic (no rendering, no audio, no platform APIs). The same C++ source compiles for all platforms. Only build configuration differs:

- **Windows:** vcxproj (start here for development/testing)
- **Android:** Add to existing Android.mk or CMakeLists
- **macOS/iOS:** Xcode project or CMake

## Dependencies for New Plugin

- libImmCore (cross-platform)
- libImmImporter (cross-platform)
- zlib (cross-platform, required by importer for decompression)

## Notes

- The collector is called during the normal import process, so stroke data is captured without duplicating decode logic
- The existing rendering path is unchanged when collector is nullptr
- Point data uses the same `Point` struct from libImmImporter, converted to C-friendly struct at API boundary

---

## Implementation Status (as of Feb 3, 2026)

### ✅ COMPLETED

All core C++ implementation is complete and committed:

| Component | File(s) | Status |
|-----------|---------|--------|
| Stroke Collector Interface | `libImmImporter/src/fromImmersive/strokeCollector.h` | ✅ Created |
| Import API Changes | `fromImmersive.h`, `fromImmersive.cpp` | ✅ Modified |
| Layer Paint Hook | `fromImmersiveLayerPaint.cpp:509-520` | ✅ Added |
| Stroke Store | `appImmStrokeReader/src/strokeStore.h/.cpp` | ✅ Created |
| C API Exports | `appImmStrokeReader/src/main.cpp` | ✅ Created |
| VS Project | `appImmStrokeReader/appImmStrokeReader.vcxproj` | ✅ Created |
| Windows DLL | `exe/ImmStrokeReader.dll` (313 KB) | ✅ Built & committed |
| .gitignore | Updated for appImmStrokeReader | ✅ Committed |
| Documentation | `appImmStrokeReader/README.md` | ✅ Created |

**Commit:** `cf02e48` - "wip" (15 files, 990 insertions)
**Follow-up:** `fd86551` - "Basic demo of stroke data extraction in Unity"

### 📋 NEXT STEPS

1. [ ] **Unity UPM packaging** - Extract the stroke reader into a Unity package for reuse
2. [ ] **Unity C# wrapper (P/Invoke bindings)** - Verify/demo bindings from the Unity demo commit; add missing APIs if needed
3. [ ] **Unity integration testing** - Expand the Unity demo beyond the basic stroke extraction test
4. [ ] **Android build configuration** - Add to Android.mk or CMakeLists.txt
5. [ ] **macOS/iOS build configuration** - Xcode project or CMake

## Unity UPM Packaging Notes

Follow the structure used in `/Users/andrewbaker/Documents/GitHub/IMM-unity/Packages/com.immersive-foundation.imm-unity`:

```
code/ImmUnitySampleProject/Packages/com.immersive-foundation.imm-stroke-reader/
├── package.json
├── README.md
├── Plugins/
├── Runtime/
└── Samples~/
```

Recommended package metadata:

```json
{
  "name": "com.immersive-foundation.imm-stroke-reader",
  "version": "0.1.0",
  "displayName": "IMM Stroke Reader",
  "description": "IMM stroke reader runtime and samples.",
  "unity": "2021.3",
  "author": {
    "name": "Immersive Foundation"
  },
  "samples": [
    {
      "displayName": "Stroke Reader Samples",
      "description": "Example scripts for IMM stroke reader.",
      "path": "Samples~/Examples"
    }
  ]
}
```

Plugin placement:

- Native binaries go under `Plugins/` with platform subfolders (Windows/macOS/Android).
- C# bindings and runtime scripts go under `Runtime/`.
- Unity demo scene/scripts go under `Samples~/Examples`.

Second UPM package:

- The primary IMM Unity plugin package already exists in `/Users/andrewbaker/Documents/GitHub/IMM-unity/Packages/com.immersive-foundation.imm-unity` and serves as the reference template for layout and metadata.

### C API Reference

**Lifecycle:**
- `StrokeReader_Init(char* logFileName)` → int
- `StrokeReader_End()` → void
- `StrokeReader_IsInitialized()` → bool

**Loading:**
- `StrokeReader_LoadFromFile(char* fileName)` → int (docId)
- `StrokeReader_LoadFromMemory(void* data, int size)` → int (docId)
- `StrokeReader_Unload(int docId)` → void
- `StrokeReader_GetDocumentCount()` → int

**Query:**
- `StrokeReader_GetLayerCount(int docId)` → int
- `StrokeReader_GetLayerInfo(int docId, int layerIdx, StrokeLayerInfoC* info)` → bool
- `StrokeReader_GetDrawingCount(int docId, int layerIdx)` → int
- `StrokeReader_GetStrokeCount(int docId, int layerIdx, int drawingIdx)` → int
- `StrokeReader_GetStrokeInfo(...)` → bool
- `StrokeReader_GetStrokePoints(...)` → bool
