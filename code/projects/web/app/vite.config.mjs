import { resolve } from "node:path";
import { createReadStream } from "node:fs";
import { copyFile, mkdir, readdir, writeFile } from "node:fs/promises";
import { defineConfig } from "vite";

const samplePath = resolve(import.meta.dirname, "../../../../exampleImmFiles/sample1.imm");
const decoderPath = resolve(import.meta.dirname, "public/decoder");
const basePath = process.env.IMM_WEB_BASE_PATH ?? "/";
const releaseId = process.env.VITE_IMM_RELEASE_ID?.trim() ?? "";
if (releaseId !== "" && !/^[A-Za-z0-9._-]+$/.test(releaseId)) {
    throw new Error(`VITE_IMM_RELEASE_ID contains unsafe path characters: ${releaseId}`);
}

export default defineConfig({
    base: basePath,
    plugins: [{
        name: "imm-sample-fixture",
        configureServer(server) {
            server.middlewares.use("/fixtures/sample1.imm", serveSample);
        },
        configurePreviewServer(server) {
            server.middlewares.use("/fixtures/sample1.imm", serveSample);
        },
        transformIndexHtml(html, context) {
            if (releaseId === "" || !context.filename.endsWith("index.html")) return html;
            return {
                html,
                tags: [
                    {
                        tag: "meta",
                        attrs: { name: "imm-release", content: releaseId },
                        injectTo: "head",
                    },
                    {
                        tag: "script",
                        attrs: { "data-imm-release-bootstrap": "" },
                        children: releaseBootstrap(basePath),
                        injectTo: "head",
                    },
                ],
            };
        },
        async writeBundle(outputOptions) {
            const outputDirectory = outputOptions.dir ?? resolve(import.meta.dirname, "dist");
            const fixtureDirectory = resolve(outputDirectory, "fixtures", releaseId);
            await mkdir(fixtureDirectory, { recursive: true });
            await copyFile(samplePath, resolve(fixtureDirectory, "sample1.imm"));
            if (releaseId !== "") {
                const releaseDecoderDirectory = resolve(outputDirectory, "decoder", releaseId);
                await mkdir(releaseDecoderDirectory, { recursive: true });
                for (const filename of await readdir(decoderPath)) {
                    await copyFile(resolve(decoderPath, filename), resolve(releaseDecoderDirectory, filename));
                }
                await writeFile(resolve(outputDirectory, "version.json"), `${JSON.stringify({ release: releaseId }, null, 2)}\n`);
            }
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
    createReadStream(samplePath).pipe(response);
}

function releaseBootstrap(base) {
    return `(() => {
  const current = document.querySelector('meta[name="imm-release"]')?.content;
  const versionUrl = new URL(${JSON.stringify(`${base}version.json`)}, location.origin);
  versionUrl.searchParams.set("cache-bust", Date.now().toString());
  fetch(versionUrl, { cache: "no-store" })
    .then((response) => response.ok ? response.json() : Promise.reject(new Error(response.statusText)))
    .then(({ release }) => {
      if (typeof release !== "string" || release === "" || release === current) {
        if (release === current) sessionStorage.removeItem("imm-release-reload");
        return;
      }
      if (sessionStorage.getItem("imm-release-reload") === release) return;
      sessionStorage.setItem("imm-release-reload", release);
      const next = new URL(location.href);
      next.searchParams.set("__imm_release", release);
      location.replace(next);
    })
    .catch(() => {});
})();`;
}
