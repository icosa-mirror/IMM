import { releaseAssetUrl } from "./release-assets";
import {
    assertSupportedDelta,
    assertSupportedDocument,
    assertSupportedSummary,
} from "./format/validate-imm-document";

export interface ImmDocumentSummary {
    schemaVersion: number;
    formatVersion: number;
    sourceSize: bigint;
    chunkCount: number;
    chunkFlags: number;
    sequenceType: number;
    sequenceCapabilities: number;
    sequenceOffset: bigint;
    sequenceSize: bigint;
    resourceTableOffset: bigint;
    resourceTableSize: bigint;
    assetCount: number;
}

interface DecoderError {
    status: number;
    byteOffset: bigint;
    message: string;
}

interface DecoderResponse {
    requestId: number;
    ok: boolean;
    summary?: ImmDocumentSummary;
    document?: ImmDocument;
    delta?: ImmStagedDelta;
    diagnostics?: ImmDecoderDiagnostics;
    error?: DecoderError;
    sentAtEpochMs?: number;
}

interface PendingRequest {
    resolve: (value: ImmDocumentSummary | ImmDocument | ImmStagedDelta | ImmDecoderDiagnostics | undefined) => void;
    reject: (error: Error) => void;
    allowEmpty: boolean;
}


export class ImmDecoderClient {
    readonly #worker: Worker;
    readonly #pending = new Map<number, PendingRequest>();
    #nextRequestId = 1;
    #disposed = false;
    #stagedRequests: ImmStagedRequestMetrics[] = [];

    constructor(workerUrl = releaseAssetUrl("decoder", "imm-web-decoder-worker.mjs")) {
        this.#worker = new Worker(workerUrl, { type: "module", name: "imm-decoder" });
        this.#worker.addEventListener("message", (event: MessageEvent<DecoderResponse>) => {
            this.#handleResponse(event.data);
        });
        this.#worker.addEventListener("error", (event) => {
            this.#failAll(new Error(`IMM decoder worker failed: ${event.message}`));
        });
    }

    inspect(source: ArrayBuffer): Promise<ImmDocumentSummary> {
        return this.#request<ImmDocumentSummary>("inspect", { source }, [source]);
    }

    decode(source: ArrayBuffer): Promise<ImmDocument> {
        this.#stagedRequests = [];
        return this.#requestDocument("decode", { source }, [source]);
    }

    openMetadata(source: ArrayBuffer): Promise<ImmDocument> {
        this.#stagedRequests = [];
        return this.#requestDocument("openMetadata", { source }, [source]);
    }

    decodeDrawing(layerId: number, drawingId: number): Promise<ImmStagedDelta> {
        return this.#request<ImmStagedDelta>("decodeDrawing", { layerId, drawingId });
    }

    decodeLayerAsset(layerId: number): Promise<ImmStagedDelta> {
        return this.#request<ImmStagedDelta>("decodeLayerAsset", { layerId });
    }

    fallbackEager(reason: string): Promise<ImmDocument> {
        return this.#requestDocument("fallbackEager", { fallbackReason: reason });
    }

    diagnostics(): Promise<ImmDecoderDiagnostics> {
        return this.#request<ImmDecoderDiagnostics>("diagnostics", {});
    }

    release(): Promise<void> {
        return this.#request<void>("release", {}, [], true);
    }

    #requestDocument(
        type: "decode" | "openMetadata" | "fallbackEager",
        payload: Record<string, unknown>,
        transfer: Transferable[] = [],
    ): Promise<ImmDocument> {
        return this.#request<ImmDocument>(type, payload, transfer);
    }

    #request<T>(
        type: "inspect" | "decode" | "openMetadata" | "decodeDrawing" | "decodeLayerAsset" | "fallbackEager" | "diagnostics" | "release",
        payload: Record<string, unknown>,
        transfer: Transferable[] = [],
        allowEmpty = false,
    ): Promise<T> {
        if (this.#disposed) {
            return Promise.reject(new Error("IMM decoder client is disposed"));
        }

        const requestId = this.#nextRequestId++;
        const result = new Promise<T>((resolve, reject) => {
            this.#pending.set(requestId, {
                resolve: (value) => resolve(value as T),
                reject,
                allowEmpty,
            });
        });
        this.#worker.postMessage({ requestId, type, ...payload }, transfer);
        return result;
    }

    dispose(): void {
        if (this.#disposed) {
            return;
        }
        this.#disposed = true;
        this.#worker.terminate();
        this.#failAll(new Error("IMM decoder client was disposed"));
    }

    #handleResponse(response: DecoderResponse): void {
        const pending = this.#pending.get(response.requestId);
        if (pending === undefined) {
            return;
        }
        this.#pending.delete(response.requestId);

        if (response.delta?.metrics !== undefined && response.sentAtEpochMs !== undefined) {
            response.delta.metrics.transferMs = Math.max(
                0,
                performance.timeOrigin + performance.now() - response.sentAtEpochMs,
            );
            this.#stagedRequests.push(response.delta.metrics);
        }
        if (response.diagnostics !== undefined) {
            response.diagnostics.stagedRequests = this.#stagedRequests.map((metrics) => ({ ...metrics }));
        }

        const value = response.summary ?? response.document ?? response.delta ?? response.diagnostics;
        if (response.ok && (value !== undefined || pending.allowEmpty)) {
            try {
                if (response.summary !== undefined) assertSupportedSummary(response.summary);
                if (response.document !== undefined) assertSupportedDocument(response.document);
                if (response.delta !== undefined) assertSupportedDelta(response.delta);
                pending.resolve(value);
            } catch (error) {
                pending.reject(error instanceof Error ? error : new Error(String(error)));
            }
            return;
        }

        const error = response.error;
        const detail = error === undefined
            ? "Decoder returned an invalid response"
            : `${error.message} (status ${error.status}, byte ${error.byteOffset})`;
        pending.reject(new Error(detail));
    }

    #failAll(error: Error): void {
        for (const pending of this.#pending.values()) {
            pending.reject(error);
        }
        this.#pending.clear();
    }
}
import type { ImmDocument, ImmDrawing, ImmPicture, ImmSound } from "./format/imm-document";

export type ImmStagedDelta =
    | { type: "drawing"; layerId: number; drawingId: number; drawing: ImmDrawing; metrics: ImmStagedRequestMetrics }
    | { type: "asset"; layerId: number; picture?: ImmPicture; sound?: ImmSound; metrics: ImmStagedRequestMetrics };

export interface ImmStagedRequestMetrics {
    type: "drawing" | "asset";
    layerId: number;
    drawingId?: number;
    decodeMs: number;
    nativeBuildMs: number;
    wasmCopyMs: number;
    packetParseMs: number;
    transferMs: number;
    adapterMs: number;
    lookupMs: number;
    packetBytes: number;
}

export interface ImmDecoderDiagnostics {
    peakWasmHeapBytes: number;
    peakPaintPacketBytes: number;
    geometryTransferBytes: number;
    requestedLoadMode: "eager" | "staged";
    effectiveLoadMode: "eager" | "staged";
    fallbackReason: string | null;
    stagedRequests: ImmStagedRequestMetrics[];
}
