import { readFile, rm } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { validateDecoderAssets } from "./decoder-assets.mjs";

const appDirectory = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repositoryDirectory = resolve(appDirectory, "../../../..");
const decoderSource = resolve(repositoryDirectory, "code/projects/web/decoder");
const decoderBuild = resolve(repositoryDirectory, "build/web-decoder-wasm-library");
const decoderOutput = resolve(appDirectory, "public/decoder");
const decoderReady = process.argv.includes("--decoder-ready");

if (!decoderReady) {
    await rm(decoderOutput, { recursive: true, force: true });
    const emcmake = process.platform === "win32"
        ? [requiredEnvironment("EMSDK_PYTHON"), resolve(requiredEnvironment("EMSDK"), "upstream/emscripten/emcmake.py")]
        : ["emcmake"];
    await verifyEmscriptenVersion();
    run(emcmake[0], [...emcmake.slice(1),
        "cmake",
        "-S", decoderSource,
        "-B", decoderBuild,
        "-DCMAKE_BUILD_TYPE=Release",
        `-DIMM_WEB_OUTPUT_DIRECTORY=${decoderOutput}`,
    ], repositoryDirectory);
    run("cmake", ["--build", decoderBuild, "--parallel", "2"], repositoryDirectory);
    run("ctest", ["--test-dir", decoderBuild, "--output-on-failure"], repositoryDirectory);
}

async function verifyEmscriptenVersion() {
    const pinned = (await readFile(resolve(repositoryDirectory, "code/projects/web/EMSCRIPTEN_VERSION"), "utf8")).trim();
    const emcc = process.platform === "win32"
        ? [requiredEnvironment("EMSDK_PYTHON"), resolve(requiredEnvironment("EMSDK"), "upstream/emscripten/emcc.py")]
        : ["emcc"];
    const result = spawnSync(emcc[0], [...emcc.slice(1), "--version"], { encoding: "utf8" });
    if (result.error !== undefined) throw result.error;
    if (result.status !== 0) throw new Error(result.stderr || "Unable to read the Emscripten version");
    if (!result.stdout.includes(pinned)) {
        throw new Error(`Expected Emscripten ${pinned}; activate the version pinned in code/projects/web/EMSCRIPTEN_VERSION`);
    }
}

await validateDecoderAssets(decoderOutput);
run("bun", ["run", "build:library:bundle"], appDirectory);

function run(command, args, cwd) {
    const result = spawnSync(command, args, {
        cwd,
        stdio: "inherit",
    });
    if (result.error !== undefined) throw result.error;
    if (result.status !== 0) process.exit(result.status ?? 1);
}

function requiredEnvironment(name) {
    const value = process.env[name]?.trim();
    if (value === undefined || value === "") {
        throw new Error(`${name} is unavailable; activate the pinned Emscripten environment first`);
    }
    return value;
}
