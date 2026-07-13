const releaseId = String(import.meta.env.VITE_IMM_RELEASE_ID ?? "").trim();

export function releaseAssetUrl(directory: string, filename: string): string {
    const releasePath = releaseId === "" ? "" : `${encodeURIComponent(releaseId)}/`;
    return `${import.meta.env.BASE_URL}${directory}/${releasePath}${filename}`;
}
