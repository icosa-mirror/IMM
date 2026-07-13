import { resolve } from "node:path";
import { createReadStream } from "node:fs";
import { defineConfig } from "vite";

export default defineConfig({
    base: process.env.IMM_WEB_BASE_PATH ?? "/",
    plugins: [{
        name: "imm-sample-fixture",
        configureServer(server) {
            server.middlewares.use("/fixtures/sample1.imm", serveSample);
        },
        configurePreviewServer(server) {
            server.middlewares.use("/fixtures/sample1.imm", serveSample);
        },
    }],
    build: {
        rollupOptions: {
            input: {
                player: resolve(import.meta.dirname, "index.html"),
                embed: resolve(import.meta.dirname, "embed.html"),
                phase3Fixture: resolve(import.meta.dirname, "phase3-fixture.html"),
            },
        },
    },
});

function serveSample(_request, response) {
    response.setHeader("Content-Type", "application/octet-stream");
    createReadStream(resolve(import.meta.dirname, "../../../../exampleImmFiles/sample1.imm")).pipe(response);
}
