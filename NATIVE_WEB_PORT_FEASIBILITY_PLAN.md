# Native Web Port Feasibility and Implementation Plan

**Status:** Phase 3 completed; authored viewpoints and initial WebXR presentation implemented
**Date:** 2026-07-13  
**Target:** a browser-native IMM viewer, using JavaScript/TypeScript, WebAssembly, or a mixture of the two; Three.js is an acceptable rendering framework.

## Executive decision

A native web port is feasible. The recommended design is a **hybrid port**:

- Compile a deliberately small subset of the existing C++ importer to WebAssembly. Reuse the binary scene parser, timeline parser, paint decompression, quantization, prediction, and stroke reconstruction code.
- Run that Wasm decoder in a dedicated Web Worker.
- Implement file/network I/O, lifecycle, playback scheduling, rendering, audio, UI, and WebXR in TypeScript.
- Use Three.js with `WebGLRenderer` as the first rendering target. Use custom `BufferGeometry` and shader materials for paint rather than trying to compile the native renderer into Wasm.
- Treat WebGPU as a later optimization target, not as an MVP dependency.
- Ship two delivery profiles over the same decoder/runtime: a self-contained player and an embeddable Three.js scene adapter. The self-contained player should consume the adapter rather than becoming an independent renderer unless profiling or visual parity proves that impossible.

This approach preserves the code where format correctness is difficult to reproduce while replacing platform-specific systems with browser-native equivalents. It also avoids making cross-origin isolation and Wasm pthreads prerequisites for the initial release.

The alternatives are technically possible but have worse risk/reward:

| Option | Feasibility | Initial effort | Main issue | Recommendation |
|---|---:|---:|---|---|
| Pure TypeScript/JavaScript | Medium | High | Reimplements the least documented and most error-prone binary and paint decoding logic | Do not start here |
| Entire C++ player compiled to Wasm | Medium | High | Native renderer, sound, file, window, thread, and host abstractions do not map cleanly to the browser | Reject as the primary design |
| C++ importer in Wasm + browser runtime | High | Medium | Requires a purpose-built flat data boundary and a new renderer | Recommended |
| Pure TypeScript after a hybrid reference exists | High later | Medium later | Still duplicates the decoder, but can be verified against Wasm | Optional long-term simplification |

The first useful target should be **desktop monoscopic playback of paint and picture layers from a local file or full HTTP download**, validated against `exampleImmFiles/sample1.imm`. WebXR, streaming ranges, and full audio behavior should follow after visual and timeline correctness are established.

For integration into an existing Three.js application, the preferred contract is **one renderer, one canvas, one camera/XR session, and one depth buffer**. An IMM document should appear to the host as an `Object3D`-like scene subtree plus a small update/dispose API. Stacking or copying two independently rendered canvases is retained only as a limited 2D composition fallback.

## What was examined

The recommendation is based on the current repository rather than only on the high-level README.

### Format and scene import

- `code/libImmImporter/src/fromImmersive/fromImmersive.cpp`
  - Parses the top-level chunked format.
  - Recognizes `Immersiv`, `CoordSys`, `Category`, `Sequence`, and `ResTable` chunks.
  - Requires file version `0x00010001`.
  - Builds an asset table of 64-bit offsets and sizes.
  - Loads assets in first-needed order, with a five-second initial buffer in the native path.
- `code/libImmImporter/src/fromImmersive/fromImmersiveLayer.cpp`
  - Parses the recursive layer tree and timelines.
  - Supports group, paint, model, picture, sound, and spawn-area layer descriptors.
  - Parses visibility, opacity, transform, draw-in, action, loop, and offset animation keys.
- `code/libImmImporter/src/fromImmersive/fromImmersiveLayerPaint.cpp`
  - Contains the high-risk paint decoder.
  - Uses zlib plus project-specific bit interlacing, predictors, quantization, and transforms.
  - Produces strokes made of point position, orientation, view direction, color, transparency, quantized width, length, and time.
- `code/libImmImporter/src/fromImmersive/fromImmersiveLayerPicture.cpp`
  - Reads PNG and JPEG blobs.
- `code/libImmImporter/src/fromImmersive/fromImmersiveLayerSound.cpp`
  - Reads WAV, OGG/Vorbis, and Opus assets.
- `code/libImmImporter/src/fromImmersive/fromImmersiveLayerModel.cpp`
  - Parses a version field but `ReadAsset()` currently returns `true` without reading mesh data. Model playback is therefore not an existing working capability to port.

### Runtime and rendering

- `code/libImmPlayer/src/player.*`, `document.*`, and `mngrPlayer.*`
  - Implement loading state, playback state, chapters, visibility, transforms, timing, and per-frame traversal.
  - Couple those behaviors to native renderer and sound interfaces.
- `code/libImmPlayer/src/layerRenderers/layerRendererPaint/`
  - Implements static and pretessellated paint paths.
  - Batches geometry by five brush section types and caps native chunks at 65,536 vertices for 16-bit indices.
  - Uses custom shaders for width expansion, directional visibility, color conversion, animation parameters, and alpha-to-coverage.
- `code/libImmPlayer/src/layerRenderers/layerRendererPicture/`
  - Supports 2D pictures, mono/stereo equirectangular 360 images, and two cubemap layouts.
- `code/libImmPlayer/src/layerRenderers/layerRendererSound/`
  - Delegates playback and spatialization to native sound backends.

### Existing extraction boundary

`code/appImmStrokeReader/` and `IStrokeCollector` already demonstrate that the importer can be separated from the renderer. They expose decoded paint strokes, pictures, transforms, drawing/frame mappings, and chapter information through a C-friendly API.

This is useful prior art for a Wasm boundary, but it is not sufficient as the final web API:

- It stores all decoded content in nested C++ vectors and then exposes it through many small query calls. That creates excessive JS/Wasm crossing overhead.
- It copies picture pixels after decoding rather than preserving compressed image blobs for browser-native decoding.
- It does not export the complete hierarchy and every animation key required for general playback parity.
- It does not expose sound asset bytes and metadata.
- Its synchronous collector path loads the entire document rather than supporting asset-by-asset range loading.

The web port should reuse the separation idea, not freeze the current API as its data contract.

### Test evidence available in the repository

- `exampleImmFiles/sample1.imm` is 5,831,101 bytes and already has content and render baseline contracts.
- `tests/baselines/render/windows-directx-sample1.ppm` is the principal committed visual reference.
- Existing runtime evidence for this fixture reports 38 draw calls, including 37 paint calls and one 360 equirectangular picture, and approximately 645,802 triangles. This is large enough to expose geometry, batching, orientation, and performance mistakes in an initial web implementation.
- `tests/tools/compare_render_metrics.py` and the baseline JSON files provide reusable image-level parity checks.

## Scope and definition of “native web port”

The port should be a normal web application that can be hosted as static assets and run without Unity, Godot, a native executable, or a server-side transcoder. Wasm counts as browser-native execution for the decoder. The content must still be rendered by browser graphics APIs and played through browser audio APIs.

### MVP scope

- Load an `.imm` from a file picker, drag-and-drop, or a same-origin URL.
- Parse scene metadata and assets in the browser.
- Render static paint with all five brush section types.
- Render the `sample1.imm` 360 background and paint scene with correct transforms, camera orientation, color space, depth behavior, and reasonable alpha edge quality.
- Support layer hierarchy, initial visibility/opacity, frame-to-drawing mapping, play, pause, seek, restart, and chapter selection.
- Provide an orbit/fly desktop camera.
- Report load/decode/build/render timing and memory diagnostics.
- Run parsing and geometry preparation off the browser main thread.

### Subsequent parity scope

- Animated transforms, visibility, opacity, actions, loops, offsets, keep-alive effects, and draw-in behavior.
- 2D pictures, stereo equirectangular images, cross cubemaps, and vertical-strip cubemaps.
- WAV, OGG/Vorbis, and Opus audio; flat, positional, and ambisonic behavior.
- Spawn areas and WebXR presentation.
- HTTP range loading and time-prioritized asset streaming.
- Cancellation, unloading, bounded memory, recovery from malformed files, and production telemetry.

### Explicit non-goals for the first release

- Exporting or editing IMM files.
- Reusing the native GL/DX/Vulkan/Metal renderer in the browser.
- Pixel-identical emulation of every native backend.
- A model-layer promise. The current importer does not deserialize model assets, so a model feature needs a separate format definition and implementation.
- WebGPU-only support.
- Wasm pthreads or shared memory as an initial requirement.

## Why the hybrid split is the best fit

### Code that should remain C++/Wasm initially

- Top-level chunk recognition and version validation.
- Scene graph and timeline deserialization.
- Asset table interpretation.
- Paint layer headers, drawing tables, and frame maps.
- zlib inflation of paint payloads.
- Bit interlacing, predictors, inverse transforms, quantization, and reconstruction of `Element`/`Point` data.
- Canonical transform, time tick, interpolation, and chapter semantics where practical.
- Optional web-oriented geometry packing once the raw-stroke path is proven.

These areas contain compact, deterministic logic and are directly testable against the native importer.

### Code that should be browser-native

- `fetch`, file-picker, drag-and-drop, and HTTP Range behavior.
- Worker lifecycle and cancellation.
- Playback clock integration with `requestAnimationFrame` and `AudioContext`.
- Three.js scene objects, camera, materials, texture creation, resource disposal, and WebXR.
- Image decode using `createImageBitmap`, `ImageDecoder` where appropriate, or a browser image element fallback.
- Audio decode/playback and Web Audio spatial nodes.
- UI, controls, loading progress, error surfaces, and accessibility.
- Cache/storage policy.

The native window, renderer, VR, sound engine, platform file code, and detached C++ loader thread should not enter the Wasm dependency closure.

## Proposed architecture

```text
File / URL
    |
    v
TypeScript source adapter
  - File slices
  - full fetch for MVP
  - HTTP Range fetch later
    |
    | transferable ArrayBuffer
    v
Decoder Web Worker
  +-----------------------------+
  | single-thread Emscripten    |
  | Wasm decoder                |
  | - scene/timeline parser     |
  | - paint decompressor        |
  | - flat output builders      |
  +-----------------------------+
    |
    | transferable typed buffers + descriptors
    v
TypeScript IMM runtime
  - document/layer model
  - playback clock/state machine
  - asset residency and cancellation
  - browser audio scheduler
    |
    v
Three.js adapter
  - paint BufferGeometry/custom shaders
  - image/360 materials
  - desktop camera and WebXR
```

Suggested repository layout when implementation begins:

```text
code/projects/web/
  CMakeLists.txt                 # Emscripten decoder build
  decoder/
    imm_web_decoder.cpp          # stable C ABI; no renderer dependencies
    imm_web_output.h             # packed records and versioned schema
    imm_web_log.cpp
  app/
    package.json
    vite.config.ts
    src/
      decoder-worker.ts
      decoder-client.ts
      format/
      runtime/
      render-three/
      audio/
      xr/
      ui/
    tests/
```

The exact folder may change, but the decoder and web application should remain independently testable.

## Standalone player and Three.js integration architecture

There are two useful products, but they do not initially require two rendering implementations.

### Shared core

Both products should depend on the same packages:

```text
@imm/decoder-wasm
  - worker and Wasm decoder
  - flat document/asset output

@imm/runtime
  - document and timeline state
  - loading/residency
  - playback clock and audio scheduling
  - no DOM or renderer ownership

@imm/three
  - turns runtime layers into Three.js Object3D/BufferGeometry/materials
  - accepts a host renderer/camera
  - owns only the resources it creates
```

### Product A: self-contained player

The standalone viewer owns its canvas, `WebGLRenderer`, scene, camera controls, UI, audio context, and optional WebXR button. Internally it uses `@imm/three` exactly as an embedding application would.

Advantages:

- smallest maintenance surface;
- standalone and embedded rendering cannot silently diverge;
- the standalone application becomes the reference example for integrators;
- most tests run against the same adapter used in production integrations.

The standalone viewer can still select more aggressive defaults, such as a dedicated render loop, known MSAA mode, fixed color management, and an IMM-focused residency budget.

### Product B: embeddable Three.js adapter

The host supplies its renderer, scene parent, camera, frame time, and optional audio/XR context. A proposed API shape is:

```ts
const document = await immLoader.load(source, { signal });
const view = new ImmThreeView(document, {
  renderer,
  audioContext,
  renderMode: "shared-scene",
});

hostScene.add(view.object3d);

function frame(timeSeconds: number) {
  view.update(timeSeconds, hostCamera);
  renderer.render(hostScene, hostCamera);
}

view.dispose();
document.dispose();
```

The adapter must not create a second animation loop, change renderer-wide settings without restoring them, clear the framebuffer, or assume it owns the camera. It must document requirements for color space, tone mapping, MSAA, clipping, and supported Three.js versions.

This gives real integration rather than visual overlay:

- normal Three.js objects can appear in front of or behind IMM strokes;
- depth testing and occlusion work across both content sets;
- the host camera and controls apply to IMM;
- one WebXR session renders both scenes;
- raycasting, layer masks, post-processing, and render ordering can be integrated deliberately;
- there is no extra full-screen canvas composite on mobile.

### Optional Product C: dedicated standalone renderer

A raw WebGL/WebGPU standalone renderer should be considered only if the shared Three.js renderer fails an agreed gate, for example:

- it cannot reproduce acceptable paint alpha/depth behavior on target devices;
- Three.js CPU/draw-call overhead materially misses the headset or mobile frame budget;
- a storage-buffer/WebGPU representation yields a necessary, measured memory reduction that the adapter cannot expose;
- strict standalone visual behavior conflicts with safe host-renderer state ownership.

If needed, this renderer would still share `@imm/decoder-wasm` and `@imm/runtime`. It would be a second renderer, not a second format port. The embedded Three.js adapter could accept reduced parity or performance on the documented cases.

### Canvas composition alternatives

| Composition method | Mobile cost | Shared depth/XR | Suitability |
|---|---:|---:|---|
| IMM objects in the host Three.js scene | Lowest practical | Yes | Recommended |
| IMM rendered by the same Three.js renderer into a render target, then composited | Moderate | Possible but must be designed | Useful for post-processing or an intentionally flattened IMM layer |
| Transparent IMM canvas stacked over the host canvas | Moderate-to-high full-screen composition cost | No | Acceptable only for HUD/background-like separation |
| Upload/copy an IMM canvas into a Three.js `CanvasTexture` every frame | High bandwidth and texture-upload cost | No | Avoid for animated/mobile use |
| Two renderers sharing one WebGL context | Fragile state ownership | Potentially | Avoid unless a dedicated integration proves safe |
| Two independent WebXR canvases/sessions | Not a workable composition model | No | Reject |

Two stacked canvases are easy to prototype in DOM terms, but they cannot correctly resolve per-pixel depth between IMM and host geometry. They also duplicate canvas resolution, clearing, render-loop coordination, and compositing work. On high-DPI phones that extra full-screen bandwidth can cost more than the IMM draw calls themselves. Use it only when the desired composition is inherently flat, such as “IMM always behind the application” or “IMM always over it.”

Rendering to a Three.js render target is the middle option. It keeps one WebGL context and lets the host post-process or blend IMM as a texture. It is appropriate when flattening is intentional. It is not a general substitute for shared-scene rendering because ordinary host geometry cannot intersect the flattened IMM result without a shared/composited depth design.

### Integration rules that need to be part of the public contract

- The host owns `renderer.render()` and the animation loop in embedded mode.
- IMM updates accept an explicit time; they do not read wall-clock time implicitly.
- Every created geometry, texture, material, worker, and audio node has deterministic disposal.
- IMM uses an `Object3D` root so the host can position, scale, hide, mask, and remove the document.
- Materials expose render-order and depth-policy hooks without permitting arbitrary per-stroke mutation.
- The adapter detects incompatible renderer settings and reports them rather than silently changing them.
- Post-processing integrations receive an explicit render-pass option; support for a particular composer is separate from basic scene integration.
- WebXR uses the host renderer’s existing XR manager and reference space.
- Multiple IMM documents can coexist without global singleton decoder/player state.
- The supported Three.js revision range is pinned and tested because renderer/material internals change over time.

## Wasm decoder design

### Build a narrow source closure

Do not compile `libImmCore` and `libImmImporter` wholesale. Create an Emscripten target with only the sources that the memory importer and paint decoder require. Likely inputs include:

- importer document and `fromImmersive` sources;
- basic array/string/stream/tick/math types;
- basic paint compression sources;
- zlib;
- a web log shim;
- a web output collector.

Exclude native renderer, mesh renderer, sound engine, wave encoders, VR, window, OS file, timer, and thread backends. Avoid PNG/JPEG/Vorbis/Opus libraries in the first decoder target by returning their compressed blobs to JavaScript. This materially reduces binary size, legal surface, build complexity, and peak decode memory.

Some existing headers pull renderer or VR types into otherwise renderer-neutral document classes. The spike should first remove or forward-declare those accidental dependencies. If that becomes invasive, introduce small format-facing structs rather than carrying platform headers into the Wasm build.

### Use a versioned, bulk data contract

Do not expose a query-per-point interface. A large film can contain enough strokes and points for thousands or millions of JS-to-Wasm calls to dominate decode time.

The decoder should produce flat, typed tables such as:

- `DocumentRecord`: format version, sequence type, capabilities, frame rate, background color, root ID, initial spawn area.
- `LayerRecord[]`: ID, parent ID, type, flags, name offset, asset ID, local transform, pivot, opacity, duration, repeat count, and animation ranges.
- `AnimationKeyRecord[]`: layer ID/property, tick, interpolation, and typed value payload.
- `AssetRecord[]`: type, format, source offset/size, decoded table ranges, and residency state.
- `PaintLayerRecord[]`: drawing range, frame-map range, frame rate, repeat count.
- `DrawingRecord[]`: stroke range, bounds, biggest-stroke scale, file offset.
- `StrokeRecord[]`: brush type, visibility mode, point range, bounds.
- Structure-of-arrays point buffers for positions, normals/orientation, authored view direction, color/alpha, width, length, and time.
- `PictureRecord[]`: content type, viewer lock, compressed format, blob range.
- `SoundRecord[]`: sound type, loop/play flags, gain, duration, attenuation, directional modifier, channel metadata, compressed format, blob range.
- `SpawnAreaRecord[]`: tracking level, sphere/box, allowed translation axes, screenshot blob range.

Use explicit fixed-width fields, offsets, lengths, and a schema version. Do not expose compiler-dependent C++ object layouts, pointers, `std::vector`, `wchar_t`, or virtual objects to JavaScript.

### Boundary mechanics

Recommended first implementation:

1. Transfer the input `ArrayBuffer` to the decoder worker.
2. Copy it once into Wasm linear memory.
3. Call a C ABI such as `imm_web_decode_document(ptr, size, options, result)`.
4. Build flat output blocks in Wasm.
5. Copy or transfer web-owned typed buffers out of Wasm in coarse blocks.
6. Release the input and temporary Wasm allocations as soon as output ownership is established.

Avoid Embind objects for the core data plane. A small generated JS wrapper around a C ABI gives better control over allocation, errors, and compatibility.

The API must include:

- limits and validation options;
- an atomic or callback cancellation flag;
- structured error code, byte offset, chunk/layer context, and message;
- progress by phase and bytes/assets processed;
- explicit destroy/reset calls;
- schema and decoder build IDs.

### Single-thread first

Compile the decoder without Emscripten pthreads and place the entire module in one Web Worker. This keeps heavy synchronous C++ work off the UI thread while avoiding `SharedArrayBuffer`, COOP, and COEP deployment requirements.

Emscripten pthreads are available but require cross-origin isolation headers and can introduce main-thread proxying and blocking hazards. They should be considered only after profiling shows that one decoder worker is inadequate. Large typed buffers can already move between the worker and main thread using transferable `ArrayBuffer` ownership.

### Decoder hardening

The current importer trusts many lengths, IDs, offsets, and enum values. Browser delivery increases exposure to arbitrary untrusted files. Before public release, the web decoder must reject:

- chunks or assets whose `offset + size` overflow or exceed the source;
- unreasonable layer, key, drawing, stroke, point, frame, image, or audio counts;
- recursion/hierarchy cycles and excessive depth;
- strings without a valid bounded length;
- invalid asset references and drawing IDs;
- zlib bombs and decoded sizes beyond configured budgets;
- NaN/infinite transforms, colors, widths, times, and bounds;
- unsupported format versions and enums.

Fuzz the memory importer under native ASan/UBSan and, separately, the Wasm build. A malformed file should return an error without hanging, growing memory without bound, or crashing the worker repeatedly.

## Rendering plan with Three.js

### Initial backend

Use Three.js `WebGLRenderer`, which now targets WebGL 2. Three.js supplies scene/camera integration, resource management, WebXR plumbing, and typed `BufferGeometry`, while custom shader materials preserve IMM paint behavior.

Do not translate the native shader verbatim. The desktop shader reads point data through shader storage buffers, which WebGL 2 does not expose as the same general facility. The web representation needs to be designed for WebGL 2.

### Paint geometry path

For the first parity implementation:

1. Decode canonical stroke points in Wasm.
2. Build web-ready indexed geometry either in the worker TypeScript code or, preferably after correctness is proven, in a small C++ web packer.
3. Preserve batching by drawing and brush section type.
4. Split batches to remain compatible with index and practical buffer limits; the native 65,536-vertex chunk rule is a safe starting point.
5. Upload coarse interleaved attributes into Three.js `BufferGeometry`.
6. Use a custom shader material for layer opacity, color behavior, authored directional visibility, width scale, and optional keep-alive/draw-in parameters.

Brush topology to preserve:

| Brush type | Native section | Web MVP representation |
|---|---|---|
| Point | 2 vertices per point | camera/view-oriented narrow ribbon or native-equivalent paired section |
| Segment | 2 vertices per point | indexed ribbon strip |
| Circle | 7 vertices per point | seven-sided tube |
| Ellipse | 7 vertices per point | flattened seven-sided tube |
| Square | 4 vertices per point | four-sided tube |

The existing C++ computes a tangent and two basis vectors per point, applies `1.7 * biggestStroke * width / 32767`, handles duplicated end points, and reverses topology for flipped transforms. Those rules must be captured in golden geometry tests rather than inferred again in shaders.

An optimized later path can store canonical points in float/integer textures and use `gl_VertexID` for shader expansion, or use WebGPU storage buffers. It should only replace the expanded-geometry path after memory and frame profiles justify the complexity.

### Alpha, depth, and color

Paint parity is more difficult than drawing the tubes:

- Native paint uses custom stochastic alpha-to-coverage with blue noise and writes opaque output color while controlling sample coverage.
- Three.js materials expose `alphaToCoverage` on MSAA-enabled WebGL contexts, but exact native sample-mask behavior depends on the optional WebGL `OES_sample_variables` extension.
- Transparent blending alone is not an equivalent fallback because stroke self-ordering and depth writes differ.

Implement and compare these modes during the rendering spike:

1. MSAA plus Three.js `alphaToCoverage`, depth writes enabled.
2. `OES_sample_variables` custom shader path when available.
3. Blue-noise/alpha-hash discard fallback for devices without sample variables.

Select the default using the committed render metric comparison and close-up edge captures. Expose the chosen fallback in diagnostics so platform differences are explainable.

Color handling also needs an explicit contract. The native static geometry path transforms colors differently according to requested color space and platform. The web port should define:

- whether decoded point colors are canonical linear values;
- texture color spaces for JPEG/PNG assets;
- renderer output color space;
- whether tone mapping is disabled for reference parity;
- where opacity and directional coverage are applied.

Start with `NoToneMapping`, explicit texture color spaces, and a known sRGB canvas output. Add a unit color ramp and image-level color checks.

### Layer graph and animation

Map each IMM layer to a lightweight runtime node and, where renderable, a Three.js `Group`/`Object3D`. Preserve:

- parent-child order;
- local transform and pivot semantics;
- uniform scale and flip/handedness;
- recursive visibility and opacity;
- timeline ownership and local time offsets;
- interpolation mode;
- action, loop, stop/wait, and chapter behavior;
- frame-map selection of a paint drawing.

Do not update every dormant drawing every frame. Playback should evaluate only active timelines, update changed nodes, and attach/show only the current drawing for an animated paint layer. Geometry may be decoded ahead of time without being resident on the GPU.

### Pictures and 360 content

Keep PNG/JPEG bytes compressed across the Wasm boundary and decode them in the browser. Then implement:

- 2D image: aspect-correct quad in its layer transform.
- Viewer-locked image: attach to a camera-relative group with native-equivalent depth/scale rules.
- Mono equirectangular 360: inward-facing sphere or equirectangular background shader.
- Stereo equirectangular 360: eye-dependent UV selection during WebXR rendering.
- Cross and vertical-strip cubemaps: CPU relayout or shader sampling with the same face mapping and flips used by `layerRendererPicture.cpp`.

The `sample1.imm` fixture contains a mono equirectangular picture, so picture orientation is part of the first visual gate rather than a deferred feature.

### WebXR

Three.js `WebXRManager` supplies per-eye cameras, controller groups, and a default `local-floor` reference space. Add WebXR only after desktop rendering parity because XR can hide camera and handedness errors behind another transform layer.

WebXR acceptance must cover:

- entering/exiting immersive VR after user activation;
- local-floor versus eye-level spawn-area behavior;
- stereo background selection;
- world scale and handedness;
- pause/resume when session visibility changes;
- controller chapter/transport actions;
- Quest-class standalone browser performance.

WebXR availability varies by browser and device, so desktop mono remains a supported mode rather than a fallback used only for development.

## Audio plan

Use Web Audio rather than compiling the native sound engine or the discontinued Facebook Audio360 SDK.

### Asset handling

- Return complete compressed audio blobs and metadata from the decoder.
- Try `AudioContext.decodeAudioData()` for supported WAV, OGG/Vorbis, and Opus container data. It requires complete file data rather than arbitrary fragments.
- Maintain a tested codec support matrix for target browsers; browser support must be detected rather than assumed from file extension.
- If a required codec/container fails on a target browser, add a small decoder Wasm module for that codec or define a server-side/content-pipeline compatibility policy. Do not add all native audio libraries to the main decoder preemptively.

### Playback mapping

- Flat/headlocked: `AudioBufferSourceNode` to the master gain.
- Positional: `PannerNode` plus gain and the imported linear/log attenuation parameters.
- Directional cone: map to panner cone controls where semantics align; otherwise use a custom gain stage.
- Directional frustum: custom orientation-dependent gain calculation.
- Ambisonic: separate proof of concept. Determine channel order/normalization from representative files and implement an AudioWorklet or maintained ambisonic decoder. Do not claim parity based solely on stereo playback.

Audio sources are one-shot nodes in Web Audio. Pause, seek, chapter changes, loops, and restart need source recreation with offsets. Schedule against `AudioContext.currentTime` and keep a documented mapping between IMM ticks, the animation clock, and the audio clock. Browser autoplay rules require an explicit user gesture before audio starts.

## Loading and streaming

### MVP: complete-buffer loading

Support:

- local `File`/drag-and-drop;
- full `fetch()` to `ArrayBuffer`;
- cancellation by terminating/resetting the decoder worker;
- visible phase progress: download, parse, decode, geometry build, upload.

This is sufficient to establish format and render parity. It is not acceptable as the final path for film-scale content.

### Phase 2: range-backed asset source

The format already contains asset offsets and sizes, so HTTP range loading is conceptually aligned with it. Implement a TypeScript random-access source:

```ts
interface RandomAccessSource {
  readonly size: bigint;
  read(offset: bigint, length: number, signal: AbortSignal): Promise<ArrayBuffer>;
}
```

Provide `File.slice()` and HTTP Range implementations. Parse enough of the leading chunk stream to obtain the scene graph and resource table, then request assets according to timeline need. Preserve the native idea of loading the first five seconds before background prefetch, but make the horizon configurable and driven by measured bandwidth/decode cost.

Deployment constraints:

- The server must support byte ranges and expose a stable content length/identity.
- `.imm` responses used for range offsets should not be transparently gzip/Brotli content-encoded; HTTP byte ranges apply to the encoded representation when content coding is present.
- Verify `206`/`Content-Range`; tolerate a server that ignores the Range header and returns `200` by falling back to a complete buffer.
- Same-origin hosting is simplest. Cross-origin content needs an appropriate CORS policy.
- Cache by URL plus validator (`ETag`/last-modified) and never combine ranges from different object versions.

For very large timelines, decode individual drawings/assets on demand rather than producing one monolithic Wasm result. That requires a persistent parsed document handle in the decoder worker and commands such as `parseMetadata`, `decodeAsset`, `decodeDrawing`, and `releaseAsset`.

## Memory and performance model

The 5.8 MB sample is not representative of the largest films. Track peak memory by category:

- fetched/source bytes;
- duplicate Wasm input bytes;
- Wasm decoder temporaries and zlib output;
- canonical stroke buffers;
- expanded web geometry;
- compressed and decoded image/audio buffers;
- GPU buffers/textures;
- prefetched but inactive drawings.

Expanded tube geometry may be several times larger than canonical point data. Therefore:

- transfer buffers rather than structured-cloning them;
- release the original full file after the decoder no longer needs it, or retain it only in the worker;
- do not retain both nested C++ `StrokeStore` vectors and flat output buffers;
- upload and release one drawing/batch at a time where possible;
- dispose Three.js geometries, materials, and textures explicitly;
- use an LRU/residency budget for inactive drawings;
- record peak Wasm pages and renderer geometry/texture counts;
- add device-tier budgets before WebXR release.

Performance work must follow actual corpus measurements. Do not add pthreads, shared memory, a texture-buffer point format, or WebGPU storage buffers until profiles identify decode, transfer, geometry expansion, upload, draw calls, or fill rate as the limiting stage.

### Large-file desktop checkpoint (2026-07-13)

Local-only Chrome tests used several uncommitted IMM files between roughly 159
MB and 440 MB. None of these large local test files is part of the repository or
deployment. The initial eager decoder consistently failed above roughly 150 MB
while growing the default Emscripten heap toward its 2 GB limit.

Two measured changes moved the tested desktop boundary:

- The Wasm build can grow to the Wasm32 4 GB ceiling rather than Emscripten's
  default 2 GB maximum.
- `IMM_WEB_DECODER` imports no longer retain both the importer's tessellated
  `Drawing` representation and the collector's canonical stroke copy. After
  worker-side geometry packing, temporary descriptors, bounds, points, and
  point-time arrays are also discarded instead of being transferred and kept
  by the player.

On the current reference desktop, a 263 MB scene with about 18.8 million points
loaded in approximately 21 seconds. Removing the retained JavaScript paint
buffers reduced its reported browser heap from about 5.9 GB to 4.75 GB. A 440
MB scene with about 32.9 million points, which previously aborted during native
decode even with a 4 GB Wasm maximum, then loaded in approximately 30 seconds
with about 3.6 GB reported browser heap. These figures are single-machine
diagnostics, not product budgets; browser heap reporting includes memory outside
the live Wasm allocation and should be treated as comparative evidence.

This establishes an eager full-download desktop fallback for the largest case
tested. It does not make that architecture suitable for mobile. Multi-gigabyte
peak memory and 20–30 second preparation times reinforce the existing Phase 5
requirement: metadata-first parsing, drawing/asset-at-a-time decode, bounded
residency, and incremental geometry upload are required for large-film mobile
and WebXR support.

## Expected performance and performance gates

No reliable frame-rate or load-time number can be derived from source inspection alone. It depends on film size, decoded point count, geometry expansion, alpha overdraw, image sizes, animation residency, device thermal state, and whether rendering is mono or stereo. The port is nevertheless structurally capable of good performance because decoding is batch-oriented and the public sample has a modest draw-call count.

### What the existing sample implies

The repository’s `sample1.imm` evidence reports approximately:

- 5.8 MB compressed source;
- 38 draw calls;
- 37 paint draw calls and one mono equirectangular picture draw;
- 645,802 triangles at the validated camera/time.

Thirty-eight draw calls are not a concerning Three.js submission count by themselves. Roughly 646k small paint triangles are plausible on current desktop GPUs and many mobile GPUs, but triangle count is not the decisive metric. Tube strokes can cover the same pixels repeatedly, and alpha-to-coverage/MSAA makes fill rate and memory bandwidth more important on mobile. WebXR also renders two views and commonly uses a high-resolution framebuffer.

### Expected bottlenecks by stage

| Stage | Likely constraint | Design response |
|---|---|---|
| Download | file size/network | full fetch for MVP; ranges later |
| Wasm decode | zlib and point reconstruction | worker, coarse calls, optional SIMD only after profile |
| Wasm-to-JS transfer | duplicate/copy volume | transferable coarse buffers; release source/temp memory |
| Geometry expansion | CPU time and memory multiplication | worker-side packing, active-drawing residency, later GPU expansion if needed |
| GPU upload | large first-frame stalls | incremental upload with a frame budget and progress state |
| Desktop mono rendering | overdraw/MSAA | batching, culling, measured coverage mode |
| Mobile rendering | fill rate, bandwidth, thermals | capped pixel ratio, residency budget, LOD/quality presets |
| WebXR | two eyes plus headset resolution | multiview where available through Three.js/WebXR, aggressive device profiling |
| Existing Three.js scene integration | combined host+IMM overdraw and state | one renderer/canvas; expose diagnostics and host budgets |

### Initial budgets to validate, not promised results

For `sample1.imm`, establish these engineering targets during the spike:

- Decoder worker produces metadata and canonical paint data without any main-thread task longer than 50 ms.
- Desktop load-to-first-image under 2 seconds on a defined reference machine after the source buffer is available.
- Mobile load-to-first-image under 5 seconds on a defined mid/high-tier Android reference device after the source buffer is available.
- Steady desktop mono rendering at 60 fps at 1280×720 and at a normal desktop device pixel ratio.
- Steady mobile rendering at 30 or 60 fps using an explicit pixel-ratio cap; record both rather than hiding a resolution reduction.
- No more than 50 draw calls for the sample unless a correctness split is documented.
- No main-thread geometry generation.
- No monotonic memory/resource growth over 20 load/unload cycles.

For WebXR, do not set a 72/90 fps commitment until Phase 6 runs on the actual standalone headset. Use the headset’s required frame interval as a hard profiling budget and report CPU update, submission, GPU time where exposed, resolution scale, draw calls, triangles, and dropped frames.

### Likely performance of the three architectures

| Architecture | Decode | Render overhead | Mobile outlook |
|---|---:|---:|---|
| Hybrid Wasm + shared Three.js scene | Near the intended design optimum | Three.js traversal/material overhead plus IMM GPU work | Best integration option; one full-resolution canvas |
| Hybrid Wasm + standalone Three.js player | Same decoder | Similar or slightly lower than embedded because the scene is controlled | Good baseline and benchmark target |
| Hybrid Wasm + dedicated raw renderer | Same decoder | Potentially lowest after substantial optimization | Useful only if measurements justify second renderer |
| Full native player compiled to Wasm/WebGL | More porting does not imply faster decode | Renderer emulation/state glue can offset native reuse | Poor integration and deployment trade-off |
| Two stacked canvases | Same underlying decode/render | Adds full-frame browser composition and duplicates surfaces | Works for flat layers; unattractive for sustained high-resolution mobile/XR |

“Wasm is near native” should not be used as a project acceptance claim. The relevant measurement is end-to-end: fetch, copy, decode, pack, upload, and render on the supported device corpus.

## Feasibility by feature

| Feature | Feasibility | Evidence / concern |
|---|---:|---|
| Scene chunks and hierarchy | High | Deterministic memory parser already exists |
| Paint decompression | High via Wasm | Self-contained C++ plus zlib; highest pure-JS reimplementation risk |
| Static paint rendering | High | Native topology and shaders provide a reference; WebGL representation must change |
| Animated paint drawing selection | High | Frame buffer and frame rate already exposed by importer |
| Full timeline behavior | Medium-high | Logic exists, but current stroke-reader boundary omits full keys |
| 2D/360 pictures | High | Browser image decode and Three.js texture/sphere/cube support are direct matches |
| Exact paint alpha parity | Medium | Depends on MSAA and optional sample-variable capability; needs fallback validation |
| Flat and positional audio | Medium-high | Web Audio maps well, subject to browser codec support and autoplay |
| Ambisonic audio parity | Medium | Audio360 cannot be reused as the browser plan; channel conventions need corpus proof |
| Spawn areas | High | Simple metadata and WebXR reference-space mapping |
| WebXR | High for basic VR, medium for parity | Three.js supplies the platform integration; device testing remains required |
| HTTP asset streaming | Medium-high | Asset offsets exist; metadata discovery and server byte-range policy need implementation |
| Model layers | Not currently scoped | Reference `ReadAsset()` is a no-op |
| Pure-JS decoder | Medium now, high later | Feasible but unnecessary duplication before a conformance oracle exists |

## Phased implementation plan

Estimates below are rough **single experienced engineer-equivalents**, not calendar commitments. They assume access to a representative private content corpus and target devices. Work can overlap once the decoder output schema stabilizes.

### Phase 0 — corpus and contract (about 1 week)

Deliverables:

- Inventory representative IMM files by sequence type, layer types, versions, codecs, animation features, file size, decoded stroke count, and expected appearance.
- Keep `sample1.imm` as the public baseline, but add legally usable fixtures that exercise each brush type, flipped transforms, alpha, animated drawings, chapters, pictures, sound formats, and spawn areas.
- Write a concise format/runtime behavior contract from the existing importer and player.
- Define MVP and parity tiers in the test matrix.

Exit gate:

- No feature is declared supported without at least one fixture and expected metadata/render/audio behavior.

### Phase 1 — Emscripten decoder spike (about 1–2 weeks)

Deliverables:

- Minimal Emscripten CMake target that compiles the memory importer and paint decoder without renderer, native file, sound, window, VR, or pthread code.
- Web Worker wrapper accepting `sample1.imm` as a transferred buffer.
- Versioned flat output for document/layers, paint drawings/strokes/points, frame maps, picture blob metadata, and all timeline keys.
- Structured errors, cancellation, phase timing, and memory counters.
- Native-versus-Wasm conformance dump test.

Exit gate:

- The Wasm decoder reports the same layer tree, transforms, drawing/frame mappings, stroke counts, representative decoded point values, bounds, picture type, and chapter markers as the native importer for all Phase 0 fixtures.
- The decoder runs in a worker without cross-origin isolation or pthreads.
- No platform renderer/audio libraries appear in the Wasm link map.

Stop/reassess condition:

- If removing accidental core dependencies requires broad changes, create a format-only decoder library from the existing source rather than compiling the production importer unchanged.

### Phase 2 — static visual MVP (about 3–5 weeks)

**Completed:** 2026-07-13 on `feature/web-native`.

Verification evidence:

- The production TypeScript/Vite build, focused native decoder test, Wasm
  worker smoke, and exact five-brush geometry tests pass.
- The Chrome harness renders all 30 paint layers (1,171 strokes / 58,405
  points) and the mono equirectangular picture from `sample1.imm`.
- The deterministic 1280×720 capture passes the committed web/native
  spatial/color contract in `tests/baselines/render/web-three-sample1.json`.
- The embedded fixture uses one host renderer, canvas, scene, camera, and depth
  buffer and places a normal host cube on an IMM paint vertex.
- Three consecutive reloads retain a constant WebGL geometry count, and the
  event loop remains responsive while the worker decodes and packs geometry.
- Desktop and four-times CPU-throttled mobile measurements are committed in
  `tests/baselines/web/sample1-phase2-browser.json` and are reproducible with
  `bun run test:browser` from `code/projects/web/app`.

Deliverables:

- TypeScript document model and decoder client.
- `@imm/three` adapter accepting a host renderer, scene parent, camera, and explicit time.
- Desktop Three.js viewer with file/URL loading and camera controls, implemented as a consumer of that adapter.
- Worker-side geometry generation for five paint brush types.
- Layer transforms, pivot, visibility, opacity, flip handling, bounds, and background color.
- Mono equirectangular picture rendering for `sample1.imm`.
- Alpha/color modes and capability diagnostics.
- Deterministic screenshot harness at 1280×720.

Exit gate:

- `sample1.imm` loads without blocking UI interaction for the duration of decode.
- The committed spatial/color render metric comparison passes a web-specific tolerance approved from side-by-side review.
- Dedicated geometry fixtures pass exact buffer/topology assertions.
- No WebGL resource growth occurs across repeated load/unload cycles.
- The same decoded document can render in the standalone viewer and inside a host Three.js fixture with shared depth against ordinary host geometry.
- A mobile profile records decode, pack, upload, GPU/frame time where available, pixel ratio, draw calls, triangles, and peak memory.

### Phase 3 — playback and full picture support (about 3–5 weeks)

Status: implemented and verified on the `feature/web-native` branch (2026-07-13).

Implementation evidence:

- The schema-v3 Wasm contract retains the complete layer hierarchy, root clock,
  chapters, animation keys, pivots, paint frame maps, point timing, picture
  layout metadata, and keep-alive parameters after native import.
- `ImmPlaybackController` provides deterministic tick-based play, pause, seek,
  restart, manual/author-authored wait and continue, chapter selection, skip
  back/forward, repeat limits, and root loop actions. Stateless evaluation
  covers nested timeline clocks, offsets, eased keys, pivoted transforms,
  visibility, opacity, draw-in, and drawing-frame selection.
- `ImmThreeView` applies evaluated local state through the authored hierarchy,
  keeps only the active drawing resident in WebGL, and implements draw-in,
  wiggle, blink, 2D pictures, viewer locking, mono/stereo equirectangular
  pictures, cross cubemaps, and vertical-strip cubemaps. Its explicit
  `setTimeTicks`/`setTimeSeconds` API is usable in host-owned Three.js loops.
- The standalone page owns a document-relative clock and exposes transport,
  seek, continue, restart, and chapter controls. The embedded fixture retains
  its host-owned renderer, scene, camera, canvas, depth buffer, and clock.

Verification evidence:

- `bun test` covers fresh-versus-history seeks, chapter equivalence, every
  transport state, stop/continue boundaries, nested loops, offsets, easing,
  paint frames, root loops, and pivot compensation.
- The Wasm CTest suite decodes the real sample and asserts 75 hierarchical
  layers, 1,015 animation keys, root duration, chapter bounds, paint geometry,
  point timing, and schema identity.
- `bun run test:browser` uses Chrome to capture the real sample at start,
  midpoint, and end plus a synthetic scene at start, interpolation midpoints,
  a nested-loop boundary, chapter boundary, pre-end, and end. At every fixture
  timestamp it asserts the exact active drawing, transform, opacity, draw-in,
  and picture/effect types.
- The same browser run proves history-independent rendered pixels after a
  re-seek, stable WebGL geometry counts through drawing swaps and three real
  sample reloads, all six cross/vertical cubemap face mappings, both stereo
  eyes, viewer locking, embedded shared depth, and the throttled mobile profile.
  Timestamp captures and the JSON report are written under
  `artifacts/web-native/`.

Deliverables:

- Playback clock and state machine: play, pause, seek, restart, wait/continue, skip, and chapter selection.
- Timeline interpolation, local time, repeats, offsets, transform/visibility/opacity keys, and drawing frame selection.
- Draw-in and keep-alive effects, gated independently if exact semantics require more work.
- 2D, viewer-locked, stereo equirectangular, cross cubemap, and vertical-strip cubemap support.
- CPU/GPU residency management for drawings.

Exit gate:

- Automated timestamp screenshots match expected active drawings, transforms, visibility, and opacity at start, midpoints, chapter boundaries, loops, and end.
- Seek and chapter changes are deterministic from a fresh load and after prior playback.

### Phase 4 — transparency/depth parity and browser audio (about 3–6 weeks)

Transparency and depth parity are release-critical, not optional visual polish.
The current web renderer combines Three.js transparent blending with hardware
alpha-to-coverage and depth writes. Native rendering instead disables blending,
writes opaque color, and uses a blue-noise-dithered, primitive-seeded MSAA
coverage mask. The web camera's current `0.01` to `20,000` clipping range also
needs replacement with scene-appropriate depth precision.

Deliverables:

- Native-equivalent order-independent coverage for paint, pictures, and models:
  no conventional alpha blending on the MSAA coverage path, opaque color output,
  depth writes, and opacity represented by sample coverage.
- A capability-selected custom sample-mask path where WebGL exposes the required
  sample-variable support, including native-equivalent blue-noise dithering and
  primitive/layer mask rotation.
- A measured alpha-hash/coverage fallback for browsers and embedded renderers
  that cannot expose programmable sample masks; diagnostics must identify the
  active path and actual MSAA sample count.
- Correct composition of point alpha, inherited layer opacity, directional
  visibility, draw-in, keep-alive effects, and picture/model alpha before
  coverage conversion.
- Scene-bounds-appropriate near/far planes, or an explicitly validated reverse-Z
  or logarithmic-depth alternative, in standalone and embedded camera guidance.
- Fixtures with coincident and near-coplanar translucent strokes, intersecting
  opaque geometry, animated fades, and deliberately permuted submission order.
- Compressed sound blob export and codec capability probe.
- Flat and positional playback with gain, loop, pause, seek, and chapter synchronization.
- Attenuation and directional cone/frustum behavior.
- Ambisonic proof and explicit supported channel/order contract.
- Autoplay/user-gesture UX and suspend/resume handling.

Exit gate:

- Animated opacity and overlapping-stroke captures match the native reference at
  agreed sample counts, without darkening from blended alpha or visible depth
  fighting under a stationary camera.
- Permuting layer, stroke, and batch submission order does not materially change
  resolved pixels on the order-independent coverage path.
- Depth precision remains stable across the supported scene-size corpus and the
  embedded adapter reports incompatible host camera/depth settings.
- A/V drift stays within an agreed bound over long-running fixtures and after pause/seek/chapter operations.
- Each claimed codec works on the supported browser matrix.
- Ambisonic is either validated spatially or marked unsupported; stereo downmix is not labeled parity.

### Phase 5 — streaming and production memory (about 3–5 weeks)

Deliverables:

- `File.slice()` and HTTP Range random-access sources.
- Metadata-first parsing and asset/drawing decode commands.
- Timeline-prioritized prefetch, cancellation, retry, and bounded LRU residency.
- Complete-download fallback for non-range servers.
- Size/complexity limits and malformed-file hardening.

Exit gate:

- A large representative film begins playback without downloading/decoding all assets.
- Network traces show only required ranges plus configured prefetch.
- Peak JS, Wasm, and GPU memory remain within per-device budgets during long playback and repeated chapter seeks.

### Phase 6 — WebXR and device validation (about 2–4 weeks)

Status: partially implemented on `main` (2026-07-13).

Implemented so far:

- Named spawn areas are exposed as standalone-player viewpoints, with the exact
  scene-header initial spawn distinguished from later `MakeDefault` actions.
- The active authored spawn is resolved from playback time, so chapter changes
  and seeks apply their authored camera transform. Manual viewpoint selection
  remains independent until a later authored spawn action takes over.
- Fly/free-look is the standalone default on mouse, keyboard, wheel, and touch;
  OrbitControls remains selectable. The embedded adapter still leaves all
  camera and navigation ownership to its host.
- The standalone renderer enables WebXR, presents Three.js's VR session entry,
  and uses the active authored spawn as the XR rig pose.

Remaining before the Phase 6 exit gate:

- Export and enforce spawn-area tracking level, movement volume, and per-axis
  locomotion constraints.
- Add controller transport input, explicit reference-space policy, and headset
  lifecycle recovery tests.
- Validate stereo rendering, orientation, scale, performance, and interaction
  on desktop WebXR and a standalone Quest-class browser.

Deliverables:

- Three.js WebXR session UI and reference-space selection.
- Spawn-area placement and locomotion constraints.
- Controller transport input and session lifecycle handling.
- Stereo 360 behavior and standalone-headset performance presets.

Exit gate:

- Validated on at least one desktop WebXR configuration and one standalone Quest-class browser.
- No eye mismatch, reversed winding, background seam/orientation error, or scale discrepancy against desktop/native references.
- Sustained frame timing meets the agreed headset target on the representative corpus.

### Phase 7 — release hardening (about 2–4 weeks)

Deliverables:

- Fuzzing, failure recovery, CSP/CORS/deployment documentation, caching, accessibility, and telemetry controls.
- CI build with pinned Emscripten and npm dependencies.
- Browser smoke matrix and screenshot regressions.
- Public API and embedding example.
- License and third-party notice audit for the exact Wasm dependency closure.

Exit gate:

- Reproducible production build, supported-browser statement, known-limitations document, and no unresolved critical decoder/security defects.

### Overall estimate

- Static visual feasibility proof: roughly **5–8 engineer-weeks** after corpus setup.
- Useful desktop viewer with playback and pictures: roughly **8–13 engineer-weeks**.
- Audio, streaming, WebXR, and production hardening: roughly **9–17 additional engineer-weeks**.
- Broad production target: roughly **17–30 engineer-weeks total**, with ambisonic parity and large-film behavior as the largest uncertainty multipliers.

These estimates should be replaced after Phase 1 measures the actual source closure, output volume, decode time, and geometry expansion cost.

## Testing strategy

### Decoder conformance

Build one corpus-driven test runner that can invoke both native and Wasm decoders and compare a normalized manifest:

- document metadata and caps;
- layer IDs, names, hierarchy, types, transforms/pivots, and bounds;
- all animation keys and values;
- asset records;
- drawing/frame maps;
- stroke brush/visibility/bounds;
- selected and hashed point ranges with floating-point tolerances;
- picture/sound/spawn metadata and compressed blob hashes.

Do not serialize every point into CI JSON for large files. Hash canonical binary arrays and retain small human-readable samples.

### Geometry tests

For each brush type and flip state, compare:

- vertex/index counts;
- winding and degenerate strip connectors;
- endpoint adjustment;
- decoded width;
- tangent/basis orthogonality;
- bounds;
- color/alpha values;
- chunk boundaries around 65,536 vertices.

### Visual tests

Reuse the repository’s baseline metric tooling rather than relying only on pixel equality. Add:

- fixed camera and resolution;
- deterministic animation time/frame ID;
- color means and spatial luma grid;
- silhouette/edge coverage metrics;
- 360 orientation markers;
- close crops for thin translucent strokes;
- timestamp-specific animation baselines.

Maintain different approved references only where browser/device rendering differences are demonstrated and understood.

### Runtime and performance tests

- UI heartbeat/long-task monitoring during download, decode, build, and upload.
- decode time, point throughput, geometry expansion time, transfer time, upload time, draw calls, triangles, and frame percentiles.
- peak source/Wasm/JS/GPU memory.
- repeated load/unload and seek soak tests.
- slow-network range and cancellation tests.
- WebGL context loss/recovery.
- browser audio suspend/resume and device change.
- WebXR enter/exit and visibility changes.

### Security tests

- native ASan/UBSan corpus and fuzz target;
- truncated input at every major structure boundary;
- corrupt lengths/offsets/counts/enums;
- zlib expansion limits;
- deep/cyclic layer structures;
- invalid floating point values;
- worker crash/restart behavior.

## CI and deployment requirements

- Pin an Emscripten SDK version in CI and produce a deterministic decoder build where practical.
- Serve `.wasm` as `application/wasm` to permit streaming compilation.
- Keep the first build compatible without COOP/COEP because it uses a normal worker rather than Wasm pthreads.
- If pthreads are introduced later, document and test `Cross-Origin-Opener-Policy` and `Cross-Origin-Embedder-Policy`; these headers can affect embedding and third-party resources.
- Serve range-addressed `.imm` files without content encoding and with byte-range support.
- Use content hashes for Wasm/JS assets and validators for IMM media.
- Test at least current Chromium and Firefox desktop. Add Safari only after codec, WebGL alpha, worker, and WebXR expectations are explicitly scoped; do not imply XR support where the browser/device does not provide it.

## Key risks and mitigations

| Risk | Impact | Mitigation / decision gate |
|---|---|---|
| Wasm source closure drags in all of `libImmCore` | Large binary and porting work | Dedicated decoder target; return compressed media; replace accidental type dependencies |
| Current collector omits timeline/audio/hierarchy details | Incomplete player | New versioned flat schema based on importer data, not the current query API |
| Expanded WebGL geometry uses too much memory | Large films fail on mobile | Measure Phase 2; worker packing, residency budgets, texture-backed points or WebGPU only if justified |
| Alpha-to-coverage differs by browser/GPU | Visible paint artifacts | Three modes, capability diagnostics, close-up baselines, documented fallback |
| Color transform differs from native | Broad baseline mismatch | Canonical linear contract, explicit Three.js color spaces, no tone mapping in parity mode |
| Codec support varies | Missing audio | Capability matrix and optional codec-specific Wasm fallback |
| Ambisonic conventions are unclear | Incorrect spatial audio | Corpus inventory and spatial proof before support claim |
| Detached native loader semantics do not fit browser | Hangs/races | Worker command protocol, `AbortSignal`, explicit ownership and teardown |
| Full-buffer load multiplies memory | Film-scale failure | MVP limitation; Phase 5 range source and per-asset decode before production claim |
| Malformed input exploits unchecked C++ reads | Security/stability issue | Bounds-aware stream, configured limits, ASan/UBSan, fuzzing, worker isolation |
| WebXR masks transform bugs | Device-only visual mismatch | Desktop parity first, orientation fixtures, per-eye XR gates |
| Models appear supported in enums but are not imported | False capability claim | Mark unsupported until a real model format/fixture/reader exists |

## Immediate implementation backlog

The first implementation PRs should be small and ordered as follows:

1. Add corpus manifest tooling that records native decoder facts for `sample1.imm` and targeted micro-fixtures.
2. Add `code/projects/web/decoder` CMake/Emscripten skeleton and a no-render memory-input smoke.
3. Introduce bounds-aware input errors and an `IMM_WEB` logging shim.
4. Define `imm_web_output.h` schema v1 and a native dump implementation before binding it to Wasm.
5. Compile single-threaded Wasm and run it in a Worker test page.
6. Compare native and Wasm manifests in CI.
7. Build one segment/ribbon fixture into Three.js `BufferGeometry`.
8. Extend to all five brush types and flipped transforms.
9. Define and test embedded renderer state ownership with a host cube intersecting IMM paint through the shared depth buffer.
10. Load and render all paint from `sample1.imm` in both the standalone and embedded fixtures.
11. Add its mono equirectangular picture and fixed-camera screenshot comparison.
12. Profile the same build on a reference mobile device before considering a second renderer.

Do not begin audio, WebXR, range streaming, or a pure-JS decoder until the Phase 2 visual gate passes. Their APIs can be reserved in the data schema, but early parallel implementation would make format, geometry, camera, and platform errors harder to isolate.

## Go/no-go criteria

Proceed from feasibility work to a committed product port only if Phase 1 and the early Phase 2 spike demonstrate all of the following:

- The importer compiles to a focused Wasm module without native renderer/audio/platform dependencies.
- Wasm output matches native decoded content across the initial corpus.
- `sample1.imm` renders recognizably and passes agreed automated spatial/color metrics.
- Decode and geometry build occur off the main thread.
- Peak memory has a credible path to film-scale bounds through per-asset loading.
- No required feature depends on the discontinued Audio360 binary; ambisonic replacement is independently scoped.

Reassess the architecture if any of these occur:

- the focused decoder is not separable without maintaining a near-fork of the importer;
- expanded geometry exceeds target-device memory on representative content and the texture/WebGPU alternatives fail their spike;
- required browsers cannot provide acceptable paint coverage quality;
- representative audio uses undocumented channel/container conventions that cannot be decoded reliably;
- content hosting cannot support identity byte ranges and full downloads are too large.

## Current technical references

- [Emscripten: Building to WebAssembly](https://emscripten.org/docs/compiling/WebAssembly.html)
- [Emscripten: Pthreads support and cross-origin isolation requirements](https://emscripten.org/docs/porting/pthreads.html)
- [Emscripten: Browser runtime and asynchronous main-loop constraints](https://emscripten.org/docs/porting/emscripten-runtime-environment.html)
- [Three.js: WebGLRenderer](https://threejs.org/docs/pages/WebGLRenderer.html)
- [Three.js: BufferGeometry](https://threejs.org/docs/pages/BufferGeometry.html)
- [Three.js: Material alpha-to-coverage](https://threejs.org/docs/pages/Material.html)
- [Three.js: WebXRManager](https://threejs.org/docs/pages/WebXRManager.html)
- [Khronos: WebGL `OES_sample_variables`](https://registry.khronos.org/webgl/extensions/OES_sample_variables/)
- [MDN: Transferable objects](https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Transferable_objects)
- [MDN: HTTP Range header](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Range)
- [MDN: `decodeAudioData()`](https://developer.mozilla.org/en-US/docs/Web/API/BaseAudioContext/decodeAudioData)

These references describe current platform capabilities; repository behavior and the project’s fixtures remain the authority for IMM semantics.
