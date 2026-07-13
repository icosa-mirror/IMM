import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import type { ImmTransform } from "./format/imm-document";

export type CameraMode = "fly" | "orbit";

export class ImmCameraControls {
    readonly orbit: OrbitControls;
    mode: CameraMode = "fly";

    readonly #camera: THREE.PerspectiveCamera;
    readonly #element: HTMLElement;
    readonly #pressed = new Set<string>();
    readonly #pointers = new Map<number, { x: number; y: number }>();
    readonly #euler = new THREE.Euler(0, 0, 0, "YXZ");

    constructor(camera: THREE.PerspectiveCamera, element: HTMLElement) {
        this.#camera = camera;
        this.#element = element;
        this.orbit = new OrbitControls(camera, element);
        this.orbit.enableDamping = true;
        this.orbit.enabled = false;
        this.#syncEuler();
        element.addEventListener("pointerdown", this.#onPointerDown);
        element.addEventListener("pointermove", this.#onPointerMove);
        element.addEventListener("pointerup", this.#onPointerUp);
        element.addEventListener("pointercancel", this.#onPointerUp);
        element.addEventListener("wheel", this.#onWheel, { passive: false });
        window.addEventListener("keydown", this.#onKeyDown);
        window.addEventListener("keyup", this.#onKeyUp);
        window.addEventListener("blur", this.#onBlur);
    }

    setMode(mode: CameraMode): void {
        this.mode = mode;
        this.orbit.enabled = mode === "orbit";
        this.#pointers.clear();
        this.#pressed.clear();
        if (mode === "fly") this.#syncEuler();
        else this.#syncOrbitTarget();
    }

    setPose(transform: ImmTransform): void {
        this.#camera.position.fromArray(transform.translation);
        this.#camera.quaternion.fromArray(transform.rotation).normalize();
        this.#camera.scale.set(1, 1, 1);
        this.#syncEuler();
        this.#syncOrbitTarget();
        this.orbit.update();
    }

    reset(position: THREE.Vector3, target: THREE.Vector3): void {
        this.#camera.position.copy(position);
        this.#camera.quaternion.identity();
        this.#camera.scale.set(1, 1, 1);
        this.orbit.target.copy(target);
        this.#syncEuler();
        this.orbit.update();
    }

    update(deltaSeconds: number): void {
        if (this.mode === "orbit") {
            this.orbit.update();
            return;
        }
        const speed = (this.#pressed.has("ShiftLeft") || this.#pressed.has("ShiftRight") ? 6 : 2) * deltaSeconds;
        const movement = new THREE.Vector3(
            Number(this.#pressed.has("KeyD")) - Number(this.#pressed.has("KeyA")),
            Number(this.#pressed.has("KeyE")) - Number(this.#pressed.has("KeyQ")),
            Number(this.#pressed.has("KeyS")) - Number(this.#pressed.has("KeyW")),
        );
        if (movement.lengthSq() > 0) {
            movement.normalize().multiplyScalar(speed).applyQuaternion(this.#camera.quaternion);
            this.#camera.position.add(movement);
        }
    }

    dispose(): void {
        this.orbit.dispose();
        this.#element.removeEventListener("pointerdown", this.#onPointerDown);
        this.#element.removeEventListener("pointermove", this.#onPointerMove);
        this.#element.removeEventListener("pointerup", this.#onPointerUp);
        this.#element.removeEventListener("pointercancel", this.#onPointerUp);
        this.#element.removeEventListener("wheel", this.#onWheel);
        window.removeEventListener("keydown", this.#onKeyDown);
        window.removeEventListener("keyup", this.#onKeyUp);
        window.removeEventListener("blur", this.#onBlur);
    }

    #syncEuler(): void {
        this.#euler.setFromQuaternion(this.#camera.quaternion, "YXZ");
        this.#euler.z = 0;
    }

    #syncOrbitTarget(): void {
        const forward = new THREE.Vector3(0, 0, -1).applyQuaternion(this.#camera.quaternion);
        this.orbit.target.copy(this.#camera.position).add(forward.multiplyScalar(10));
    }

    #rotate(deltaX: number, deltaY: number): void {
        this.#euler.y -= deltaX * 0.004;
        this.#euler.x = THREE.MathUtils.clamp(this.#euler.x - deltaY * 0.004, -Math.PI / 2, Math.PI / 2);
        this.#camera.quaternion.setFromEuler(this.#euler);
    }

    #onPointerDown = (event: PointerEvent): void => {
        if (this.mode !== "fly" || event.button > 0) return;
        this.#pointers.set(event.pointerId, { x: event.clientX, y: event.clientY });
        this.#element.setPointerCapture(event.pointerId);
    };

    #onPointerMove = (event: PointerEvent): void => {
        if (this.mode !== "fly") return;
        const previous = this.#pointers.get(event.pointerId);
        if (previous === undefined) return;
        const deltaX = event.clientX - previous.x;
        const deltaY = event.clientY - previous.y;
        this.#pointers.set(event.pointerId, { x: event.clientX, y: event.clientY });
        if (this.#pointers.size === 1) {
            this.#rotate(deltaX, deltaY);
        } else {
            const movement = new THREE.Vector3(deltaX, 0, deltaY).multiplyScalar(0.006);
            movement.applyQuaternion(this.#camera.quaternion);
            this.#camera.position.add(movement);
        }
    };

    #onPointerUp = (event: PointerEvent): void => {
        this.#pointers.delete(event.pointerId);
    };

    #onWheel = (event: WheelEvent): void => {
        if (this.mode !== "fly") return;
        event.preventDefault();
        const forward = new THREE.Vector3(0, 0, -1).applyQuaternion(this.#camera.quaternion);
        this.#camera.position.addScaledVector(forward, -event.deltaY * 0.002);
    };

    #onKeyDown = (event: KeyboardEvent): void => {
        if (this.mode !== "fly" || isFormControl(event.target)) return;
        this.#pressed.add(event.code);
    };

    #onKeyUp = (event: KeyboardEvent): void => {
        this.#pressed.delete(event.code);
    };

    #onBlur = (): void => {
        this.#pressed.clear();
        this.#pointers.clear();
    };
}

function isFormControl(target: EventTarget | null): boolean {
    return target instanceof HTMLInputElement || target instanceof HTMLSelectElement || target instanceof HTMLTextAreaElement;
}
