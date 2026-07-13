# IMM web player

This directory contains two consumers of the shared Wasm decoder and Three.js
adapter:

- `index.html` is the standalone player. It owns its renderer, canvas, camera,
  controls, and render loop and accepts local files or same-origin/CORS URLs.
  It loads the bundled `sample1.imm` by default, keeps both source controls
  available while loading, and can paste an HTTP(S) IMM URL from the clipboard.
  Authored spawn areas appear as named viewpoints. The authored initial spawn is
  used on load, and timeline `MakeDefault` actions move to the viewpoint selected
  by a chapter or seek. Fly/free-look navigation is the default, with orbit as an
  optional mode.
- `embed.html` is the integration fixture. The host owns one renderer, canvas,
  scene, camera, and render loop; the IMM adapter contributes an `Object3D`
  subtree to that scene so host and IMM geometry share the depth buffer.

Both render all five IMM paint brush types and the complete Phase 3 playback
model. Paint decoding and indexed-geometry packing run in the decoder worker;
the main thread creates and uploads Three.js resources. Picture support covers
2D, viewer-locked, mono/stereo equirectangular, cross-cubemap, and
vertical-strip-cubemap layers.

`ImmThreeView` also applies the native double-sided, unlit, vertex-RGB coverage
contract to canonical model geometry supplied by a host. The browser fixture
checks opacity coverage, depth state, flipped transforms, and submission-order
independence. This is not a claim that model assets load from IMM files: the
repository's native model import/export functions are serialization stubs, so
the Wasm decoder explicitly returns no model geometry until a real format and
fixture exist.

The standalone player decodes embedded WAV, Ogg Vorbis, and Ogg Opus payloads
through Web Audio. Audio remains suspended until the user selects **Enable
audio**. Flat and positional layers follow authored visibility, opacity, gain,
looping, transforms, distance attenuation, directional cone/frustum modifiers,
transport pause, seek, and chapter changes. Browser diagnostics report codec
probes, decode failures, active source types, and restart offsets. Ambisonic
layers are currently decoded for capability reporting but explicitly not played
or claimed as spatially correct.

While enabled audio is running, playback advances from
`AudioContext.currentTime`, giving visuals and sources one monotonic clock.
Diagnostics expose current/maximum source drift and browser base/output
latency. Authored waits retain native behavior: active child timelines and
sounds may continue, and a stationary authored sound timeline is not counted
as clock drift.

On browsers with WebXR support, the standalone player also presents Three.js's
VR entry button. The current authored viewpoint becomes the XR reference pose;
the embeddable adapter continues to leave renderer, camera, controls, and the XR
session entirely under host ownership.

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

The same Chrome run verifies that `sample1.imm` exports and decodes all three
Opus layers, starts no sources before the user gesture, resumes and suspends the
audio context with transport controls, uses one positional and two flat source
paths, advances visuals from the audio clock within a 50 ms smoke bound, and
restarts each source at deterministic seek/chapter offsets.

Set `IMM_WEB_HEADLESS=1` only for unattended automation. Results are written to
`artifacts/web-native/` and include decode, marshal, pack, upload-render,
CPU-frame, GPU-frame (when timer queries are available), draw-call, triangle,
pixel-ratio, texture/geometry, and JavaScript-heap measurements.

Private local codec fixtures can be included without copying or serving them by
setting `IMM_WEB_LOCAL_OGG_FIXTURE` and/or `IMM_WEB_LOCAL_WAV_FIXTURE` to their
absolute paths before `bun run test:browser`. Playwright transfers each file
directly through the browser file input; reports retain only the codec label and
decoded layer counts, not the source path or bytes.

`IMM_WEB_LOCAL_FLOOR_SPAWN_FIXTURE` optionally verifies a private floor-level
viewpoint through the browser file input. The assertion compares the decoded
authored height with the applied desktop camera height and retains only the
tracking label and measured offset.

## GitHub Pages

`.github/workflows/web-pages.yml` builds and verifies the Wasm decoder and web
application on pushes to `main`, then deploys the production bundle with the
repository's GitHub Pages base path. The deployed bundle includes
`sample1.imm` and loads it on startup. Pages builds scope the decoder, Wasm,
geometry module, and sample scene beneath the Git commit ID. A small bootstrap
in `index.html` checks a cache-busted `version.json` and reloads stale HTML with
the current release ID. Local builds continue to use unversioned asset paths;
`VITE_IMM_DEFAULT_SOURCE` can still override the initial scene.
