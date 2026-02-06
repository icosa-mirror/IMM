using System.IO;
using ImmPlayer.Exporter;
using UnityEngine;

public class ImmExportExample : MonoBehaviour
{
    [SerializeField] private bool exportOnStart = false;
    [SerializeField] private string fileName = "exported_example.imm";

    private void Start()
    {
        if (exportOnStart)
        {
            ExportSimpleStroke();
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
}
