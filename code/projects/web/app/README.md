# IMM web player

This directory contains two consumers of the shared Wasm decoder and Three.js
adapter:

- `index.html` is the standalone player. It owns its renderer, canvas, camera,
  controls, and render loop and accepts local files or same-origin/CORS URLs.
- `embed.html` is the integration fixture. The host owns one renderer, canvas,
  scene, camera, and render loop; the IMM adapter contributes an `Object3D`
  subtree to that scene so host and IMM geometry share the depth buffer.

Both render all five IMM paint brush types and the complete Phase 3 playback
model. Paint decoding and indexed-geometry packing run in the decoder worker;
the main thread creates and uploads Three.js resources. Picture support covers
2D, viewer-locked, mono/stereo equirectangular, cross-cubemap, and
vertical-strip-cubemap layers.

## Build decoder assets

Activate the Emscripten version in `../EMSCRIPTEN_VERSION`, then run from the
repository root:

```bash
emcmake cmake -S code/projects/web/decoder -B build/web-decoder-wasm \
  -DCMAKE_BUILD_TYPE=Release \
  -DIMM_WEB_OUTPUT_DIRECTORY="$PWD/code/projects/web/app/public/decoder"
cmake --build build/web-decoder-wasm
```

## Run the application

```bash
cd code/projects/web/app
bun install
bun run dev
```

Open the displayed local URL and choose `exampleImmFiles/sample1.imm`, or load
the same file by URL. The standalone summary reports decoded, packed, upload,
render, and capability diagnostics. Open `/embed.html` for the shared-scene
fixture. Generated decoder assets under `public/decoder` are ignored by Git.

## Adapter contract

`ImmThreeView` accepts an optional host `WebGLRenderer` and scene parent. It
does not create a renderer, canvas, camera, animation loop, or WebXR session.
The host calls `update(timeSeconds, camera)`, `setTimeSeconds`, or
`setTimeTicks` and renders its own scene, then calls `dispose()` when unloading
the document. The adapter disposes every geometry, material, and texture it
creates and keeps only each paint layer's active drawing resident in WebGL.

## Verification

The focused decoder suite includes the Wasm worker smoke and exact buffer tests
for all five brush topologies:

```bash
ctest --test-dir build/web-decoder-wasm --output-on-failure
```

The browser harness launches installed Chrome visibly by default, captures the
fixed 1280×720 standalone and embedded views, compares the standalone capture
to the committed native spatial/color contract, checks deterministic Phase 3
timestamps and every picture mapping, checks drawing swaps and three reload
cycles for WebGL resource growth, and records a 390×844, 3× DPR, four-times
CPU-throttled mobile profile:

```bash
bun run test:browser
```

Set `IMM_WEB_HEADLESS=1` only for unattended automation. Results are written to
`artifacts/web-native/` and include decode, marshal, pack, upload-render,
CPU-frame, GPU-frame (when timer queries are available), draw-call, triangle,
pixel-ratio, texture/geometry, and JavaScript-heap measurements.
