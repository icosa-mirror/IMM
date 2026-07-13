import assert from "node:assert/strict";
import { readFile, stat } from "node:fs/promises";
import { resolve } from "node:path";

const release = process.env.VITE_IMM_RELEASE_ID?.trim() ?? "";
if (release === "") {
    await stat(resolve("dist/decoder/imm-web-decoder-worker.mjs"));
    await stat(resolve("dist/fixtures/sample1.imm"));
    console.log("Web cache versioning: local unversioned asset layout passed");
    process.exit(0);
}

const index = await readFile(resolve("dist/index.html"), "utf8");
const version = JSON.parse(await readFile(resolve("dist/version.json"), "utf8"));
assert.equal(version.release, release);
assert.match(index, new RegExp(`<meta name="imm-release" content="${escapeRegExp(release)}"`));
assert.match(index, /data-imm-release-bootstrap/);
assert.match(index, /cache-bust/);
await stat(resolve("dist/decoder", release, "imm-web-decoder-worker.mjs"));
await stat(resolve("dist/decoder", release, "imm-web-decoder.wasm"));
await stat(resolve("dist/decoder", release, "imm-web-geometry.mjs"));
await stat(resolve("dist/fixtures", release, "sample1.imm"));

const builtFiles = await import("node:fs/promises").then(({ readdir }) => readdir(resolve("dist/assets")));
const bundle = (await Promise.all(builtFiles
    .filter((filename) => filename.endsWith(".js"))
    .map((filename) => readFile(resolve("dist/assets", filename), "utf8")))).join("\n");
assert.ok(bundle.includes(release));
assert.ok(bundle.includes("imm-web-decoder-worker.mjs"));
assert.ok(bundle.includes("sample1.imm"));
console.log(`Web cache versioning: release ${release} passed`);

function escapeRegExp(value) {
    return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
