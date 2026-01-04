#if UNITY_EDITOR
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using ImmPlayer;

[CustomEditor(typeof(ImmFeatureExamples))]
public class ImmFeatureExamplesEditor : Editor
{
    private static readonly HashSet<string> SkipFields = new HashSet<string>
    {
        "documentPath",
        "loadOnStart",
        "autoPlay",
        "documentTransform",
        "documentState",
        "documentInfoFlags",
        "chapterCount",
        "layerCount",
        "spawnAreaCount",
        "documentBounds",
        "selectedLayerId",
        "selectedLayerName",
        "selectedLayerType",
        "selectedLayerParentId",
        "selectedLayerLoaded",
        "selectedLayerVisible",
        "selectedLayerOpacity",
        "selectedLayerHasBounds",
        "selectedLayerBounds",
        "selectedLayerVisibilityOverrideEnabled",
        "selectedLayerVisibilityOverrideValue",
        "layerVisible",
        "layerOpacity",
        "layerPosition",
        "layerEuler",
        "layerScale",
        "selectedLayerIndex",
        "layerList",
        "layers",
        "spawnAreas",
        "spawnAreaIndex"
    };

    private static readonly HashSet<string> ReadOnlyFields = new HashSet<string>
    {
        "documentState",
        "documentInfoFlags",
        "chapterCount",
        "layerCount",
        "spawnAreaCount",
        "documentBounds",
        "selectedLayerId",
        "selectedLayerName",
        "selectedLayerType",
        "selectedLayerParentId",
        "selectedLayerLoaded",
        "selectedLayerVisible",
        "selectedLayerOpacity",
        "selectedLayerHasBounds",
        "selectedLayerBounds",
        "selectedLayerVisibilityOverrideEnabled",
        "selectedLayerVisibilityOverrideValue",
    };

    private bool _showDocumentStatus = false;
    private bool _showSelectedLayerStatus = false;
    private bool _showActions = false;

    public override void OnInspectorGUI()
    {
        serializedObject.Update();

        DrawScriptField();
        DrawDocumentFields();
        DrawDocumentStatusFoldout();

        ImmFeatureExamples script = (ImmFeatureExamples)target;
        DrawActionsFoldout(script);
        DrawLayerDropdown(script);
        DrawLayerEditsFields();
        DrawClearLayerOverridesButton(script);

        DrawSelectedLayerStatusFoldout();
        DrawPropertiesExcludingReadOnly();

        serializedObject.ApplyModifiedProperties();
    }

    private void DrawScriptField()
    {
        SerializedProperty scriptProp = serializedObject.FindProperty("m_Script");
        if (scriptProp == null)
            return;

        using (new EditorGUI.DisabledScope(true))
        {
            EditorGUILayout.PropertyField(scriptProp);
        }
    }

    private void DrawDocumentFields()
    {
        EditorGUILayout.PropertyField(serializedObject.FindProperty("documentPath"));
        EditorGUILayout.PropertyField(serializedObject.FindProperty("loadOnStart"));
        EditorGUILayout.PropertyField(serializedObject.FindProperty("autoPlay"));
        EditorGUILayout.PropertyField(serializedObject.FindProperty("documentTransform"));
    }

    private void DrawDocumentStatusFoldout()
    {
        EditorGUILayout.Space(6);
        _showDocumentStatus = EditorGUILayout.Foldout(_showDocumentStatus, "Document Status (Read Only)", true);
        if (!_showDocumentStatus)
            return;

        using (new EditorGUI.DisabledScope(true))
        {
            EditorGUILayout.PropertyField(serializedObject.FindProperty("documentState"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("documentInfoFlags"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("chapterCount"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("layerCount"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("spawnAreaCount"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("documentBounds"));
        }
    }

    private void DrawLayerEditsFields()
    {
        EditorGUILayout.Space(6);
        EditorGUILayout.LabelField("Layer Edits", EditorStyles.boldLabel);
        EditorGUILayout.PropertyField(serializedObject.FindProperty("layerVisible"));
        EditorGUILayout.PropertyField(serializedObject.FindProperty("layerOpacity"));
        EditorGUILayout.PropertyField(serializedObject.FindProperty("layerPosition"));
        EditorGUILayout.PropertyField(serializedObject.FindProperty("layerEuler"));
        EditorGUILayout.PropertyField(serializedObject.FindProperty("layerScale"));
    }

    private void DrawSelectedLayerStatusFoldout()
    {
        EditorGUILayout.Space(6);
        _showSelectedLayerStatus = EditorGUILayout.Foldout(_showSelectedLayerStatus, "Selected Layer Status (Read Only)", true);
        if (!_showSelectedLayerStatus)
            return;

        using (new EditorGUI.DisabledScope(true))
        {
            EditorGUILayout.PropertyField(serializedObject.FindProperty("selectedLayerId"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("selectedLayerName"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("selectedLayerType"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("selectedLayerParentId"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("selectedLayerLoaded"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("selectedLayerVisible"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("selectedLayerOpacity"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("selectedLayerHasBounds"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("selectedLayerBounds"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("selectedLayerVisibilityOverrideEnabled"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("selectedLayerVisibilityOverrideValue"));
        }
    }

    private void DrawActionsFoldout(ImmFeatureExamples script)
    {
        EditorGUILayout.Space(6);
        _showActions = EditorGUILayout.Foldout(_showActions, "Actions", true);
        if (!_showActions)
            return;

        using (new EditorGUI.DisabledScope(!Application.isPlaying))
        {
            if (GUILayout.Button("Load Document"))
            {
                script.LoadDocument();
                script.RefreshStatus();
            }
            if (GUILayout.Button("Unload Document"))
            {
                script.UnloadDocument();
                script.RefreshStatus();
            }
            EditorGUILayout.Space(4);
            bool isPlaying = script.documentState.Playback == ImmDocument.PlaybackState.Playing;
            if (GUILayout.Button(isPlaying ? "Pause" : "Play"))
            {
                if (isPlaying) script.Pause();
                else script.Play();
                script.RefreshStatus();
                Repaint();
            }
            if (GUILayout.Button("Restart"))
            {
                script.Restart();
            }
            if (GUILayout.Button("Next Chapter"))
            {
                script.SkipForward();
            }
            if (GUILayout.Button("Previous Chapter"))
            {
                script.SkipBack();
            }
        }
    }

    private void DrawClearLayerOverridesButton(ImmFeatureExamples script)
    {
        EditorGUILayout.Space(6);
        using (new EditorGUI.DisabledScope(!Application.isPlaying))
        {
            if (GUILayout.Button("Clear Layer Overrides"))
            {
                script.ClearLayerOverrides();
            }
        }
    }
    private void DrawPropertiesExcludingReadOnly()
    {
        SerializedProperty prop = serializedObject.GetIterator();
        bool enterChildren = true;
        while (prop.NextVisible(enterChildren))
        {
            enterChildren = false;

            if (prop.name == "m_Script")
            {
                continue;
            }

            if (SkipFields.Contains(prop.name))
            {
                continue;
            }

            bool isReadOnly = ReadOnlyFields.Contains(prop.name);
            using (new EditorGUI.DisabledScope(isReadOnly))
            {
                EditorGUILayout.PropertyField(prop, true);
            }
        }
    }

    private void DrawLayerDropdown(ImmFeatureExamples script)
    {
        EditorGUILayout.Space(8);
        EditorGUILayout.LabelField("Layer Target", EditorStyles.boldLabel);

        if (script.layerList == null || script.layerList.Length == 0)
        {
            EditorGUILayout.HelpBox("Layer list is empty. Enter Play Mode and use IMM/Refresh Layer List.", MessageType.Info);
            return;
        }

        List<string> options = new List<string>(script.layerList.Length);
        for (int i = 0; i < script.layerList.Length; i++)
        {
            var entry = script.layerList[i];
            string name = string.IsNullOrEmpty(entry.Name) ? "(unnamed)" : entry.Name;
            int depth = GetLayerDepth(entry.FullName);
            string indent = new string(' ', depth * 2);
            options.Add($"{entry.Index}: {indent}- {name} [{entry.Type}]");
        }

        SerializedProperty selectedIndexProp = serializedObject.FindProperty("selectedLayerIndex");
        int currentIndex = selectedIndexProp != null ? selectedIndexProp.intValue : -1;
        int popupIndex = Mathf.Clamp(currentIndex, 0, options.Count - 1);

        int newPopupIndex = EditorGUILayout.Popup("Selected Layer", popupIndex, options.ToArray());
        if (selectedIndexProp != null)
        {
            bool changed = newPopupIndex != currentIndex;
            selectedIndexProp.intValue = newPopupIndex;
            if (changed)
            {
                serializedObject.ApplyModifiedProperties();
                if (Application.isPlaying)
                {
                    script.RefreshSelectedLayerStatus();
                }
                EditorUtility.SetDirty(script);
                Repaint();
            }
        }
    }

    private static int GetLayerDepth(string fullName)
    {
        if (string.IsNullOrEmpty(fullName))
            return 0;

        int depth = 0;
        for (int i = 0; i < fullName.Length; i++)
        {
            char c = fullName[i];
            if (c == '/' || c == '\\')
                depth++;
        }

        return depth;
    }
}
#endif
