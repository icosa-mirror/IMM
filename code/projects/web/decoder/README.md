# IMM web decoder

This target is the format-facing C ABI for the native web port. It intentionally
has no dependency on the native renderer, sound engine, file system, windowing,
VR, or threading backends.

The first slice performs bounded inspection of the top-level IMM chunk stream
and resource table. Paint and scene decoding will be moved behind the same ABI
in subsequent slices.

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

The Emscripten build emits an ES module intended to run inside a Web Worker. It
does not enable pthreads or a virtual filesystem.

Run its worker-level smoke test with:

```bash
ctest --test-dir build/web-decoder-wasm --output-on-failure
```

The smoke transfers `sample1.imm` to the worker, calls the Wasm C ABI, and
checks the returned document summary. The same worker module supports browser
module workers and Node workers used by CI.
