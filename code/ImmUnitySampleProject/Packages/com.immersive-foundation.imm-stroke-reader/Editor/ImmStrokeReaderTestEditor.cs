using UnityEngine;
using UnityEditor;
using ImmPlayer;
using System.IO;

/// <summary>
/// Editor window for testing ImmStrokeReader without entering Play mode.
/// </summary>
public class ImmStrokeReaderTestEditor : EditorWindow
{
    private string _logPath = "";
    private Vector2 _scrollPos;
    private string _output = "";

    [MenuItem("IMM/Stroke Reader Test")]
    public static void ShowWindow()
    {
        GetWindow<ImmStrokeReaderTestEditor>("Stroke Reader Test");
    }

    void OnGUI()
    {
        EditorGUILayout.LabelField("ImmStrokeReader Integration Test", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        if (GUILayout.Button("Run Test on logo_animation.imm"))
        {
            RunTest("Assets/ExampleImmFiles/logo_animation.imm");
        }

        if (GUILayout.Button("Run Test on fail-snail.imm"))
        {
            RunTest("Assets/ExampleImmFiles/fail-snail.imm");
        }

        if (GUILayout.Button("Run Test on sample1.imm"))
        {
            RunTest("Assets/ExampleImmFiles/sample1.imm");
        }

        EditorGUILayout.Space();

        if (!string.IsNullOrEmpty(_logPath) && GUILayout.Button("Open Log File"))
        {
            if (File.Exists(_logPath))
            {
                System.Diagnostics.Process.Start(_logPath);
            }
        }

        EditorGUILayout.Space();
        EditorGUILayout.LabelField("Output:", EditorStyles.boldLabel);

        _scrollPos = EditorGUILayout.BeginScrollView(_scrollPos, GUILayout.Height(400));
        EditorGUILayout.TextArea(_output, GUILayout.ExpandHeight(true));
        EditorGUILayout.EndScrollView();
    }

    void RunTest(string relativePath)
    {
        _output = "";
        Log($"=== ImmStrokeReader Test ===");
        Log($"Testing: {relativePath}");

        string fullPath = Path.GetFullPath(relativePath);
        Log($"Full path: {fullPath}");

        if (!File.Exists(fullPath))
        {
            Log($"ERROR: File not found!");
            return;
        }

        // Set up log path
        _logPath = Path.Combine(Application.temporaryCachePath, "stroke_reader_test.log");
        Log($"Log path: {_logPath}");

        // Initialize
        Log("\n--- Initializing ---");
        bool wasInitialized = ImmStrokeReader.StrokeReader_IsInitialized();
        Log($"Already initialized: {wasInitialized}");

        if (!wasInitialized)
        {
            int initResult = ImmStrokeReader.StrokeReader_Init(_logPath);
            Log($"Init result: {initResult}");
            if (initResult != 0)
            {
                Log("ERROR: Failed to initialize!");
                return;
            }
        }

        // Load
        Log("\n--- Loading ---");
        int docId = ImmStrokeReader.StrokeReader_LoadFromFile(fullPath);
        Log($"LoadFromFile result (docId): {docId}");

        if (docId < 0)
        {
            Log("ERROR: Failed to load file!");
            return;
        }

        // Query structure
        Log("\n--- Document Structure ---");
        int layerCount = ImmStrokeReader.StrokeReader_GetLayerCount(docId);
        Log($"Layer count: {layerCount}");

        int totalStrokes = 0;
        int totalPoints = 0;

        for (int l = 0; l < layerCount; l++)
        {
            if (ImmStrokeReader.StrokeReader_GetLayerInfo(docId, l, out StrokeLayerInfo layerInfo))
            {
                int drawingCount = ImmStrokeReader.StrokeReader_GetDrawingCount(docId, l);
                Log($"  Layer {l}: id={layerInfo.id}, type={layerInfo.type}, name='{layerInfo.name}', drawings={drawingCount}");

                for (int d = 0; d < drawingCount; d++)
                {
                    int strokeCount = ImmStrokeReader.StrokeReader_GetStrokeCount(docId, l, d);
                    totalStrokes += strokeCount;

                    if (strokeCount > 0)
                    {
                        Log($"    Drawing {d}: {strokeCount} strokes");
                    }

                    for (int s = 0; s < strokeCount; s++)
                    {
                        if (ImmStrokeReader.StrokeReader_GetStrokeInfo(docId, l, d, s, out StrokeInfo strokeInfo))
                        {
                            totalPoints += strokeInfo.numPoints;
                        }
                    }
                }
            }
        }

        Log($"\nTotal: {totalStrokes} strokes, {totalPoints} points");

        // Sample first stroke
        Log("\n--- First Stroke Sample ---");
        for (int l = 0; l < layerCount && totalStrokes > 0; l++)
        {
            int drawingCount = ImmStrokeReader.StrokeReader_GetDrawingCount(docId, l);
            for (int d = 0; d < drawingCount; d++)
            {
                int strokeCount = ImmStrokeReader.StrokeReader_GetStrokeCount(docId, l, d);
                for (int s = 0; s < strokeCount; s++)
                {
                    if (ImmStrokeReader.StrokeReader_GetStrokeInfo(docId, l, d, s, out StrokeInfo info) && info.numPoints > 0)
                    {
                        Log($"Stroke [{l},{d},{s}]: brush={info.brushType}, vis={info.visibilityMode}, pts={info.numPoints}");
                        Log($"  BBox: ({info.bboxMinX:F3}, {info.bboxMinY:F3}, {info.bboxMinZ:F3}) to ({info.bboxMaxX:F3}, {info.bboxMaxY:F3}, {info.bboxMaxZ:F3})");

                        StrokePoint[] points = new StrokePoint[info.numPoints];
                        if (ImmStrokeReader.StrokeReader_GetStrokePoints(docId, l, d, s, points, info.numPoints))
                        {
                            int numToShow = Mathf.Min(3, points.Length);
                            for (int p = 0; p < numToShow; p++)
                            {
                                var pt = points[p];
                                Log($"  Point {p}: pos=({pt.px:F3}, {pt.py:F3}, {pt.pz:F3}), col=({pt.r:F2}, {pt.g:F2}, {pt.b:F2}, {pt.alpha:F2}), w={pt.width:F1}");
                            }
                            if (points.Length > numToShow)
                            {
                                Log($"  ... and {points.Length - numToShow} more points");
                            }
                        }
                        goto done_sampling;
                    }
                }
            }
        }
        done_sampling:

        // Cleanup
        Log("\n--- Cleanup ---");
        ImmStrokeReader.StrokeReader_Unload(docId);
        Log($"Document unloaded");

        int remaining = ImmStrokeReader.StrokeReader_GetDocumentCount();
        Log($"Remaining documents: {remaining}");

        Log("\n=== Test Complete ===");
        Repaint();
    }

    void Log(string message)
    {
        _output += message + "\n";
        Debug.Log("[StrokeReaderTest] " + message);
    }
}
