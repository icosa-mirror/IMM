/** Read rendered scene pixels after measurement; exclude edge UI from the check. */
export async function captureScenePixels(page, path) {
    const screenshot = await page.screenshot(path ? { path } : {});
    const pixels = await page.evaluate(async base64 => {
        const image = new Image();
        image.src = `data:image/png;base64,${base64}`;
        await image.decode();
        const canvas = document.createElement("canvas");
        canvas.width = image.width; canvas.height = image.height;
        const context = canvas.getContext("2d");
        context.drawImage(image, 0, 0);
        // Exclude Gallery's edge controls, which can be visible over a black scene.
        const pixels = context.getImageData(Math.floor(image.width * .1), Math.floor(image.height * .1),
            Math.floor(image.width * .8), Math.floor(image.height * .8)).data;
        let nonBlack = 0;
        for (let i = 0; i < pixels.length; i += 4) if (Math.max(pixels[i], pixels[i + 1], pixels[i + 2]) > 24) nonBlack++;
        const hash = await crypto.subtle.digest("SHA-256", pixels);
        return { nonBlack, total: pixels.length / 4, hash: [...new Uint8Array(hash)].map(x => x.toString(16).padStart(2, "0")).join("") };
    }, screenshot.toString("base64"));
    if (pixels.nonBlack < pixels.total * .001) throw new Error("Loading comparison rendered a black scene");
    return pixels;
}
