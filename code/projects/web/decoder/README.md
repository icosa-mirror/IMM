# IMM web decoder

This target is the format-facing C ABI for the native web port. It intentionally
has no dependency on the native renderer, sound engine, file system, windowing,
VR, or threading backends.

The decoder performs bounded top-level inspection and full scene decoding. Its
schema-v4 bulk C ABI exports the complete hierarchy, playback clock, chapters,
animation keys, transforms and pivots, visibility and opacity, default spawn,
paint drawings/strokes/points/timing/frame maps, keep-alive parameters, and
decoded picture metadata/pixels. It also preserves WAV, Ogg Vorbis, and Ogg
Opus sound payloads with their playback and spatial metadata. Its worker
expands the five paint brush types into transferable indexed buffers before
returning the canonical document.

## Native contract test

```powershell
cmake -S code/projects/web/decoder -B build/web-decoder-native
cmake --build build/web-decoder-native --config Release
ctest --test-dir build/web-decoder-native -C Release --output-on-failure
```

## Emscripten module

With the Emscripten SDK version pinned in `../EMSCRIPTEN_VERSION` activated:

```bash
emcmake cmake -S code/projects/web/decoder -B build/web-decoder-wasm \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/web-decoder-wasm
```

To serve the generated module directly from the standalone Vite application,
configure with:

```bash
emcmake cmake -S code/projects/web/decoder -B build/web-decoder-wasm \
  -DCMAKE_BUILD_TYPE=Release \
  -DIMM_WEB_OUTPUT_DIRECTORY="$PWD/code/projects/web/app/public/decoder"
cmake --build build/web-decoder-wasm
```

The Emscripten build emits an ES module intended to run inside a Web Worker. It
does not enable pthreads or a virtual filesystem.

Run its worker-level smoke test with:

```bash
ctest --test-dir build/web-decoder-wasm --output-on-failure
```

The smoke transfers `sample1.imm` to the worker, calls the Wasm C ABI, and
checks hierarchy, root timing, chapters, all animation keys, spawn, paint,
picture, encoded sound, stroke, point, geometry-batch, and triangle contracts. A separate
deterministic test asserts exact positions, colors, indices, and topology sizes
for all five brush types. The same worker module supports browser module
workers and Node workers used by CI.

For machine-local corpus inspection without copying an IMM into the repository
or onto a web host, pass the generated worker and source file to:

```powershell
node tests/inspect_scene_file.mjs <worker.mjs> <local-file.imm>
```
