using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SharpQuill
{
  /// <summary>
  /// The binary data of embedded sound.
  /// </summary>
  public class SoundData
  {
    /// <summary>
    /// The raw audio file bytes (typically MP3, OGG, or WAV format).
    /// </summary>
    public byte[] AudioBytes { get; set; }

    /// <summary>
    /// The length of the audio data in bytes.
    /// </summary>
    public int Length { get; set; }
  }
}
