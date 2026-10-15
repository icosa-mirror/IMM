using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SharpQuill
{
  /// <summary>
  /// Binary reader for qbin files.
  /// </summary>
  public class QBinReader : BinaryReader
  {
    public QBinReader(Stream stream)
			: base(stream)
		{
    }

    /// <summary>
    /// Reads the paint stroke data for one drawing.
    /// </summary>
    public DrawingData ReadDrawingData()
    {
      DrawingData pl = new DrawingData();

      int count = ReadInt32();
      pl.Strokes = new List<Stroke>();
      for (int i = 0; i < count; i++)
        pl.Strokes.Add(ReadStroke());
      
      return pl;
    }

    public PictureData ReadPictureData()
    {
      PictureData data = new PictureData();

      // Unknown header.
      ReadBytes(5);

      byte format = ReadByte();
      data.HasAlpha = format == 0x07;

      // Unknown.
      ReadBytes(2);

      uint width = ReadUInt32();
      uint height = ReadUInt32();

      // Unknown.
      ReadUInt32();

      if (width == 0 || height == 0 || width > int.MaxValue || height > int.MaxValue)
        return data;

      int channels = data.HasAlpha ? 4 : 3;
      long byteCount = (long)width * height * channels;
      if (byteCount > int.MaxValue)
        return data;

      data.Width = (int)width;
      data.Height = (int)height;
      data.Pixels = ReadBytes((int)byteCount);

      return data;
    }

    /// <summary>
    /// Reads Quill's embedded 16-bit PCM sound and wraps it in a WAV file.
    /// The qbin stores a 20-byte header followed by interleaved sample frames,
    /// not the original imported MP3/OGG/WAV file.
    /// </summary>
    public SoundData ReadSoundData()
    {
      ReadUInt32(); // Reserved header fields.
      ReadByte();
      byte format = ReadByte();
      byte channels = ReadByte();
      byte bitsPerSample = ReadByte();
      uint sampleRate = ReadUInt32();
      uint frameCount = ReadUInt32();
      ReadUInt32();

      if (format != 0 || bitsPerSample != 16)
        throw new NotSupportedException($"Unsupported Quill sound format {format}, {bitsPerSample} bits per sample.");
      if (channels == 0 || sampleRate == 0)
        throw new InvalidDataException("Quill sound must have channels and a sample rate.");

      ushort blockAlign = (ushort)(channels * (bitsPerSample / 8));
      ulong byteRate = (ulong)sampleRate * blockAlign;
      ulong byteCount = (ulong)frameCount * blockAlign;
      if (byteRate > uint.MaxValue || byteCount > int.MaxValue - 44)
        throw new InvalidDataException("Quill sound is too large for a WAV byte array.");
      if (BaseStream.CanSeek && byteCount > (ulong)(BaseStream.Length - BaseStream.Position))
        throw new EndOfStreamException("Quill sound samples are truncated.");

      byte[] wav = new byte[44 + (int)byteCount];
      using (var stream = new MemoryStream(wav))
      using (var writer = new BinaryWriter(stream))
      {
        writer.Write(0x46464952u); // RIFF
        writer.Write((uint)(wav.Length - 8));
        writer.Write(0x45564157u); // WAVE
        writer.Write(0x20746d66u); // fmt
        writer.Write(16u);
        writer.Write((ushort)1); // Integer PCM.
        writer.Write((ushort)channels);
        writer.Write(sampleRate);
        writer.Write((uint)byteRate);
        writer.Write(blockAlign);
        writer.Write((ushort)bitsPerSample);
        writer.Write(0x61746164u); // data
        writer.Write((uint)byteCount);
      }

      int copied = 0;
      while (copied < (int)byteCount)
      {
        int count = Read(wav, 44 + copied, (int)byteCount - copied);
        if (count == 0)
          throw new EndOfStreamException("Quill sound samples are truncated.");
        copied += count;
      }
      return new SoundData { AudioBytes = wav, Length = wav.Length };
    }

    private Stroke ReadStroke()
    {
      Stroke stroke = new Stroke();

      stroke.Id = ReadUInt32();
      stroke.u2 = ReadInt32();
      stroke.BoundingBox = ReadBoundingBox();
      short brushType = ReadInt16();
      stroke.BrushType = (Enum.IsDefined(typeof(BrushType), brushType)) ? (BrushType)brushType : BrushType.Cylinder;
      stroke.DisableRotationalOpacity = ReadBoolean();
      stroke.u3 = ReadByte();
      int count = ReadInt32();
      for (int i = 0; i < count; i++)
        stroke.Vertices.Add(ReadVertex());
      
      return stroke;
    }

    private BoundingBox ReadBoundingBox()
    {
      BoundingBox bbox = new BoundingBox();

      bbox.MinX = ReadSingle();
      bbox.MaxX = ReadSingle();
      bbox.MinY = ReadSingle();
      bbox.MaxY = ReadSingle();
      bbox.MinZ = ReadSingle();
      bbox.MaxZ = ReadSingle();

      return bbox;
    }

    private Vertex ReadVertex()
    {
      Vertex v = new Vertex();

      v.Position = ReadVector3();
      v.Normal = ReadVector3();
      v.Tangent = ReadVector3();
      v.Color = ReadColor();
      v.Opacity = ReadSingle();
      v.Width = ReadSingle();
      
      return v;
    }

    private Color ReadColor()
    {
      Color c = new Color();

      c.R = ReadSingle();
      c.G = ReadSingle();
      c.B = ReadSingle();

      return c;
    }

    private Vector3 ReadVector3()
    {
      Vector3 v = new Vector3();

      v.X = ReadSingle();
      v.Y = ReadSingle();
      v.Z = ReadSingle();

      return v;
    }
  }
}
