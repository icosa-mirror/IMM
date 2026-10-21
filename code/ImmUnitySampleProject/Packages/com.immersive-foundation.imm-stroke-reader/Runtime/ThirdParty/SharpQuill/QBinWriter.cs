using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SharpQuill
{
  /// <summary>
  /// Binary writer for qbin files.
  /// </summary>
  public class QBinWriter : BinaryWriter
  {
    public QBinWriter(Stream stream)
      : base(stream)
    {
    }
    
    internal static void ValidatePictureData(PictureData value)
    {
      if (value == null || value.Width <= 0 || value.Height <= 0 || value.Pixels == null)
        throw new InvalidDataException("Picture data requires positive dimensions and decoded pixels.");
      int channels = value.HasAlpha ? 4 : 3;
      long pixels = (long)value.Width * value.Height;
      if (pixels > int.MaxValue / channels || pixels * channels != value.Pixels.Length)
        throw new InvalidDataException("Picture pixel bytes do not match RGB/RGBA dimensions.");
    }

    public void Write(PictureData value)
    {
      ValidatePictureData(value);
      // Observed Quill picture header prefix, RGB8=6 or RGBA8=7,
      // dimensions and trailing value 1, followed by unmodified pixel bytes.
      Write(0x01000000u);
      Write((byte)2);
      Write((byte)(value.HasAlpha ? 7 : 6));
      Write((ushort)0);
      Write((uint)value.Width);
      Write((uint)value.Height);
      Write(1u);
      Write(value.Pixels);
    }

    public void Write(DrawingData value)
    {
      Write(value.Strokes.Count);
      foreach (Stroke stroke in value.Strokes)
        Write(stroke);
    }

    public void Write(Stroke stroke)
    {
      Write(stroke.Id);
      Write(stroke.u2);
      Write(stroke.BoundingBox);
      Write((UInt16)stroke.BrushType);
      Write(stroke.DisableRotationalOpacity);
      Write(stroke.u3);
      Write(stroke.Vertices.Count);
      foreach (Vertex vertex in stroke.Vertices)
        Write(vertex);
    }

    public void Write(BoundingBox bbox)
    {
      Write(bbox.MinX);
      Write(bbox.MaxX);
      Write(bbox.MinY);
      Write(bbox.MaxY);
      Write(bbox.MinZ);
      Write(bbox.MaxZ);
    }

    public void Write(Vertex vertex)
    {
      Write(vertex.Position);
      Write(vertex.Normal);
      Write(vertex.Tangent);
      Write(vertex.Color);
      Write(vertex.Opacity);
      Write(vertex.Width);
    }

    public void Write(Vector3 v)
    {
      Write(v.X);
      Write(v.Y);
      Write(v.Z);
    }

    public void Write(Color c)
    {
      Write(c.R);
      Write(c.G);
      Write(c.B);
    }
  }
}
