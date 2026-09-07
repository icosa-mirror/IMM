import { open } from "node:fs/promises";

export async function startPerformanceTrace(page, path) {
    const session = await page.context().newCDPSession(page);
    await session.send("Tracing.start", {
        categories: "devtools.timeline,v8,blink.user_timing,disabled-by-default-devtools.timeline,disabled-by-default-v8.cpu_profiler",
        transferMode: "ReturnAsStream",
    });
    return async () => {
        const complete = new Promise(resolve => session.once("Tracing.tracingComplete", resolve));
        await session.send("Tracing.end");
        const { stream } = await complete;
        const output = await open(path, "w");
        try {
            for (;;) {
                const chunk = await session.send("IO.read", { handle: stream });
                await output.write(chunk.base64Encoded ? Buffer.from(chunk.data, "base64") : chunk.data);
                if (chunk.eof) break;
            }
        } finally {
            await output.close();
            await session.send("IO.close", { handle: stream });
            await session.detach();
        }
    };
}
