import { afterAll, beforeAll, describe, expect, test } from "bun:test";
import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { resolve } from "node:path";
import {
    copyDecoderRelease,
    decoderAssetFiles,
    validateDecoderAssets,
    verifyDecoderRelease,
} from "../scripts/decoder-assets.mjs";

describe("decoder release assets", () => {
    let temporaryDirectory;
    let source;
    let release;

    beforeAll(async () => {
        temporaryDirectory = await mkdtemp(resolve(tmpdir(), "imm-decoder-assets-"));
        source = resolve(temporaryDirectory, "source");
        release = resolve(temporaryDirectory, "release");
        await import("node:fs/promises").then(({ mkdir }) => mkdir(source));
        for (const filename of decoderAssetFiles) {
            const content = filename === "imm-web-decoder-worker.mjs"
                ? 'import decoder from "./imm-web-decoder.mjs";\nimport "./imm-web-paint-packet.mjs";\n'
                : filename;
            await writeFile(resolve(source, filename), content);
        }
    });

    afterAll(async () => {
        await rm(temporaryDirectory, { recursive: true, force: true });
    });

    test("accepts and packages one exact decoder set", async () => {
        await validateDecoderAssets(source);
        await copyDecoderRelease(source, release);
        await verifyDecoderRelease(release);
    });

    test("rejects stale extra files", async () => {
        const stale = resolve(source, "obsolete-decoder-module.mjs");
        await writeFile(stale, "export {};\n");
        await expect(validateDecoderAssets(source)).rejects.toThrow("Unexpected: obsolete-decoder-module.mjs");
        await rm(stale);
    });

    test("rejects a modified packaged asset", async () => {
        await copyDecoderRelease(source, release);
        await writeFile(resolve(release, "imm-web-decoder.wasm"), "modified");
        await expect(verifyDecoderRelease(release)).rejects.toThrow("Decoder release hash mismatch");
    });
});
