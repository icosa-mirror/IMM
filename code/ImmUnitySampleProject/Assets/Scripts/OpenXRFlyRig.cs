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

        [Header("Testing")]
        [SerializeField] private ImmFeatureExamples featureExamples;
        [SerializeField] private bool enableRandomLoadButton = true;
        [SerializeField] private bool enableNavigationButtons = true;

        private float _nextSnapTime;
        private bool _wasRandomLoadPressed;
        private bool _wasNextChapterPressed;
        private bool _wasPrevSpawnPressed;
        private bool _wasNextSpawnPressed;

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
            if (featureExamples == null)
                featureExamples = FindObjectOfType<ImmFeatureExamples>();
        }

        private void Update()
        {
            if (rigRoot == null)
                return;

            InputDevice leftHand = InputDevices.GetDeviceAtXRNode(XRNode.LeftHand);
            leftHand.TryGetFeatureValue(CommonUsages.primary2DAxis, out Vector2 leftStick);
            InputDevice rightHand = InputDevices.GetDeviceAtXRNode(XRNode.RightHand);
            rightHand.TryGetFeatureValue(CommonUsages.primary2DAxis, out Vector2 rightStick);

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
            HandleRandomLoadShortcut(rightHand);
            HandleNavigationShortcuts(leftHand, rightHand);
        }

        private void HandleRandomLoadShortcut(InputDevice rightHand)
        {
            if (!enableRandomLoadButton)
                return;

            rightHand.TryGetFeatureValue(CommonUsages.secondaryButton, out bool isPressed);
            if (isPressed && !_wasRandomLoadPressed)
            {
                if (featureExamples != null && featureExamples.PickRandomStreamingAssetsFile())
                {
                    featureExamples.LoadDocument();
                }
            }

            _wasRandomLoadPressed = isPressed;
        }

        private void HandleNavigationShortcuts(InputDevice leftHand, InputDevice rightHand)
        {
            if (!enableNavigationButtons || featureExamples == null)
                return;

            rightHand.TryGetFeatureValue(CommonUsages.triggerButton, out bool nextChapterPressed);
            if (nextChapterPressed && !_wasNextChapterPressed)
            {
                featureExamples.SkipForward();
            }
            _wasNextChapterPressed = nextChapterPressed;

            leftHand.TryGetFeatureValue(CommonUsages.gripButton, out bool prevSpawnPressed);
            if (prevSpawnPressed && !_wasPrevSpawnPressed)
            {
                featureExamples.PreviousSpawnArea();
            }
            _wasPrevSpawnPressed = prevSpawnPressed;

            rightHand.TryGetFeatureValue(CommonUsages.gripButton, out bool nextSpawnPressed);
            if (nextSpawnPressed && !_wasNextSpawnPressed)
            {
                featureExamples.NextSpawnArea();
            }
            _wasNextSpawnPressed = nextSpawnPressed;
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
