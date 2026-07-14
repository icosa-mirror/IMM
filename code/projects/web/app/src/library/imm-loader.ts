import * as THREE from "three";
import { IMMAsset, type IMMAssetOptions } from "./imm-asset";
import { IMMLoadSession, type IMMLoadProgress } from "./imm-load-session";

export interface IMMLoaderOptions {
    signal?: AbortSignal;
    audio?: boolean;
    audioContext?: AudioContext;
    initialBufferSeconds?: number;
    stagedLoading?: boolean;
    onBackgroundError?: (error: unknown) => void;
}

export class IMMLoader extends THREE.Loader {
    #renderer?: THREE.WebGLRenderer;
    #decoderWorkerURL?: string | URL;
    #initialBufferSeconds = 5;
    #stagedLoading = true;
    #audio = true;

    setRenderer(renderer: THREE.WebGLRenderer): this { this.#renderer = renderer; return this; }
    setDecoderWorkerURL(url: string | URL): this { this.#decoderWorkerURL = url; return this; }
    setInitialBufferSeconds(seconds: number): this { this.#initialBufferSeconds = Math.max(0, seconds); return this; }
    setStagedLoading(enabled: boolean): this { this.#stagedLoading = enabled; return this; }
    setAudio(enabled: boolean): this { this.#audio = enabled; return this; }

    load(
        url: string,
        onLoad: (asset: IMMAsset) => void,
        onProgress?: (event: ProgressEvent) => void,
        onError?: (error: unknown) => void,
    ): void {
        void this.loadAsync(url, onProgress).then(onLoad, onError);
    }

    async loadAsync(url: string, onProgress?: (event: ProgressEvent) => void): Promise<IMMAsset> {
        return this.loadWithOptions(url, {}, onProgress);
    }

    async loadWithOptions(
        url: string,
        options: IMMLoaderOptions = {},
        onProgress?: (event: ProgressEvent) => void,
    ): Promise<IMMAsset> {
        const itemURL = this.manager.resolveURL(this.path + url);
        this.manager.itemStart(itemURL);
        try {
            const response = await fetch(itemURL, {
                signal: options.signal,
                headers: this.requestHeader,
                credentials: this.withCredentials ? "include" : "same-origin",
            });
            if (!response.ok) throw new Error(`IMM fetch failed: HTTP ${response.status} for ${itemURL}`);
            const total = Number(response.headers.get("content-length")) || 0;
            const source = await readResponse(response, total, onProgress);
            return await this.parseAsync(source, options, onProgress);
        } catch (error) {
            this.manager.itemError(itemURL);
            throw error;
        } finally {
            this.manager.itemEnd(itemURL);
        }
    }

    async loadFile(file: File, options: IMMLoaderOptions = {}): Promise<IMMAsset> {
        return this.parseAsync(await file.arrayBuffer(), options);
    }

    async parseAsync(
        source: ArrayBuffer | ArrayBufferView,
        options: IMMLoaderOptions = {},
        onProgress?: (event: ProgressEvent) => void,
    ): Promise<IMMAsset> {
        const renderer = this.#renderer;
        if (renderer === undefined) throw new Error("IMMLoader requires setRenderer(renderer) before loading");
        const decoderWorkerURL = this.#decoderWorkerURL;
        if (decoderWorkerURL === undefined) throw new Error("IMMLoader requires setDecoderWorkerURL(url) before loading");
        if (options.signal?.aborted) throw new DOMException("IMM load was aborted", "AbortError");
        const session = new IMMLoadSession({
            decoderWorkerURL,
            initialBufferSeconds: options.initialBufferSeconds ?? this.#initialBufferSeconds,
            stagedLoading: options.stagedLoading ?? this.#stagedLoading,
            onProgress: (progress) => onProgress?.(progressEvent(progress)),
        });
        const abort = () => session.dispose();
        options.signal?.addEventListener("abort", abort, { once: true });
        try {
            const bytes = source instanceof ArrayBuffer
                ? source
                : source.buffer.slice(source.byteOffset, source.byteOffset + source.byteLength) as ArrayBuffer;
            const initial = await session.load(bytes);
            const assetOptions: IMMAssetOptions = {
                renderer,
                audio: options.audio ?? this.#audio,
                audioContext: options.audioContext,
                onBackgroundError: options.onBackgroundError,
            };
            return new IMMAsset(initial.document, session, initial.remainingWork, assetOptions);
        } catch (error) {
            session.dispose();
            throw error;
        } finally {
            options.signal?.removeEventListener("abort", abort);
        }
    }
}

async function readResponse(
    response: Response,
    total: number,
    onProgress?: (event: ProgressEvent) => void,
): Promise<ArrayBuffer> {
    if (response.body === null || onProgress === undefined) return response.arrayBuffer();
    const reader = response.body.getReader();
    const chunks: Uint8Array[] = [];
    let loaded = 0;
    while (true) {
        const result = await reader.read();
        if (result.done) break;
        chunks.push(result.value);
        loaded += result.value.byteLength;
        onProgress(new ProgressEvent("progress", { lengthComputable: total > 0, loaded, total }));
    }
    const bytes = new Uint8Array(loaded);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.byteLength; }
    return bytes.buffer;
}

function progressEvent(progress: IMMLoadProgress): ProgressEvent {
    return new ProgressEvent("progress", {
        lengthComputable: progress.total > 0,
        loaded: progress.loaded,
        total: progress.total,
    });
}
