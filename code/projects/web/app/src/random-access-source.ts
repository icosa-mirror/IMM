export interface RandomAccessSource {
    readonly size: bigint;
    read(offset: bigint, length: number, signal: AbortSignal): Promise<ArrayBuffer>;
}

export class ArrayBufferRandomAccessSource implements RandomAccessSource {
    readonly size: bigint;
    readonly #bytes: Uint8Array;

    constructor(source: ArrayBuffer | ArrayBufferView) {
        this.#bytes = source instanceof ArrayBuffer
            ? new Uint8Array(source)
            : new Uint8Array(source.buffer, source.byteOffset, source.byteLength);
        this.size = BigInt(this.#bytes.byteLength);
    }

    async read(offset: bigint, length: number, signal: AbortSignal): Promise<ArrayBuffer> {
        signal.throwIfAborted();
        const start = checkedRangeStart(this.size, offset, length);
        const result = this.#bytes.slice(start, start + length).buffer;
        signal.throwIfAborted();
        return result;
    }
}

export class FileRandomAccessSource implements RandomAccessSource {
    readonly size: bigint;
    readonly #file: File;

    constructor(file: File) {
        this.#file = file;
        this.size = BigInt(file.size);
    }

    async read(offset: bigint, length: number, signal: AbortSignal): Promise<ArrayBuffer> {
        signal.throwIfAborted();
        const start = checkedRangeStart(this.size, offset, length);
        const result = await this.#file.slice(start, start + length).arrayBuffer();
        signal.throwIfAborted();
        return result;
    }
}

function checkedRangeStart(size: bigint, offset: bigint, length: number): number {
    if (offset < 0n) throw new RangeError("Random-access offset cannot be negative");
    if (!Number.isSafeInteger(length) || length < 0) {
        throw new RangeError("Random-access length must be a non-negative safe integer");
    }
    const end = offset + BigInt(length);
    if (end > size) {
        throw new RangeError(`Random-access range ${offset}+${length} exceeds source size ${size}`);
    }
    const start = Number(offset);
    if (!Number.isSafeInteger(start)) {
        throw new RangeError("Random-access offset exceeds JavaScript's safe integer range");
    }
    return start;
}
