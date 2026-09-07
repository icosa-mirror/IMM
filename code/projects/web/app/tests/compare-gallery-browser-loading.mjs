import { spawn } from "node:child_process";
import { open, readFile, writeFile, mkdir } from "node:fs/promises";
import { resolve } from "node:path";
import { createHash } from "node:crypto";

const root = resolve(import.meta.dirname, "../../../../..");
const args = process.argv.slice(2);
const option = (name, fallback) => args.includes(name) ? args[args.indexOf(name) + 1] : fallback;
const rounds = Number(option("--rounds", "5"));
const output = resolve(option("--output", resolve(root, "artifacts/rebalance-matched-harnesses.json")));
const directory = `${output}.runs`;
await mkdir(directory, { recursive: true });
const report = { workload: "upper", seconds: 1, backgroundItems: 600, viewport: { width: 1440, height: 900 },
    audio: false, rounds, results: [] };
for (let round = 0; round < rounds; round++) {
    for (const harness of round % 2 ? ["gallery", "app"] : ["app", "gallery"]) {
        const path = resolve(directory, `${harness}.${round}.json`);
        const script = harness === "app" ? "web-player-delivery-performance.mjs" : "gallery-viewer-performance.mjs";
        const parameters = harness === "app" ? ["--evaluation-comparison", "--match-gallery"] : ["--loading-comparison"];
        const log = await open(resolve(directory, `${harness}.${round}.log`), "w");
        let code;
        try {
            const child = spawn(process.execPath, [resolve(import.meta.dirname, script), ...parameters,
                "--workload", "upper", "--rounds", "1", "--round-offset", String(round), "--screenshots", "--output", path],
                { cwd: root, stdio: ["ignore", log.fd, log.fd], windowsHide: true });
            code = await new Promise((done, reject) => { child.on("error", reject); child.on("exit", done); });
        } finally { await log.close(); }
        if (code !== 0) throw new Error(`${harness} round ${round + 1} failed; inspect its saved log`);
        const run = JSON.parse(await readFile(path, "utf8"));
        for (const result of run.results) {
            const resources = harness === "app"
                ? result.decoder.stagedRequests.slice(-600).map(item => [item.type, item.layerId, item.drawingId ?? null, item.packetBytes])
                : result.resources;
            const resourceHash = createHash("sha256").update(JSON.stringify(resources)).digest("hex");
            const ms = harness === "app" ? result.elapsedMs : result.completedAt - result.readyAt;
            report.results.push({ harness, round, mode: result.mode, backgroundMs: ms, resourceHash,
                resourceCount: resources.length, packetBytes: resources.reduce((sum, item) => sum + item[3], 0),
                threeRevision: harness === "app" ? result.diagnostics.threeRevision : result.threeRevision,
                pixelHash: result.pixels.hash, nonBlackPixels: result.pixels.nonBlack, reportPath: path });
            console.log(`${harness} ${result.mode} round ${round + 1}: ${ms.toFixed(1)} ms`);
        }
        await writeFile(output, JSON.stringify(report, null, 2));
    }
}
if (new Set(report.results.map(result => result.resourceHash)).size !== 1
    || report.results.some(result => result.resourceCount !== 600 || result.threeRevision !== "185")) {
    throw new Error("The harnesses used different resources or Three.js versions");
}
console.log("Matched comparison passed: same ordered resources, byte counts, and Three.js revision.");
