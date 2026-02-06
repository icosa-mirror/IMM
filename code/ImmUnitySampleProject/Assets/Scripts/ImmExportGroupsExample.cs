using ImmPlayer.Exporter;
using UnityEngine;

public class ImmExportGroupsExample : MonoBehaviour
{
    [SerializeField] private bool exportOnStart = false;
    [SerializeField] private string fileName = "grouped_example.imm";

    private void Start()
    {
        if (exportOnStart)
        {
            ExportGroupedStroke();
        }
    }

    public void ExportGroupedStroke()
    {
        var requirements = new ExportRequirements();
        using var seq = ExportSequence.Create(ExportSequenceType.Still, 30, Color.black, requirements);
        if (seq == null)
        {
            Debug.LogError("IMM export: failed to create sequence.");
            return;
        }

        var rootGroup = seq.CreateGroupLayer("RootGroup");
        if (rootGroup == null)
        {
            Debug.LogError("IMM export: failed to create root group.");
            return;
        }

        var childGroup = rootGroup.CreateGroupLayer("ChildGroup");
        if (childGroup == null)
        {
            Debug.LogError("IMM export: failed to create child group.");
            return;
        }

        var paintLayer = childGroup.CreatePaintLayer("StrokeLayer", true, 1.0f, transform);
        if (paintLayer == null)
        {
            Debug.LogError("IMM export: failed to create paint layer.");
            return;
        }

        using (var drawing = paintLayer.CreateDrawing())
        {
            if (drawing == null || !drawing.Init(1))
            {
                Debug.LogError("IMM export: failed to init drawing.");
                return;
            }

            var element = drawing.GetElement(0);
            if (element == null || !element.Init(4, BrushSectionType.Circle, VisibilityType.Always))
            {
                Debug.LogError("IMM export: failed to init element.");
                return;
            }

            for (uint i = 0; i < 4; i++)
            {
                float t = i / 3.0f;
                var point = new PaintPoint
                {
                    Position = new Vector3(t * 0.5f, 0.0f, 0.0f),
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

        string path = System.IO.Path.Combine(Application.persistentDataPath, fileName);
        bool ok = seq.ExportToFile(path);
        Debug.Log(ok ? $"IMM export: wrote {path}" : $"IMM export: failed to write {path}");
    }
}
