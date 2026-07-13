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
    resolve: (value: ImmDocumentSummary | ImmDocument) => void;
    reject: (error: Error) => void;
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
        return this.#request<ImmDocumentSummary>("inspect", source);
    }

    decode(source: ArrayBuffer): Promise<ImmDocument> {
        return this.#request<ImmDocument>("decode", source);
    }

    #request<T extends ImmDocumentSummary | ImmDocument>(type: "inspect" | "decode", source: ArrayBuffer): Promise<T> {
        if (this.#disposed) {
            return Promise.reject(new Error("IMM decoder client is disposed"));
        }

        const requestId = this.#nextRequestId++;
        const result = new Promise<T>((resolve, reject) => {
            this.#pending.set(requestId, {
                resolve: (value) => resolve(value as T),
                reject,
            });
        });
        this.#worker.postMessage({ requestId, type, source }, [source]);
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
        if (response.ok && value !== undefined) {
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
