import {
    ImmDecoderClient,
    type ImmStagedDelta,
    type ImmStagedRequestMetrics,
} from "../decoder-client";
import type { ImmDocument } from "../format/imm-document";
import { createNativeLoadOrder, type StagedLoadWork } from "../staged-loading";

export type IMMLoadStage = "metadata" | "initial" | "background" | "complete" | "fallback";

export interface IMMLoadProgress {
    stage: IMMLoadStage;
    loaded: number;
    total: number;
    item?: StagedLoadWork;
}

export interface IMMLoadSessionOptions {
    decoderWorkerURL: string | URL;
    initialBufferSeconds?: number;
    stagedLoading?: boolean;
    onProgress?: (progress: IMMLoadProgress) => void;
}

export interface IMMInitialLoad {
    document: ImmDocument;
    remainingWork: StagedLoadWork[];
    telemetry: IMMLoadTelemetry;
}

export interface IMMLoadTelemetry {
    requestedMode: "eager" | "staged";
    effectiveMode: "eager" | "staged";
    fallbackReason: string | null;
    requestedItems: number;
    initiallyLoadedItems: number;
    deferredItems: number;
    backgroundCompletedItems: number;
    requests: ImmStagedRequestMetrics[];
}

export class IMMLoadSession {
    readonly #decoder: ImmDecoderClient;
    readonly #initialBufferSeconds: number;
    readonly #stagedLoading: boolean;
    readonly #onProgress?: (progress: IMMLoadProgress) => void;
    #disposed = false;
    #released = false;
    readonly #telemetry: IMMLoadTelemetry;

    constructor(options: IMMLoadSessionOptions) {
        this.#decoder = new ImmDecoderClient(String(options.decoderWorkerURL));
        this.#initialBufferSeconds = options.initialBufferSeconds ?? 5;
        this.#stagedLoading = options.stagedLoading ?? true;
        this.#onProgress = options.onProgress;
        this.#telemetry = {
            requestedMode: this.#stagedLoading ? "staged" : "eager",
            effectiveMode: this.#stagedLoading ? "staged" : "eager",
            fallbackReason: null,
            requestedItems: 0,
            initiallyLoadedItems: 0,
            deferredItems: 0,
            backgroundCompletedItems: 0,
            requests: [],
        };
    }

    async load(source: ArrayBuffer): Promise<IMMInitialLoad> {
        this.#throwIfDisposed();
        this.#onProgress?.({ stage: "metadata", loaded: 0, total: 1 });
        if (!this.#stagedLoading) {
            const document = await this.#decoder.decode(source);
            this.#throwIfDisposed();
            this.#onProgress?.({ stage: "complete", loaded: 1, total: 1 });
            await this.release();
            return { document, remainingWork: [], telemetry: this.#telemetry };
        }

        let document: ImmDocument;
        try {
            document = await this.#decoder.openMetadata(source);
            this.#throwIfDisposed();
            const work = createNativeLoadOrder(document, this.#initialBufferSeconds);
            const initialWork = work.filter((item) => item.initial);
            const remainingWork = work.filter((item) => !item.initial);
            this.#telemetry.requestedItems = work.length;
            this.#telemetry.deferredItems = remainingWork.length;
            for (let index = 0; index < initialWork.length; index++) {
                this.#throwIfDisposed();
                const item = initialWork[index]!;
                this.#onProgress?.({ stage: "initial", loaded: index, total: initialWork.length, item });
                const delta = await this.#decode(item);
                applyStagedDelta(document, delta);
                this.#telemetry.requests.push(delta.metrics);
                this.#telemetry.initiallyLoadedItems++;
            }
            this.#onProgress?.({
                stage: remainingWork.length === 0 ? "complete" : "initial",
                loaded: initialWork.length,
                total: initialWork.length,
            });
            if (remainingWork.length === 0) await this.release();
            return { document, remainingWork, telemetry: this.#telemetry };
        } catch (stagedError) {
            this.#throwIfDisposed();
            this.#onProgress?.({ stage: "fallback", loaded: 0, total: 1 });
            const fallbackReason = stagedError instanceof Error ? stagedError.message : String(stagedError);
            this.#telemetry.effectiveMode = "eager";
            this.#telemetry.fallbackReason = fallbackReason;
            document = await this.#decoder.fallbackEager(fallbackReason).catch(() => { throw stagedError; });
            this.#throwIfDisposed();
            this.#onProgress?.({ stage: "complete", loaded: 1, total: 1 });
            await this.release();
            return { document, remainingWork: [], telemetry: this.#telemetry };
        }
    }

    async continue(
        document: ImmDocument,
        work: readonly StagedLoadWork[],
        onDelta: (delta: ImmStagedDelta, item: StagedLoadWork) => void | Promise<void>,
    ): Promise<void> {
        try {
            for (let index = 0; index < work.length; index++) {
                this.#throwIfDisposed();
                const item = work[index]!;
                this.#onProgress?.({ stage: "background", loaded: index, total: work.length, item });
                const delta = await this.#decode(item);
                this.#throwIfDisposed();
                applyStagedDelta(document, delta);
                this.#telemetry.requests.push(delta.metrics);
                this.#telemetry.backgroundCompletedItems++;
                const adapterStartedAt = performance.now();
                await onDelta(delta, item);
                delta.metrics.adapterMs += performance.now() - adapterStartedAt;
            }
            this.#onProgress?.({ stage: "complete", loaded: work.length, total: work.length });
            await this.release();
        } catch (error) {
            if (this.#disposed) return;
            throw error;
        }
    }

    async release(): Promise<void> {
        if (this.#released || this.#disposed) return;
        this.#released = true;
        await this.#decoder.release();
    }

    dispose(): void {
        if (this.#disposed) return;
        this.#disposed = true;
        this.#decoder.dispose();
    }

    #decode(work: StagedLoadWork): Promise<ImmStagedDelta> {
        return work.type === "drawing"
            ? this.#decoder.decodeDrawing(work.layerId, work.drawingId)
            : this.#decoder.decodeLayerAsset(work.layerId);
    }

    #throwIfDisposed(): void {
        if (this.#disposed) throw new DOMException("IMM load was aborted", "AbortError");
    }
}

export function applyStagedDelta(document: ImmDocument, delta: ImmStagedDelta): void {
    const startedAt = performance.now();
    const layer = document.layers.find((candidate) => candidate.id === delta.layerId);
    if (layer === undefined) throw new Error(`Staged decoder returned unknown layer ${delta.layerId}`);
    if (delta.type === "drawing") {
        if (delta.drawingId < 0 || delta.drawingId >= layer.drawings.length) {
            throw new Error(`Staged decoder returned unknown drawing ${delta.layerId}/${delta.drawingId}`);
        }
        layer.drawings[delta.drawingId] = delta.drawing;
        delta.metrics.adapterMs += performance.now() - startedAt;
        return;
    }
    if (delta.picture !== undefined) layer.picture = delta.picture;
    if (delta.sound !== undefined) layer.sound = delta.sound;
    delta.metrics.adapterMs += performance.now() - startedAt;
}
