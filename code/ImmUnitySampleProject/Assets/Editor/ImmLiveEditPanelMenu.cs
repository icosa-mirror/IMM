using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using System.IO;

namespace ImmPlayer.Editor
{
    internal static class ImmLiveEditPanelMenu
    {
        [MenuItem("IMM/Live Edit Lab/Open sample and play")]
        private static void Open()
        {
            if (EditorApplication.isPlayingOrWillChangePlaymode) return;
            if (!EditorSceneManager.SaveCurrentModifiedScenesIfUserWantsTo()) return;
            EditorSceneManager.OpenScene("Assets/Scenes/SampleScene.unity");
            SessionState.SetBool("IMM.LiveEditPanel", true);
            EditorApplication.isPlaying = true;
            EditorApplication.ExecuteMenuItem("Window/General/Game");
            foreach (var window in Resources.FindObjectsOfTypeAll<EditorWindow>())
                if (window.GetType().Name == "GameView") { window.maximized = true; window.Focus(); break; }
        }

        [MenuItem("IMM/Live Edit Lab/Restart Editor with Direct3D12 (Windows)")]
        private static void RestartD3D12()
        {
            if (!EditorSceneManager.SaveCurrentModifiedScenesIfUserWantsTo()) return;
            EditorApplication.OpenProject(Directory.GetParent(Application.dataPath).FullName, "-force-d3d12");
        }

        [MenuItem("IMM/Live Edit Lab/Restart Editor with Direct3D12 (Windows)", true)]
        private static bool CanRestart() => Application.platform == RuntimePlatform.WindowsEditor && !EditorApplication.isPlayingOrWillChangePlaymode;

        [MenuItem("IMM/Live Edit Lab/Disable automatic panel")]
        private static void Disable() => SessionState.SetBool("IMM.LiveEditPanel", false);
    }
}
