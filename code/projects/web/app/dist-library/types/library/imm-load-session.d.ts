import { type ImmStagedDelta } from "../decoder-client";
import type { ImmDocument } from "../format/imm-document";
import { type StagedLoadWork } from "../staged-loading";
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
}
export declare class IMMLoadSession {
    #private;
    constructor(options: IMMLoadSessionOptions);
    load(source: ArrayBuffer): Promise<IMMInitialLoad>;
    continue(document: ImmDocument, work: readonly StagedLoadWork[], onDelta: (delta: ImmStagedDelta, item: StagedLoadWork) => void | Promise<void>): Promise<void>;
    release(): Promise<void>;
    dispose(): void;
}
export declare function applyStagedDelta(document: ImmDocument, delta: ImmStagedDelta): void;
//# sourceMappingURL=imm-load-session.d.ts.map