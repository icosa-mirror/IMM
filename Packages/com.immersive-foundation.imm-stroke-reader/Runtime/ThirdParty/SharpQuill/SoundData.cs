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
    /// WAV file bytes containing the decoded embedded PCM samples.
    /// </summary>
    public byte[] AudioBytes { get; set; }

    /// <summary>
    /// The length of the audio data in bytes.
    /// </summary>
    public int Length { get; set; }
  }
}
