using System;
using System.Collections.Generic;
using System.Text;

namespace SharpQuill
{
  public class Keyframe<T>
  {
    /// <summary>
    /// Time of the keyframe in Quill timeline ticks (12600 per second), relative to the parent sequence.
    /// </summary>
    public int Time { get; set; }

    /// <summary>
    /// Value at that keyframe.
    /// </summary>
    public T Value { get; set; }

    /// <summary>
    /// Interpolation mode at that keyframe.
    /// </summary>
    public Interpolation Interpolation { get; set; }

    public Keyframe()
    {
    }

    public Keyframe(int time, T value, Interpolation interpolation)
    {
      this.Time = time;
      this.Value = value;
      this.Interpolation = interpolation;
    }
  }
}
