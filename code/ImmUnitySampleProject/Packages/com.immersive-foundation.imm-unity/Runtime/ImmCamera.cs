using UnityEngine;

namespace ImmPlayer
{
    /// <summary>Opts a game camera into IMM rendering through the installed URP renderer feature.</summary>
    [DisallowMultipleComponent]
    [ExecuteAlways]
    [RequireComponent(typeof(Camera))]
    [AddComponentMenu("IMM/IMM Camera")]
    public sealed class ImmCamera : MonoBehaviour
    {
        // One native engine owns the camera table. IDs are private integration details,
        // shared across renderer data assets, not serialized application configuration.
        private static readonly ImmCamera[] owners = new ImmCamera[256];
        private int cameraId = -1;
        private bool reportedCapacity;

        internal static bool TryAcquire(Camera camera, out int id)
        {
            id = -1;
            if (camera == null || camera.cameraType != CameraType.Game ||
                !camera.TryGetComponent<ImmCamera>(out var registration) || !registration.isActiveAndEnabled)
                return false;
            return registration.TryAcquireId(out id);
        }

        private bool TryAcquireId(out int id)
        {
            if (cameraId >= 0 && ReferenceEquals(owners[cameraId], this))
            {
                id = cameraId;
                return true;
            }
            for (int i = 0; i < owners.Length; ++i)
            {
                if (owners[i] != null) continue;
                owners[i] = this;
                cameraId = id = i;
                reportedCapacity = false;
                return true;
            }
            id = -1;
            if (!reportedCapacity)
            {
                Debug.LogError("[IMM_RENDER_GRAPH] All 256 native camera slots are in use.", this);
                reportedCapacity = true;
            }
            return false;
        }

        private void OnDisable()
        {
            // Already-recorded passes retain their ID and immutable submission data.
            // A later use of this slot follows those events on the same rendering queue.
            if (cameraId >= 0 && ReferenceEquals(owners[cameraId], this)) owners[cameraId] = null;
            cameraId = -1;
            reportedCapacity = false;
        }
    }
}
