import * as THREE from "three";
import { IMMAsset } from "./imm-asset";
export interface IMMLoaderOptions {
    signal?: AbortSignal;
    audio?: boolean;
    audioContext?: AudioContext;
    initialBufferSeconds?: number;
    stagedLoading?: boolean;
    onBackgroundError?: (error: unknown) => void;
}
export declare class IMMLoader extends THREE.Loader {
    #private;
    setRenderer(renderer: THREE.WebGLRenderer): this;
    setDecoderWorkerURL(url: string | URL): this;
    setInitialBufferSeconds(seconds: number): this;
    setStagedLoading(enabled: boolean): this;
    setAudio(enabled: boolean): this;
    load(url: string, onLoad: (asset: IMMAsset) => void, onProgress?: (event: ProgressEvent) => void, onError?: (error: unknown) => void): void;
    loadAsync(url: string, onProgress?: (event: ProgressEvent) => void): Promise<IMMAsset>;
    loadWithOptions(url: string, options?: IMMLoaderOptions, onProgress?: (event: ProgressEvent) => void): Promise<IMMAsset>;
    loadFile(file: File, options?: IMMLoaderOptions): Promise<IMMAsset>;
    parseAsync(source: ArrayBuffer | ArrayBufferView, options?: IMMLoaderOptions, onProgress?: (event: ProgressEvent) => void): Promise<IMMAsset>;
}
//# sourceMappingURL=imm-loader.d.ts.map