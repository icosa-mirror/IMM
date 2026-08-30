import { writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { defineConfig } from "vite";
import { copyDecoderRelease } from "./scripts/decoder-assets.mjs";

const decoderPath = resolve(import.meta.dirname, "public/decoder");

export default defineConfig({
    publicDir: false,
    build: {
        outDir: "dist-library",
        emptyOutDir: true,
        sourcemap: true,
        lib: {
            entry: resolve(import.meta.dirname, "src/library/index.ts"),
            formats: ["es"],
            fileName: () => "imm-three-loader.js",
        },
        rollupOptions: {
            external: ["three", /^three\//],
        },
    },
    plugins: [{
        name: "imm-three-loader-package",
        async writeBundle(outputOptions) {
            const outputDirectory = outputOptions.dir ?? resolve(import.meta.dirname, "dist-library");
            const outputDecoder = resolve(outputDirectory, "decoder");
            await copyDecoderRelease(decoderPath, outputDecoder);
            await writeFile(resolve(outputDirectory, "package.json"), `${JSON.stringify({
                name: "@immersive-foundation/three-imm-loader",
                version: "0.0.0-dev",
                type: "module",
                module: "./imm-three-loader.js",
                types: "./types/library/index.d.ts",
                exports: {
                    ".": {
                        types: "./types/library/index.d.ts",
                        import: "./imm-three-loader.js",
                    },
                    "./decoder/*": "./decoder/*",
                },
                files: ["imm-three-loader.js", "imm-three-loader.js.map", "types", "decoder"],
                peerDependencies: { three: "0.180.x" },
            }, null, 2)}\n`);
        },
    }],
});
