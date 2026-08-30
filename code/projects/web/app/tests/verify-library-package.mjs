import assert from "node:assert/strict";
import { readFile, readdir, stat } from "node:fs/promises";
import { resolve } from "node:path";
import { decoderAssetFiles, verifyDecoderRelease } from "../scripts/decoder-assets.mjs";

const output = resolve("dist-library");
const packageJson = JSON.parse(await readFile(resolve(output, "package.json"), "utf8"));
assert.equal(packageJson.type, "module");
assert.equal(packageJson.module, "./imm-three-loader.js");
assert.equal(packageJson.types, "./types/library/index.d.ts");
await stat(resolve(output, "imm-three-loader.js"));
await stat(resolve(output, "types/library/index.d.ts"));
for (const filename of decoderAssetFiles) await stat(resolve(output, "decoder", filename));
await verifyDecoderRelease(resolve(output, "decoder"));
assert.deepEqual(
    (await readdir(resolve(output, "decoder"))).sort(),
    [...decoderAssetFiles, "manifest.json"].sort(),
);

const worker = await readFile(resolve(output, "decoder/imm-web-decoder-worker.mjs"), "utf8");
for (const filename of decoderAssetFiles.filter((filename) => filename.endsWith(".mjs") && !filename.includes("worker"))) {
    if (worker.includes(`./${filename}`)) await stat(resolve(output, "decoder", filename));
}

console.log("IMM Three.js loader package validation passed");
