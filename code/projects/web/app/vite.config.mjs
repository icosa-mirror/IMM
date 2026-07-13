import { resolve } from "node:path";
import { defineConfig } from "vite";

export default defineConfig({
    build: {
        rollupOptions: {
            input: {
                player: resolve(import.meta.dirname, "index.html"),
                embed: resolve(import.meta.dirname, "embed.html"),
            },
        },
    },
});
