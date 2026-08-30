import { createHash } from "node:crypto";
import { copyFile, mkdir, readFile, readdir, rm, writeFile } from "node:fs/promises";
import { resolve } from "node:path";

export const decoderAssetFiles = Object.freeze([
    "imm-web-decoder-worker.mjs",
    "imm-web-decoder.mjs",
    "imm-web-decoder.wasm",
    "imm-web-paint-packet.mjs",
]);

export async function validateDecoderAssets(directory) {
    const entries = await readdir(directory, { withFileTypes: true }).catch((error) => {
        throw new Error(`Decoder output is unavailable at ${directory}`, { cause: error });
    });
    const actual = entries.filter((entry) => entry.isFile()).map((entry) => entry.name).sort();
    const expected = [...decoderAssetFiles].sort();
    const missing = expected.filter((filename) => !actual.includes(filename));
    const unexpected = actual.filter((filename) => !expected.includes(filename));
    if (missing.length > 0 || unexpected.length > 0) {
        throw new Error([
            `Decoder output at ${directory} is not a clean release set.`,
            missing.length === 0 ? "" : `Missing: ${missing.join(", ")}.`,
            unexpected.length === 0 ? "" : `Unexpected: ${unexpected.join(", ")}.`,
            "Rebuild the decoder instead of reusing this directory.",
        ].filter(Boolean).join(" "));
    }

    const worker = await readFile(resolve(directory, "imm-web-decoder-worker.mjs"), "utf8");
    const imports = [...worker.matchAll(/["']\.\/([^"']+)["']/g)].map((match) => match[1]);
    const unresolved = imports.filter((filename) => !actual.includes(filename));
    if (unresolved.length > 0) {
        throw new Error(`Decoder worker has unresolved imports: ${unresolved.join(", ")}`);
    }
}

export async function copyDecoderRelease(sourceDirectory, targetDirectory) {
    await validateDecoderAssets(sourceDirectory);
    await rm(targetDirectory, { recursive: true, force: true });
    await mkdir(targetDirectory, { recursive: true });

    const hashes = {};
    for (const filename of decoderAssetFiles) {
        const source = resolve(sourceDirectory, filename);
        await copyFile(source, resolve(targetDirectory, filename));
        hashes[filename] = createHash("sha256").update(await readFile(source)).digest("hex");
    }
    await writeFile(resolve(targetDirectory, "manifest.json"), `${JSON.stringify({
        format: 1,
        revision: process.env.GITHUB_SHA ?? process.env.VITE_IMM_RELEASE_ID ?? null,
        sha256: hashes,
    }, null, 2)}\n`);
}

export async function verifyDecoderRelease(directory) {
    const manifest = JSON.parse(await readFile(resolve(directory, "manifest.json"), "utf8"));
    if (manifest.format !== 1 || typeof manifest.sha256 !== "object" || manifest.sha256 === null) {
        throw new Error("Decoder release manifest has an unsupported format");
    }
    for (const filename of decoderAssetFiles) {
        const expected = manifest.sha256[filename];
        const actual = createHash("sha256").update(await readFile(resolve(directory, filename))).digest("hex");
        if (expected !== actual) throw new Error(`Decoder release hash mismatch: ${filename}`);
    }
}
