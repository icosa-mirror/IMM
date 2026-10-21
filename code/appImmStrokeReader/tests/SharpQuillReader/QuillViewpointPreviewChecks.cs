using System;
using System.IO;
using System.Linq;
using Newtonsoft.Json.Linq;
using SharpQuill;

static class QuillViewpointPreviewChecks
{
    public static void Run()
    {
        string tempRoot = Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar);
        string directory = Path.Combine(tempRoot, $"imm-quill-viewpoint-preview-{Guid.NewGuid():N}");
        try
        {
            var sequence = Sequence.CreateDefault(); sequence.RootLayer.Children.Clear();
            var absent = LayerViewpoint.CreateDefault(); absent.Name = "Absent"; absent.DataFileOffset = 999;
            var rgb = LayerViewpoint.CreateDefault(); rgb.Name = "RGB";
            rgb.PreviewData = new PictureData { Width = 2, Height = 1, Pixels = new byte[] { 1, 2, 3, 4, 5, 6 } };
            var picture = new LayerPicture("Picture") { PictureType = PictureType.TwoD, Data = new PictureData { Width = 1, Height = 1, Pixels = new byte[] { 9, 8, 7 } } };
            var rgba = LayerViewpoint.CreateDefault(); rgba.Name = "RGBA";
            rgba.PreviewData = new PictureData { Width = 1, Height = 1, HasAlpha = true, Pixels = new byte[] { 6, 5, 4, 128 } };
            sequence.RootLayer.Children.Add(absent); sequence.RootLayer.Children.Add(rgb);
            sequence.RootLayer.Children.Add(picture); sequence.RootLayer.Children.Add(rgba);
            QuillSequenceWriter.Write(sequence, directory);
            var read = QuillSequenceReader.Read(directory);
            var views = read.RootLayer.Children.OfType<LayerViewpoint>().ToArray();
            Require(absent.DataFileOffset == 0 && views[0].DataFileOffset == 0 && views[0].PreviewData == null,
                "Absent preview retained a stale offset.");
            Require(rgb.DataFileOffset == 8 && picture.DataFileOffset == 34 && rgba.DataFileOffset == 57 &&
                views[1].PreviewData.Width == 2 && views[1].PreviewData.Height == 1 && !views[1].PreviewData.HasAlpha &&
                views[1].PreviewData.Pixels.SequenceEqual(rgb.PreviewData.Pixels) && views[2].PreviewData.HasAlpha &&
                views[2].PreviewData.Pixels.SequenceEqual(rgba.PreviewData.Pixels), "Mixed media preview offsets or pixels changed.");
            string jsonPath = Path.Combine(directory, "Quill.json"), binaryPath = Path.Combine(directory, "Quill.qbin");
            var json = JObject.Parse(File.ReadAllText(jsonPath));
            var children = (JArray)json["Sequence"]["RootLayer"]["Implementation"]["Children"];
            Require(children[0]["Implementation"]["DataFileOffset"] == null &&
                (string)children[1]["Implementation"]["DataFileOffset"] == "8", "Preview JSON offset presence changed.");
            var files = new[] { "Quill.json", "Quill.qbin", "State.json" }.ToDictionary(name => name, name => File.ReadAllBytes(Path.Combine(directory, name)));
            PictureData valid = rgb.PreviewData;
            foreach (var invalid in new[] { new PictureData { Width = 2, Height = 1, Pixels = new byte[5] },
                new PictureData { Width = int.MaxValue, Height = int.MaxValue, HasAlpha = true, Pixels = new byte[1] } })
            {
                rgb.PreviewData = invalid; bool rejected = false;
                try { QuillSequenceWriter.Write(sequence, directory); } catch (InvalidDataException) { rejected = true; }
                Require(rejected && files.All(file => file.Value.SequenceEqual(File.ReadAllBytes(Path.Combine(directory, file.Key)))),
                    "Invalid preview replaced existing project output.");
            }
            rgb.PreviewData = valid;
            byte[] originalBinary = File.ReadAllBytes(binaryPath);
            foreach (string invalidOffset in new[] { "1", "FFFFFFFFFFFFFFFF", "invalid", originalBinary.Length.ToString("X") })
            {
                children[1]["Implementation"]["DataFileOffset"] = invalidOffset;
                File.WriteAllText(jsonPath, json.ToString());
                ExpectInvalid(directory, "Invalid preview offset was accepted.");
            }
            File.WriteAllBytes(jsonPath, files["Quill.json"]);
            foreach (Action<byte[]> corrupt in new Action<byte[]>[]
            {
                data => data[13] = 5,
                data => { for (int i = 16; i < 24; i++) data[i] = 255; }
            })
            {
                byte[] bytes = (byte[])originalBinary.Clone(); corrupt(bytes); File.WriteAllBytes(binaryPath, bytes);
                ExpectInvalid(directory, "Malformed preview header was accepted.");
            }
            File.WriteAllBytes(binaryPath, originalBinary.Take(originalBinary.Length - 1).ToArray());
            ExpectInvalid(directory, "Truncated preview pixels were accepted.");
            rgb.PreviewData = null; rgba.PreviewData = null;
            QuillSequenceWriter.Write(sequence, directory);
            Require(rgb.DataFileOffset == 0 && rgba.DataFileOffset == 0 &&
                QuillSequenceReader.Read(directory).RootLayer.Children.OfType<LayerViewpoint>().All(view => view.PreviewData == null),
                "Removing preview retained binary data or offsets.");
            Console.WriteLine("[IMM_QUILL_VIEWPOINT_PREVIEW] Passed absent/RGB/RGBA previews, mixed offsets, invalid output preservation and bounded malformed input rejection.");
        }
        finally
        {
            Require(Path.GetDirectoryName(Path.GetFullPath(directory)) == tempRoot, "Unexpected test cleanup target.");
            if (Directory.Exists(directory)) Directory.Delete(directory, true);
        }
    }
    static void ExpectInvalid(string directory, string message)
    {
        bool rejected = false;
        try { QuillSequenceReader.Read(directory); } catch (InvalidDataException) { rejected = true; }
        Require(rejected, message);
    }
    static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
