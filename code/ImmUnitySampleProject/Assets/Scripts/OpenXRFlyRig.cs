using UnityEngine;
using UnityEngine.XR;

namespace ImmPlayer
{
    /// <summary>
    /// Simple OpenXR fly locomotion with snap turn.
    /// </summary>
    public class OpenXRFlyRig : MonoBehaviour
    {
        [Header("References")]
        [SerializeField] private Transform rigRoot;
        [SerializeField] private Camera headCamera;

        [Header("Movement")]
        [SerializeField] private float moveSpeed = 2.0f;
        [SerializeField] private float verticalSpeed = 1.5f;
        [SerializeField] private float deadzone = 0.15f;

        [Header("Snap Turn")]
        [SerializeField] private float snapDegrees = 30.0f;
        [SerializeField] private float snapCooldown = 0.25f;
        [SerializeField] private float snapDeadzone = 0.5f;

        private float _nextSnapTime;

        private void Reset()
        {
            rigRoot = transform;
            headCamera = Camera.main;
        }

        private void Awake()
        {
            if (rigRoot == null)
                rigRoot = transform;
            if (headCamera == null)
                headCamera = Camera.main;
        }

        private void Update()
        {
            if (rigRoot == null)
                return;

            InputDevices.GetDeviceAtXRNode(XRNode.LeftHand).TryGetFeatureValue(CommonUsages.primary2DAxis, out Vector2 leftStick);
            InputDevices.GetDeviceAtXRNode(XRNode.RightHand).TryGetFeatureValue(CommonUsages.primary2DAxis, out Vector2 rightStick);

            Vector2 move = ApplyDeadzone(leftStick, deadzone);
            Vector3 forward = headCamera != null ? headCamera.transform.forward : rigRoot.forward;
            Vector3 right = headCamera != null ? headCamera.transform.right : rigRoot.right;
            Vector3 planarForward = Vector3.ProjectOnPlane(forward, Vector3.up).normalized;
            Vector3 planarRight = Vector3.ProjectOnPlane(right, Vector3.up).normalized;

            Vector3 moveDelta = (planarForward * move.y + planarRight * move.x) * (moveSpeed * Time.deltaTime);

            float vertical = ApplyDeadzone(rightStick.y, deadzone);
            moveDelta += Vector3.up * (vertical * verticalSpeed * Time.deltaTime);

            rigRoot.position += moveDelta;

            HandleSnapTurn(rightStick.x);
        }

        private void HandleSnapTurn(float axisX)
        {
            if (Time.time < _nextSnapTime)
                return;

            if (Mathf.Abs(axisX) < snapDeadzone)
                return;

            float sign = Mathf.Sign(axisX);
            rigRoot.Rotate(Vector3.up, sign * snapDegrees, Space.World);
            _nextSnapTime = Time.time + snapCooldown;
        }

        private static Vector2 ApplyDeadzone(Vector2 value, float dz)
        {
            if (value.magnitude < dz)
                return Vector2.zero;
            return value;
        }

        private static float ApplyDeadzone(float value, float dz)
        {
            return Mathf.Abs(value) < dz ? 0.0f : value;
        }

    }
}
