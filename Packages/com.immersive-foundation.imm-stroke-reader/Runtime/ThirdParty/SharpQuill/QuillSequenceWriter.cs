using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SharpQuill
{
  /// <summary>
  /// A QuillSequenceWriter writes the scene hierarchy and data to a quill project folder.
  /// </summary>
  public static class QuillSequenceWriter
  {
    public static void Write(Sequence seq, string path)
    {
      if (string.IsNullOrEmpty(path))
        throw new InvalidOperationException();

      if (seq == null || seq.RootLayer == null)
        throw new InvalidDataException("A Quill project requires a root layer.");
      if (seq.Gallery != null && seq.Gallery.Pictures.Count != 0)
        throw new NotSupportedException("Writing Gallery pictures is not supported.");
      ValidateLayerData(seq.RootLayer);

      if (!Directory.Exists(path))
        Directory.CreateDirectory(path);

      string sequenceFilename = Path.Combine(path, "Quill.json");
      string paintDataFilename = Path.Combine(path, "Quill.qbin");
      string stateFilename = Path.Combine(path, "State.json");

      // Write the qbin file first to update the data offsets.
      using (FileStream qbinStream = File.Create(paintDataFilename))
      {
        QBinWriter qbinWriter = new QBinWriter(qbinStream);
        WriteLastStrokeId(seq, qbinWriter);
        WriteLayerData(seq.RootLayer, qbinWriter);
        qbinWriter.Flush();
      }

      WriteManifest(seq, sequenceFilename);
      WriteState(stateFilename);
    }

    private static void WriteManifest(Sequence seq, string path)
    {
      // Ref: http://www.newtonsoft.com/json/help/html/CreatingLINQtoJSON.htm
      JObject root = new JObject();
      root.Add(new JProperty("Version", 1));
      root.Add(new JProperty("Sequence", WriteSequence(seq)));
      
      // Note: the formatter will fully indent arrays of primitives.
      string json = JsonConvert.SerializeObject(root, Formatting.Indented);
      File.WriteAllText(path, json);
    }

    private static JObject WriteSequence(Sequence seq)
    {
      JObject jSeq = new JObject();
      jSeq.Add(new JProperty("Metadata", WriteMetadata(seq.Metadata)));
      jSeq.Add(new JProperty("Gallery", WriteGallery(seq.Gallery)));
      jSeq.Add(new JProperty("BackgroundColor", WriteColor(seq.BackgroundColor)));
      jSeq.Add(new JProperty("DefaultViewpoint", seq.DefaultViewpoint));
      if (seq.TrackingOrigin.HasValue)
        jSeq.Add(new JProperty("TrackingOrigin", seq.TrackingOrigin.Value));
      if (seq.AnimateOnStart.HasValue)
        jSeq.Add(new JProperty("AnimateOnStart", seq.AnimateOnStart.Value));
      jSeq.Add(new JProperty("Framerate", seq.Framerate));
      jSeq.Add(new JProperty("ExportStart", seq.ExportStart));
      jSeq.Add(new JProperty("ExportEnd", seq.ExportEnd));
      jSeq.Add(new JProperty("CameraResolution", WriteSize(seq.CameraResolution)));
      jSeq.Add(new JProperty("RootLayer", WriteLayer(seq.RootLayer)));
      return jSeq;
    }

    private static JObject WriteMetadata(Metadata value)
    {
      JObject jObj = new JObject();

      jObj.Add(new JProperty("Title", value.Title));
      jObj.Add(new JProperty("Description", value.Description));
      jObj.Add(new JProperty("ThumbnailCropPosition", value.ThumbnailCropPosition));
      
      return jObj;
    }

    private static JObject WriteGallery(Gallery value)
    {
      JObject jObj = new JObject();

      if (value.Pictures.Count == 0)
      {
        jObj.Add(new JProperty("Thumbnails", new JObject()));
        jObj.Add(new JProperty("Pictures", new JArray()));
      }
      else
      {
        throw new NotImplementedException();
      }
      
      return jObj;
    }

    private static JObject WriteLayer(Layer layer)
    {
      JObject jLayer = new JObject();

      jLayer.Add(new JProperty("Name", layer.Name));
      jLayer.Add(new JProperty("Visible", layer.Visible));
      jLayer.Add(new JProperty("Locked", layer.Locked));
      jLayer.Add(new JProperty("Collapsed", layer.Collapsed));
      jLayer.Add(new JProperty("BBoxVisible", layer.BBoxVisible));
      jLayer.Add(new JProperty("Opacity", layer.Opacity));
      jLayer.Add(new JProperty("Type", layer.Type.ToString()));
      jLayer.Add(new JProperty("IsModelTopLayer", layer.IsModelTopLayer));
      jLayer.Add(new JProperty("KeepAlive", WriteKeepAlive(layer.KeepAlive)));
      jLayer.Add(new JProperty("Transform", WriteTransform(layer.Transform)));
      jLayer.Add(new JProperty("Pivot", WriteTransform(layer.Pivot)));
      jLayer.Add(new JProperty("Animation", WriteAnimation(layer.Animation)));
      jLayer.Add(new JProperty("Implementation", WriteLayerImplementation(layer)));
      return jLayer;
    }

    private static JObject WriteLayerImplementation(Layer layer)
    {
      switch (layer.Type)
      {
        case LayerType.Group:
          return WriteLayerImplementationGroup(layer as LayerGroup);

        case LayerType.Paint:
          return WriteLayerImplementationPaint(layer as LayerPaint);

        case LayerType.Viewpoint:
          return WriteLayerImplementationViewpoint(layer as LayerViewpoint);

        case LayerType.Camera:
          return WriteLayerImplementationCamera(layer as LayerCamera);

        case LayerType.Picture:
          return WriteLayerImplementationPicture(layer as LayerPicture);

        case LayerType.Sound:
          return WriteLayerImplementationSound(layer as LayerSound);

        case LayerType.Model:
        case LayerType.Unknown:
        default:
          throw new NotSupportedException($"Writing layer type {layer.Type} is not supported.");
      }
    }

    private static bool IsSupported(Layer layer)
    {
      if (layer == null)
        return false;

      return layer.Type == LayerType.Group ||
             layer.Type == LayerType.Paint ||
             layer.Type == LayerType.Camera ||
             layer.Type == LayerType.Viewpoint ||
             layer.Type == LayerType.Picture ||
             layer.Type == LayerType.Sound;
    }

    private static JObject WriteLayerImplementationGroup(LayerGroup layer)
    {
      JObject jLayer = new JObject();
      JArray jChildren = new JArray();

      foreach (Layer child in layer.Children)
      {
        jChildren.Add(WriteLayer(child));
      }

      jLayer.Add(new JProperty("Children", jChildren));

      return jLayer;
    }

    private static JObject WriteLayerImplementationPaint(LayerPaint layer)
    {
      JObject jLayer = new JObject();

      jLayer.Add(new JProperty("Framerate", layer.Framerate));
      jLayer.Add(new JProperty("MaxRepeatCount", layer.MaxRepeatCount));

      JArray jDrawings = new JArray();
      foreach (Drawing drawing in layer.Drawings)
        jDrawings.Add(WriteDrawing(drawing));

      jLayer.Add(new JProperty("Drawings", jDrawings));

      JArray jFrames = new JArray();
      foreach (float frame in layer.Frames)
        jFrames.Add(frame);

      jLayer.Add(new JProperty("Frames", jFrames));
      
      return jLayer;
    }

    private static string WritePictureType(PictureType type)
    {
      switch (type)
      {
        case PictureType.TwoD: return "2D";
        case PictureType.ThreeSixty_Equirect_Mono: return "360_Equirect_Mono";
        case PictureType.ThreeSixty_Equirect_Stereo: return "360_Equirect_Stereo";
        default: throw new NotSupportedException($"Writing picture projection {type} is not supported.");
      }
    }

    private static JObject WriteLayerImplementationPicture(LayerPicture layer)
    {
      return new JObject
      {
        ["Type"] = WritePictureType(layer.PictureType),
        ["ViewerLocked"] = layer.ViewerLocked,
        ["DataFileOffset"] = layer.DataFileOffset.ToString("X"),
        ["ImportFilePath"] = layer.ImportFilePath ?? ""
      };
    }

    private static JObject WriteLayerImplementationSound(LayerSound layer)
    {
      var modifier = layer.Modifiers[0];
      var settings = new JObject { ["Type"] = modifier.Type.ToString() };
      if (modifier.Type == SoundModifierType.Cone)
      {
        settings["AngleIn"] = modifier.AngleIn;
        settings["AngleBand"] = modifier.AngleBand;
        settings["AttenOut"] = modifier.AttenOut;
      }
      return new JObject
      {
        ["DataFileOffset"] = layer.DataFileOffset.ToString("X"),
        ["ImportFilePath"] = layer.ImportFilePath ?? "",
        ["Type"] = layer.SoundType.ToString(), ["Gain"] = layer.Gain, ["Loop"] = layer.Loop,
        ["Attenuation"] = new JObject { ["Mode"] = layer.Attenuation.Mode.ToString(), ["Minimum"] = layer.Attenuation.Minimum, ["Maximum"] = layer.Attenuation.Maximum },
        ["Modifiers"] = new JArray(settings)
      };
    }

    private static bool IsNonnegativeFinite(float value) => !float.IsNaN(value) && !float.IsInfinity(value) && value >= 0;

    private static void ValidateSound(LayerSound layer)
    {
      if (!Enum.IsDefined(typeof(SoundType), layer.SoundType) || layer.Attenuation == null ||
          !Enum.IsDefined(typeof(SoundAttenuationMode), layer.Attenuation.Mode) ||
          layer.Modifiers == null || layer.Modifiers.Count != 1 || layer.Modifiers[0] == null ||
          !Enum.IsDefined(typeof(SoundModifierType), layer.Modifiers[0].Type))
        throw new NotSupportedException("Sound spatial type, attenuation and one None/Cone modifier are required.");
      var modifier = layer.Modifiers[0];
      if (!IsNonnegativeFinite(layer.Gain) || !IsNonnegativeFinite(layer.Attenuation.Minimum) ||
          !IsNonnegativeFinite(layer.Attenuation.Maximum) || layer.Attenuation.Maximum < layer.Attenuation.Minimum ||
          !IsNonnegativeFinite(modifier.AngleIn) || !IsNonnegativeFinite(modifier.AngleBand) || !IsNonnegativeFinite(modifier.AttenOut) ||
          (modifier.Type == SoundModifierType.Cone && layer.SoundType != SoundType.Positional) ||
          (modifier.Type == SoundModifierType.None && (modifier.AngleIn != 0 || modifier.AngleBand != 0 || modifier.AttenOut != 0)))
        throw new InvalidDataException("Sound gain, attenuation or modifier values are invalid.");
      QBinWriter.InspectSoundData(layer.Data);
    }

    private static void ValidateLayerData(Layer layer)
    {
      if (!IsSupported(layer))
        throw new NotSupportedException($"Writing layer type {layer?.Type.ToString() ?? "null"} is not supported.");
      if (layer is LayerGroup group)
      {
        foreach (Layer child in group.Children) ValidateLayerData(child);
      }
      else if (layer is LayerPicture picture)
      {
        WritePictureType(picture.PictureType);
        QBinWriter.ValidatePictureData(picture.Data);
      }
      else if (layer is LayerSound sound)
        ValidateSound(sound);
      else if (layer is LayerViewpoint viewpoint && viewpoint.PreviewData != null)
        QBinWriter.ValidatePictureData(viewpoint.PreviewData);
    }

    private static JObject WriteLayerImplementationCamera(LayerCamera layer)
    {
      JObject jLayer = new JObject();
      jLayer.Add(new JProperty("FOV", layer.FOV));
      return jLayer;
    }

    private static JObject WriteLayerImplementationViewpoint(LayerViewpoint layer)
    {
      JObject jLayer = new JObject();
      jLayer.Add(new JProperty("Version", layer.Version));
      jLayer.Add(new JProperty("Color", WriteColor(layer.Color)));
      jLayer.Add(new JProperty("Sphere", WriteVector4(layer.Sphere)));
      jLayer.Add(new JProperty("AllowTranslationX", layer.AllowTranslationX));
      jLayer.Add(new JProperty("AllowTranslationY", layer.AllowTranslationY));
      jLayer.Add(new JProperty("AllowTranslationZ", layer.AllowTranslationZ));
      jLayer.Add(new JProperty("Exporting", layer.Exporting));
      jLayer.Add(new JProperty("ShowingVolume", layer.ShowingVolume));
      jLayer.Add(new JProperty("TypeStr", layer.TypeStr));
      if (layer.PreviewData != null)
        jLayer.Add(new JProperty("DataFileOffset", layer.DataFileOffset.ToString("X")));
      return jLayer;
    }

    private static JArray WriteColor(Color value)
    {
      return new JArray(value.R, value.G, value.B);
    }

    private static JArray WriteSize(Size value)
    {
      return new JArray(value.Width, value.Height);
    }

    private static JArray WriteVector3(Vector3 value)
    {
      return new JArray(value.X, value.Y, value.Z);
    }

    private static JArray WriteVector4(Vector4 value)
    {
      return new JArray(value.X, value.Y, value.Z, value.W);
    }

    private static JArray WriteQuaternion(Quaternion value)
    {
      return new JArray(value.X, value.Y, value.Z, value.W);
    }

    private static JObject WriteTransform(Transform value)
    {
      JObject jT = new JObject();

      jT.Add(new JProperty("Rotation", WriteQuaternion(value.Rotation)));
      jT.Add(new JProperty("Scale", value.Scale));
      jT.Add(new JProperty("Flip", value.Flip));
      jT.Add(new JProperty("Translation", WriteVector3(value.Translation)));

      return jT;
    }

    private static JArray WriteBoundingBox(BoundingBox value)
    {
      return new JArray(value.MinX, value.MaxX, value.MinY, value.MaxY, value.MinZ, value.MaxZ);
    }

    private static JObject WriteKeepAlive(KeepAlive value)
    {
      JObject jKA = new JObject();

      jKA.Add(new JProperty("Type", value.Type.ToString()));

      return jKA;
    }

    private static JObject WriteAnimation(Animation value)
    {
      JObject jAnimation = new JObject();

      jAnimation.Add(new JProperty("Duration", value.Duration));
      jAnimation.Add(new JProperty("Timeline", value.Timeline));
      jAnimation.Add(new JProperty("StartOffset", value.StartOffset));
      jAnimation.Add(new JProperty("MaxRepeatCount", value.MaxRepeatCount));

      if (value.Keys == null)
      {
        jAnimation.Add(new JProperty("Keys", new JObject()));
      }
      else
      {
        jAnimation.Add(new JProperty("Keys", WriteKeyframes(value.Keys)));
      }
      
      return jAnimation;
    }

    private static JObject WriteKeyframes(Keyframes value)
    {
      JObject jKeyframes = new JObject();

      // Visibility.
      JArray jVisibility = new JArray();
      foreach (var kf in value.Visibility)
      {
        JObject jK = new JObject();
        jK.Add(new JProperty("Time", kf.Time));
        jK.Add(new JProperty("Value", kf.Value));
        jK.Add(new JProperty("Interpolation", kf.Interpolation.ToString()));
        jVisibility.Add(jK);
      }
      jKeyframes.Add(new JProperty("Visibility", jVisibility));

      // Offset.
      JArray jOffset = new JArray();
      foreach (var kf in value.Offset)
      {
        JObject jK = new JObject();
        jK.Add(new JProperty("Time", kf.Time));
        jK.Add(new JProperty("Value", kf.Value));
        jK.Add(new JProperty("Interpolation", kf.Interpolation.ToString()));
        jOffset.Add(jK);
      }
      jKeyframes.Add(new JProperty("Offset", jOffset));

      // Opacity.
      if (value.Opacity.Count > 0)
      {
        JArray jOpacity = new JArray();
        foreach (var kf in value.Opacity)
        {
          JObject jK = new JObject();
          jK.Add(new JProperty("Time", kf.Time));
          jK.Add(new JProperty("Value", kf.Value));
          jK.Add(new JProperty("Interpolation", kf.Interpolation.ToString()));
          jOpacity.Add(jK);
        }
        jKeyframes.Add(new JProperty("Opacity", jOpacity));
      }

      // Transform.
      if (value.Transform.Count > 0)
      {
        JArray jTransform = new JArray();
        foreach (var kf in value.Transform)
        {
          JObject jK = new JObject();
          jK.Add(new JProperty("Time", kf.Time));
          jK.Add(new JProperty("Value", WriteTransform(kf.Value)));
          jK.Add(new JProperty("Interpolation", kf.Interpolation.ToString()));
          jTransform.Add(jK);
        }
        jKeyframes.Add(new JProperty("Transform", jTransform));
      }

      return jKeyframes;
    }

    private static JObject WriteDrawing(Drawing drawing)
    {
      JObject jDrawing = new JObject();

      jDrawing.Add(new JProperty("BoundingBox", WriteBoundingBox(drawing.BoundingBox)));
      jDrawing.Add(new JProperty("DataFileOffset", drawing.DataFileOffset.ToString("X")));

      return jDrawing;
    }

    private static void WriteLastStrokeId(Sequence seq, QBinWriter qbinWriter)
    {
      // 8-byte header.
      // This value is sometimes seemingly broken in quill files.
      qbinWriter.Write(seq.LastStrokeId);
      int padding = 0;
      qbinWriter.Write(padding);
    }

    /// <summary>
    /// Recursive function to write paint/picture/sound/preview data and update the layers offsets.
    /// This is called with the root layer and will write all the data for the sequence.
    /// </summary>
    private static void WriteLayerData(Layer layer, QBinWriter qbinWriter)
    {
      if (layer.Type == LayerType.Group)
      {
        foreach (Layer l in ((LayerGroup)layer).Children)
        {
          if (l != null)
            WriteLayerData(l, qbinWriter);
        }
      }
      else if (layer is LayerPicture picture)
      {
        picture.DataFileOffset = qbinWriter.BaseStream.Position;
        qbinWriter.Write(picture.Data);
      }
      else if (layer is LayerSound sound)
      {
        sound.DataFileOffset = qbinWriter.BaseStream.Position;
        qbinWriter.Write(sound.Data);
      }
      else if (layer is LayerViewpoint viewpoint)
      {
        viewpoint.DataFileOffset = 0;
        if (viewpoint.PreviewData != null)
        {
          viewpoint.DataFileOffset = qbinWriter.BaseStream.Position;
          qbinWriter.Write(viewpoint.PreviewData);
        }
      }
      else if (layer.Type == LayerType.Paint)
      {
        foreach (Drawing drawing in ((LayerPaint)layer).Drawings)
        {
          drawing.DataFileOffset = qbinWriter.BaseStream.Position;
          qbinWriter.Write(drawing.Data);
        }
      }
    }
    
    private static void WriteState(string path)
    {
      // We need to write the scene state to be able to read the file in Quill.
      // Use a dummy structure with all default values.
      // Unlike quill default new document, we explicitely not start with any paint layer in move or paint mode.
      
      JObject root = new JObject();
      
      JObject jQuill = new JObject();

      JObject jRulers = new JObject();
      jRulers.Add(new JProperty("ShowGrid", false));
      jQuill.Add(new JProperty("Rulers", jRulers));

      JObject jSurface = new JObject();
      jSurface.Add(new JProperty("Texture", "None"));
      jSurface.Add(new JProperty("Scale", 1.0f));

      JObject jDetailRender = new JObject();
      jDetailRender.Add(new JProperty("Surface", jSurface));
      jQuill.Add(new JProperty("DetailRender", jDetailRender));

      jQuill.Add(new JProperty("MoveLayer", ""));
      jQuill.Add(new JProperty("PaintLayer", ""));
      jQuill.Add(new JProperty("CameraLayer", ""));
      jQuill.Add(new JProperty("DirectManipulation", 0));

      jQuill.Add(new JProperty("ToolName", "Paint"));

      JObject jTool = new JObject();
      jTool.Add(new JProperty("BrushID", 3));
      jTool.Add(new JProperty("Color", new List<float>() { 0, 0, 0 }));
      jTool.Add(new JProperty("Opacity", 1.0f));
      jTool.Add(new JProperty("Size", 0.01f));
      jTool.Add(new JProperty("TransparentTaper", "None"));
      jTool.Add(new JProperty("WidthTaper", "Pressure"));
      jTool.Add(new JProperty("DirectionalStroke", false));
      jQuill.Add(new JProperty("Tool", jTool));

      JArray jColorPalette = new JArray();
      for (int i = 0; i < 16; i++)
      {
        float luma = 0.82f;
        jColorPalette.Add(luma);
        jColorPalette.Add(luma);
        jColorPalette.Add(luma);
      }
      jQuill.Add(new JProperty("ColorPalette", jColorPalette));

      root.Add(new JProperty("Quill", jQuill));
      
      string json = JsonConvert.SerializeObject(root, Formatting.Indented);
      File.WriteAllText(path, json);
    }
  }
}
