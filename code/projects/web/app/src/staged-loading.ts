import {
    IMM_ANIM_VISIBILITY,
    IMM_LAYER_PAINT,
    type ImmDocument,
    type ImmLayer,
} from "./format/imm-document";

const IMM_LAYER_MODEL = 3;
const IMM_LAYER_PICTURE = 4;
const IMM_LAYER_SOUND = 5;
const IMM_LAYER_SPAWN = 8;
const ASSET_LAYER_TYPES = new Set([
    IMM_LAYER_MODEL,
    IMM_LAYER_PICTURE,
    IMM_LAYER_SOUND,
    IMM_LAYER_SPAWN,
]);

export type StagedLoadWork =
    | { type: "drawing"; layerId: number; drawingId: number; neededTicks: number; initial: boolean }
    | { type: "asset"; layerId: number; neededTicks: number; initial: boolean };

type OrderedWork =
    | { type: "drawing"; layerId: number; drawingId: number; neededTicks: number; order: number }
    | { type: "asset"; layerId: number; neededTicks: number; order: number };

function firstVisibilityTicks(layer: ImmLayer): number {
    return layer.keys.find((key) => key.property === IMM_ANIM_VISIBILITY)?.timeTicks ?? 0;
}

function rootTimelineChild(
    layer: ImmLayer,
    layersById: ReadonlyMap<number, ImmLayer>,
    rootId: number,
): ImmLayer {
    let current = layer;
    const visited = new Set<number>();
    while (current.parentId !== rootId && current.parentId >= 0 && !visited.has(current.id)) {
        visited.add(current.id);
        const parent = layersById.get(current.parentId);
        if (parent === undefined) break;
        current = parent;
    }
    return current;
}

export function createNativeLoadOrder(document: ImmDocument, bufferSeconds = 5): StagedLoadWork[] {
    const layersById = new Map(document.layers.map((layer) => [layer.id, layer]));
    const root = document.layers.find((layer) => layer.parentId < 0);
    const rootId = root?.id ?? -1;
    const bufferTicks = Math.max(0, bufferSeconds) * document.ticksPerSecond;
    const work: OrderedWork[] = [];
    let order = 0;

    for (const layer of document.layers) {
        const layerNeededTicks = firstVisibilityTicks(rootTimelineChild(layer, layersById, rootId));
        if (layer.type === IMM_LAYER_PAINT) {
            for (let drawingId = 0; drawingId < layer.drawings.length; drawingId++) {
                const firstFrame = layer.frameBuffer.findIndex((value) => value === drawingId);
                const frameTicks = firstFrame < 0 || layer.frameRate <= 0
                    ? 0
                    : firstFrame * document.ticksPerSecond / layer.frameRate;
                work.push({
                    type: "drawing",
                    layerId: layer.id,
                    drawingId,
                    neededTicks: layerNeededTicks + frameTicks,
                    order: order++,
                });
            }
        } else if (ASSET_LAYER_TYPES.has(layer.type)) {
            work.push({ type: "asset", layerId: layer.id, neededTicks: layerNeededTicks, order: order++ });
        }
    }

    work.sort((left, right) => left.neededTicks - right.neededTicks || left.order - right.order);
    return work.map(({ order: _order, ...item }) => ({
        ...item,
        initial: item.neededTicks <= bufferTicks ||
            item.type === "asset" && layersById.get(item.layerId)?.type === IMM_LAYER_SPAWN,
    } as StagedLoadWork));
}
