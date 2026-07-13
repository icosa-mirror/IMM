# IMM web player

This is the first standalone consumer of the shared decoder worker. It owns one
Three.js canvas and currently displays bounded Wasm inspection results for a
local IMM file. Paint rendering is not implemented in this slice.

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

Open the displayed local URL and choose `exampleImmFiles/sample1.imm`. The UI
should report 38 assets in five top-level chunks. Generated decoder assets under
`public/decoder` are ignored by Git.
