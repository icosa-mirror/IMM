using System.IO;
using ImmPlayer.Exporter;
using UnityEngine;

public class ImmExportExample : MonoBehaviour
{
    [SerializeField] private bool exportOnStart = false;
    [SerializeField] private bool exportSpawnAreaOnStart = false;
    [SerializeField] private string fileName = "exported_example.imm";
    [SerializeField] private string spawnAreaFileName = "exported_spawn_area.imm";

    private void Start()
    {
        if (exportOnStart)
        {
            ExportSimpleStroke();
        }

        if (exportSpawnAreaOnStart)
        {
            ExportSpawnAreaExample();
        }
    }

    public void ExportSimpleStroke()
    {
        var requirements = new ExportRequirements
        {
            MaxMemory = 0,
            MaxRenderCalls = 0,
            MaxTriangles = 0,
            MaxSoundChannels = 0
        };

        using (ExportSequence seq = ExportSequence.Create(
                   ExportSequenceType.Still,
                   30,
                   Color.black,
                   requirements))
        {
            if (seq == null)
            {
                Debug.LogError("IMM export: failed to create sequence.");
                return;
            }

            ExportPaintLayer paintLayer = seq.CreatePaintLayer("StrokeLayer", true, 1.0f, transform);
            if (paintLayer == null)
            {
                Debug.LogError("IMM export: failed to create paint layer.");
                return;
            }

            using (ExportDrawing drawing = paintLayer.CreateDrawing())
            {
                if (drawing == null || !drawing.Init(1))
                {
                    Debug.LogError("IMM export: failed to init drawing.");
                    return;
                }

                ExportElement element = drawing.GetElement(0);
                if (element == null || !element.Init(4, BrushSectionType.Circle, VisibilityType.Always))
                {
                    Debug.LogError("IMM export: failed to init element.");
                    return;
                }

                for (uint i = 0; i < 4; i++)
                {
                    float t = i / 3.0f;
                    Vector3 pos = new Vector3(t * 0.5f, 0.0f, 0.0f);
                    var point = new PaintPoint
                    {
                        Position = pos,
                        Normal = Vector3.up,
                        Direction = Vector3.forward,
                        Color = Color.white,
                        Alpha = 1.0f,
                        Width = 0.02f,
                        Length = t,
                        Time = t
                    };

                    if (!element.SetPoint(i, point))
                    {
                        Debug.LogError($"IMM export: failed to set point {i}.");
                        return;
                    }
                }

                element.ComputeBounds();
                drawing.ComputeBounds();
                drawing.AddFrame();
            }

            string path = Path.Combine(Application.persistentDataPath, fileName);
            bool ok = seq.ExportToFile(path);
            Debug.Log(ok
                ? $"IMM export: wrote {path}"
                : $"IMM export: failed to write {path}");
        }
    }

    /// <summary>
    /// Export a document whose viewpoint comes from an authored spawn-area layer, transferring
    /// the stroke's points in one batch (SetPoints) instead of point by point, and flagging the
    /// spawn-area layer as the sequence's initial viewpoint.
    /// </summary>
    public void ExportSpawnAreaExample()
    {
        var requirements = new ExportRequirements();

        using (ExportSequence seq = ExportSequence.Create(
                   ExportSequenceType.Animated,
                   30,
                   Color.black,
                   requirements))
        {
            if (seq == null)
            {
                Debug.LogError("IMM export: failed to create sequence.");
                return;
            }

            ExportPaintLayer paintLayer = seq.CreatePaintLayer("BatchStrokeLayer", true, 1.0f, transform);
            if (paintLayer == null)
            {
                Debug.LogError("IMM export: failed to create paint layer.");
                return;
            }

            const int pointCount = 8;
            var points = new PaintPoint[pointCount];
            for (int i = 0; i < pointCount; i++)
            {
                float t = i / (float)(pointCount - 1);
                points[i] = new PaintPoint
                {
                    Position = new Vector3(t * 0.5f, Mathf.Sin(t * Mathf.PI) * 0.1f, 0.0f),
                    Normal = Vector3.up,
                    Direction = Vector3.forward,
                    Color = Color.white,
                    Alpha = 1.0f,
                    Width = 0.02f,
                    Length = t,
                    Time = t
                };
            }

            using (ExportDrawing drawing = paintLayer.CreateDrawing())
            {
                if (drawing == null || !drawing.Init(1))
                {
                    Debug.LogError("IMM export: failed to init drawing.");
                    return;
                }

                ExportElement element = drawing.GetElement(0);
                if (element == null ||
                    !element.Init((uint)pointCount, BrushSectionType.Circle, VisibilityType.Always))
                {
                    Debug.LogError("IMM export: failed to init element.");
                    return;
                }

                if (!element.SetPoints(points))
                {
                    Debug.LogError("IMM export: failed to set the point batch.");
                    return;
                }

                element.ComputeBounds();
                drawing.ComputeBounds();
                drawing.AddFrame();
            }

            ExportSpawnAreaLayer spawnArea = seq.CreateSpawnAreaLayer("Viewpoint", transform, floorLevel: true);
            if (spawnArea == null)
            {
                Debug.LogError("IMM export: failed to create spawn-area layer.");
                return;
            }

            if (!spawnArea.SetVolume(
                    ExportSpawnAreaVolume.Box,
                    offset: new Vector3(0.0f, 0.5f, 0.0f),
                    extent: new Vector3(2.0f, 1.0f, 2.0f),
                    allowTranslationX: true,
                    allowTranslationZ: true))
            {
                Debug.LogError("IMM export: failed to set the spawn-area volume.");
                return;
            }

            if (!seq.SetInitialSpawnArea(spawnArea))
            {
                Debug.LogError("IMM export: failed to flag the spawn-area layer as the initial viewpoint.");
                return;
            }

            string path = Path.Combine(Application.persistentDataPath, spawnAreaFileName);
            bool ok = seq.ExportToFile(path);
            Debug.Log(ok
                ? $"IMM export: wrote {path} with an authored spawn area"
                : $"IMM export: failed to write {path}");
        }
    }
}
