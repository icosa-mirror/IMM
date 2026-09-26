IMM Stroke Export Plan
======================

Goal
----
Add a separate "export IMM brush strokes" path that reuses existing IMM
decode logic without changing playback behavior. The new path should
expose raw stroke data (per-point attributes) to Unity via a C API,
while the existing player continues to load/render IMM as-is.

Key Constraints
---------------
- No duplication of IMM parsing logic.
- Default playback behavior must remain unchanged.
- New export path can be called independently of playback.

High-Level Approach
-------------------
1) Add an optional stroke collector interface to libImmImporter.
2) Thread the collector through the importer call stack to the paint
   layer decode path.
3) In the paint decode loop, forward each decoded Element (stroke) to
   the collector if present.
4) Provide a new importer entry point that supplies a collector and
   gathers stroke data into a simple, exportable buffer.
5) Expose a C API for Unity to query stroke counts and point data.

Touchpoints (Existing Code)
---------------------------
- Import entry: code/libImmImporter/src/fromImmersive/fromImmersive.cpp
- Layer load: code/libImmImporter/src/fromImmersive/fromImmersiveLayer.cpp
- Paint decode: code/libImmImporter/src/fromImmersive/fromImmersiveLayerPaint.cpp
- Stroke data model: code/libImmImporter/src/document/layerPaint/element.h

Proposed Additions
-----------------
1) Collector interface (new header)
   - Location: code/libImmImporter/src/fromImmersive/strokeCollector.h
   - Purpose: Optional sink for decoded strokes.
   - Suggested API:
     - OnBeginLayer(layerId, layerName, transform)
     - OnBeginDrawing(layerId, drawingId)
     - OnStroke(brushType, visibilityMode, pointCount, points[])
     - OnEndDrawing(...)
     - OnEndLayer(...)

2) Collector threading
   - Add an optional pointer parameter to:
     - ImportFromDisk / ImportFromMemory
     - fiLayer::LoadAsset
     - fiLayerPaint::ReadDrawing
   - Default: nullptr (no collection; playback unchanged).

3) Stroke capture hook
   - In fiLayerPaint::ReadDrawing, after Element points are decoded:
     - if (collector) collector->OnStroke(...)
   - No change to geometry build or existing flow.

4) Export entry point
   - New function: ImportStrokesFromDisk(...)
   - Internally calls ImportFromDisk with a collector that writes
     into a compact export buffer.

5) Unity-facing C API
   - Minimal queries:
     - GetLayerCount()
     - GetLayerName(layerIndex)
     - GetDrawingCount(layerIndex)
     - GetStrokeCount(layerIndex, drawingIndex)
     - GetStrokePointCount(layerIndex, drawingIndex, strokeIndex)
     - GetStrokePointData(..., out arrays for pos/nor/dir/col/alpha/width)
     - GetStrokeBrushType(...), GetStrokeVisibility(...)

TODO List
---------
1) Define collector interface
   - Add new header with a pure-virtual class or C-style callbacks.
2) Thread optional collector through importer API
   - Update function signatures and call sites with default nullptr.
3) Implement stroke capture hook
   - In ReadDrawing, forward Element data to collector if set.
4) Build export buffer
   - Implement a concrete collector that stores layers/drawings/strokes.
5) Add C API wrappers
   - Expose exporter buffer queries for Unity interop.
6) Validate on sample IMM
   - Confirm counts match expected stroke/drawing totals.
7) Document usage
   - Add brief notes on calling export path vs playback path.
