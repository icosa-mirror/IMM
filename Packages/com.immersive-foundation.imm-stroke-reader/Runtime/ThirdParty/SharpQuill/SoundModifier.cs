using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SharpQuill
{
  public class SoundModifier
  {
    public SoundModifierType Type { get; set; }
    public float AngleIn { get; set; }
    public float AngleBand { get; set; }
    public float AttenOut { get; set; }
  }
}
