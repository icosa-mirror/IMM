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
        "loadSource",
        "directoryPath",
        "selectedFileName",
        "loadOnStart",
        "autoPlay",
        "documentTransform",
        "currentFilePath",
        "documentState",
        "documentInfoFlags",
        "chapterCount",
        "currentChapter",
        "layerCount",
        "spawnAreaCount",
        "activeSpawnAreaId",
        "currentSpawnAreaIndex",
        "targetSpawnAreaIndex",
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
        "targetChapter",
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
        "currentChapter",
        "layerCount",
        "spawnAreaCount",
        "activeSpawnAreaId",
        "currentSpawnAreaIndex",
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
        EditorGUILayout.PropertyField(serializedObject.FindProperty("loadSource"));

        SerializedProperty loadSourceProp = serializedObject.FindProperty("loadSource");
        bool isFileSystemMode = loadSourceProp.enumValueIndex == (int)ImmFeatureExamples.LoadSource.FileSystem;

        if (isFileSystemMode)
        {
            EditorGUILayout.PropertyField(serializedObject.FindProperty("directoryPath"));
        }

        DrawImmFileDropdown(isFileSystemMode);

        EditorGUILayout.PropertyField(serializedObject.FindProperty("loadOnStart"));
        EditorGUILayout.PropertyField(serializedObject.FindProperty("autoPlay"));
        EditorGUILayout.PropertyField(serializedObject.FindProperty("documentTransform"));
    }

    private void DrawImmFileDropdown(bool isFileSystemMode)
    {
        SerializedProperty selectedFileNameProp = serializedObject.FindProperty("selectedFileName");
        if (selectedFileNameProp == null)
            return;

        string dirPath;
        if (isFileSystemMode)
        {
            SerializedProperty directoryPathProp = serializedObject.FindProperty("directoryPath");
            if (directoryPathProp == null)
                return;
            dirPath = directoryPathProp.stringValue;

            if (string.IsNullOrEmpty(dirPath))
            {
                EditorGUILayout.HelpBox("Please set a directory path.", MessageType.Info);
                return;
            }
        }
        else
        {
            // StreamingAssets mode - use the StreamingAssets folder
            dirPath = Application.streamingAssetsPath;
        }

        if (!System.IO.Directory.Exists(dirPath))
        {
            if (isFileSystemMode)
            {
                EditorGUILayout.HelpBox($"Directory not found: {dirPath}", MessageType.Warning);
            }
            else
            {
                EditorGUILayout.HelpBox("StreamingAssets folder not found. Create Assets/StreamingAssets/ and add .imm files.", MessageType.Warning);
            }
            return;
        }

        string[] immFiles = System.IO.Directory.GetFiles(dirPath, "*.imm");

        if (immFiles.Length == 0)
        {
            string location = isFileSystemMode ? "directory" : "StreamingAssets folder";
            EditorGUILayout.HelpBox($"No .imm files found in {location}.", MessageType.Info);
            return;
        }

        // Extract just filenames
        string[] fileNames = new string[immFiles.Length];
        for (int i = 0; i < immFiles.Length; i++)
        {
            fileNames[i] = System.IO.Path.GetFileName(immFiles[i]);
        }

        // Find current selection index
        string currentFileName = selectedFileNameProp.stringValue;
        int currentIndex = System.Array.IndexOf(fileNames, currentFileName);
        if (currentIndex < 0) currentIndex = 0;

        // Draw dropdown
        int newIndex = EditorGUILayout.Popup("IMM File", currentIndex, fileNames);

        if (newIndex != currentIndex || string.IsNullOrEmpty(currentFileName))
        {
            selectedFileNameProp.stringValue = fileNames[newIndex];
            serializedObject.ApplyModifiedProperties();
        }
    }

    private void DrawDocumentStatusFoldout()
    {
        EditorGUILayout.Space(6);
        _showDocumentStatus = EditorGUILayout.Foldout(_showDocumentStatus, "Document Status (Read Only)", true);
        if (!_showDocumentStatus)
            return;

        using (new EditorGUI.DisabledScope(true))
        {
            EditorGUILayout.PropertyField(serializedObject.FindProperty("currentFilePath"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("documentState"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("documentInfoFlags"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("chapterCount"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("currentChapter"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("layerCount"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("spawnAreaCount"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("activeSpawnAreaId"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("currentSpawnAreaIndex"));
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
            if (GUILayout.Button("Pick Random + Load"))
            {
                Undo.RecordObject(target, "Pick Random and Load");
                script.PickRandomAndLoad();
                EditorUtility.SetDirty(target);
                serializedObject.Update();
                script.RefreshStatus();
                Repaint();
            }

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
            bool hasMultipleChapters = script.chapterCount > 1;
            using (new EditorGUI.DisabledScope(!hasMultipleChapters))
            {
                if (GUILayout.Button("Next Chapter"))
                {
                    script.SkipForward();
                }
                if (GUILayout.Button("Previous Chapter"))
                {
                    script.SkipBack();
                }

                SerializedProperty targetChapterProp = serializedObject.FindProperty("targetChapter");
                int maxChapter = Mathf.Max(0, script.chapterCount - 1);
                int oldTargetChapter = targetChapterProp.intValue;
                int newTargetChapter = EditorGUILayout.IntSlider("Target Chapter", oldTargetChapter, 0, maxChapter);
                if (newTargetChapter != oldTargetChapter)
                {
                    targetChapterProp.intValue = newTargetChapter;
                    serializedObject.ApplyModifiedProperties();
                    script.JumpToChapter();
                    script.RefreshStatus();
                    serializedObject.Update();
                    Repaint();
                }
            }

            EditorGUILayout.Space(4);
            bool hasMultipleSpawnAreas = script.spawnAreaCount > 1;
            using (new EditorGUI.DisabledScope(!hasMultipleSpawnAreas))
            {
                if (GUILayout.Button("Next Spawn Area"))
                {
                    script.NextSpawnArea();
                    script.RefreshStatus();
                }
                if (GUILayout.Button("Previous Spawn Area"))
                {
                    script.PreviousSpawnArea();
                    script.RefreshStatus();
                }

                SerializedProperty targetSpawnAreaIndexProp = serializedObject.FindProperty("targetSpawnAreaIndex");
                int maxSpawnAreaIndex = Mathf.Max(0, script.spawnAreaCount - 1);
                int oldTargetSpawnArea = targetSpawnAreaIndexProp.intValue;
                int newTargetSpawnArea = EditorGUILayout.IntSlider("Target Spawn Area", oldTargetSpawnArea, 0, maxSpawnAreaIndex);
                if (newTargetSpawnArea != oldTargetSpawnArea)
                {
                    targetSpawnAreaIndexProp.intValue = newTargetSpawnArea;
                    serializedObject.ApplyModifiedProperties();
                    script.JumpToSpawnArea();
                    script.RefreshStatus();
                    serializedObject.Update();
                    Repaint();
                }
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
