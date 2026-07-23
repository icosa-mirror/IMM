using System.Collections;
using UnityEngine;
using UnityEngine.XR.Management;

namespace ImmPlayer
{
    /// <summary>
    /// Starts XR only for scenes that explicitly include this component.
    /// Keep project-wide XR auto-start disabled so desktop scenes do not enter
    /// OpenXR MultiPass just because the project has an XR loader configured.
    /// </summary>
    public sealed class XrSceneBootstrap : MonoBehaviour
    {
        private bool _startedSubsystems;
        private bool _initializedLoader;

        private IEnumerator Start()
        {
            XRManagerSettings manager = XRGeneralSettings.Instance?.Manager;
            if (manager == null)
            {
                Debug.LogError("[IMM_XR_SCENE_BOOTSTRAP] XR manager settings are missing.");
                yield break;
            }

            if (manager.activeLoader == null)
            {
                yield return manager.InitializeLoader();
                _initializedLoader = manager.activeLoader != null;
            }

            if (manager.activeLoader == null)
            {
                Debug.LogError("[IMM_XR_SCENE_BOOTSTRAP] XR loader initialization failed.");
                yield break;
            }

            manager.StartSubsystems();
            _startedSubsystems = true;
        }

        private void OnDisable()
        {
            XRManagerSettings manager = XRGeneralSettings.Instance?.Manager;
            if (manager == null)
                return;

            if (_startedSubsystems)
            {
                manager.StopSubsystems();
                _startedSubsystems = false;
            }

            if (_initializedLoader)
            {
                manager.DeinitializeLoader();
                _initializedLoader = false;
            }
        }
    }
}
