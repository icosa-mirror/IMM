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

    internal struct Pcm16Payload
    {
      internal int Channels, Rate, Offset, ByteCount;
    }

    internal static Pcm16Payload InspectSoundData(SoundData value)
    {
      byte[] bytes = value?.AudioBytes;
      if (bytes == null || bytes.Length < 12)
        throw new InvalidDataException("Sound requires a complete PCM16 WAV file.");
      var payload = new Pcm16Payload();
      bool formatFound = false, dataFound = false;
      using (var stream = new MemoryStream(bytes, false))
      using (var reader = new BinaryReader(stream))
      {
        if (reader.ReadUInt32() != 0x46464952u || (ulong)reader.ReadUInt32() + 8 != (ulong)bytes.Length || reader.ReadUInt32() != 0x45564157u)
          throw new InvalidDataException("Sound WAV RIFF framing is invalid.");
        while (stream.Position < stream.Length)
        {
          if (stream.Length - stream.Position < 8) throw new InvalidDataException("Truncated WAV chunk header.");
          uint id = reader.ReadUInt32(), size = reader.ReadUInt32();
          long start = stream.Position, next = start + size + (size & 1);
          if (next > stream.Length) throw new InvalidDataException("Truncated WAV chunk.");
          if (id == 0x20746d66u) // fmt
          {
            if (formatFound || size < 16) throw new InvalidDataException("Missing or repeated WAV format.");
            int codec = reader.ReadUInt16(); payload.Channels = reader.ReadUInt16();
            uint rate = reader.ReadUInt32(), byteRate = reader.ReadUInt32();
            int alignment = reader.ReadUInt16(), bits = reader.ReadUInt16();
            if (codec != 1 || bits != 16 || (payload.Channels != 1 && payload.Channels != 2) || rate < 8000 || rate > 192000)
              throw new NotSupportedException("Quill sound writing supports mono/stereo PCM16 WAV at 8000-192000 Hz.");
            payload.Rate = (int)rate;
            if (alignment != payload.Channels * 2 || byteRate != rate * alignment)
              throw new InvalidDataException("WAV frame alignment or byte rate is invalid.");
            formatFound = true;
          }
          else if (id == 0x61746164u) // data
          {
            if (dataFound || size == 0) throw new InvalidDataException("Missing or repeated WAV sample data.");
            payload.Offset = checked((int)start); payload.ByteCount = checked((int)size); dataFound = true;
          }
          stream.Position = next;
        }
      }
      if (!formatFound || !dataFound || payload.ByteCount % (payload.Channels * 2) != 0)
        throw new InvalidDataException("WAV requires a format and complete nonempty PCM16 frames.");
      return payload;
    }

    public void Write(SoundData value)
    {
      Pcm16Payload pcm = InspectSoundData(value);
      // Observed Quill sound header: reserved zero bytes, format 0, channels,
      // 16-bit samples, rate/frame count and a reserved zero word.
      Write(0u); Write((byte)0); Write((byte)0); Write((byte)pcm.Channels); Write((byte)16);
      Write((uint)pcm.Rate); Write((uint)(pcm.ByteCount / (pcm.Channels * 2))); Write(0u);
      Write(value.AudioBytes, pcm.Offset, pcm.ByteCount);
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
