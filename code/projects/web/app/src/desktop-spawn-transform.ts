import type { ImmTransform } from "./format/imm-document";

export const IMM_MONO_EYE_HEIGHT_METERS = 1.6;

export function desktopSpawnTransform(
    transform: ImmTransform,
    tracking: "eye" | "floor" | null | undefined,
): ImmTransform {
    if (tracking !== "floor") return transform;
    const localEyeOffset = flipVector([0, IMM_MONO_EYE_HEIGHT_METERS, 0], transform.flip)
        .map((component) => component * transform.scale) as ImmTransform["translation"];
    const worldEyeOffset = rotateVector(localEyeOffset, transform.rotation);
    return {
        ...transform,
        rotation: [...transform.rotation],
        translation: transform.translation.map(
            (component, index) => component + (worldEyeOffset[index] ?? 0),
        ) as ImmTransform["translation"],
    };
}

function flipVector(value: ImmTransform["translation"], flip: number): ImmTransform["translation"] {
    return [
        flip === 1 ? -value[0] : value[0],
        flip === 2 ? -value[1] : value[1],
        flip === 3 ? -value[2] : value[2],
    ];
}

function rotateVector(
    value: ImmTransform["translation"],
    rotation: ImmTransform["rotation"],
): ImmTransform["translation"] {
    const [x, y, z] = value;
    const [qx, qy, qz, qw] = rotation;
    const ix = qw * x + qy * z - qz * y;
    const iy = qw * y + qz * x - qx * z;
    const iz = qw * z + qx * y - qy * x;
    const iw = -qx * x - qy * y - qz * z;
    return [
        ix * qw + iw * -qx + iy * -qz - iz * -qy,
        iy * qw + iw * -qy + iz * -qx - ix * -qz,
        iz * qw + iw * -qz + ix * -qy - iy * -qx,
    ];
}
