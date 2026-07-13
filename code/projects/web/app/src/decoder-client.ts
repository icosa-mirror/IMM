import { releaseAssetUrl } from "./release-assets";

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
    error?: DecoderError;
}

interface PendingRequest {
    resolve: (value: ImmDocumentSummary | ImmDocument | undefined) => void;
    reject: (error: Error) => void;
    allowEmpty: boolean;
}


export class ImmDecoderClient {
    readonly #worker: Worker;
    readonly #pending = new Map<number, PendingRequest>();
    #nextRequestId = 1;
    #disposed = false;

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
        return this.#requestDocument("decode", { source }, [source]);
    }

    openMetadata(source: ArrayBuffer): Promise<ImmDocument> {
        return this.#requestDocument("openMetadata", { source }, [source]);
    }

    decodeDrawing(layerId: number, drawingId: number): Promise<ImmDocument> {
        return this.#requestDocument("decodeDrawing", { layerId, drawingId });
    }

    decodeLayerAsset(layerId: number): Promise<ImmDocument> {
        return this.#requestDocument("decodeLayerAsset", { layerId });
    }

    fallbackEager(): Promise<ImmDocument> {
        return this.#requestDocument("fallbackEager", {});
    }

    release(): Promise<void> {
        return this.#request<void>("release", {}, [], true);
    }

    #requestDocument(
        type: "decode" | "openMetadata" | "decodeDrawing" | "decodeLayerAsset" | "fallbackEager",
        payload: Record<string, unknown>,
        transfer: Transferable[] = [],
    ): Promise<ImmDocument> {
        return this.#request<ImmDocument>(type, payload, transfer);
    }

    #request<T>(
        type: "inspect" | "decode" | "openMetadata" | "decodeDrawing" | "decodeLayerAsset" | "fallbackEager" | "release",
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

        const value = response.summary ?? response.document;
        if (response.ok && (value !== undefined || pending.allowEmpty)) {
            pending.resolve(value);
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
import type { ImmDocument } from "./format/imm-document";
