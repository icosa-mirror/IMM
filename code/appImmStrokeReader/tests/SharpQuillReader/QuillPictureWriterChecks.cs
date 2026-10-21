using System;
using System.IO;
using System.Linq;
using SharpQuill;

static class QuillPictureWriterChecks
{
    public static void Run()
    {
        string tempRoot = Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar);
        string directory = Path.Combine(tempRoot, $"imm-quill-picture-writer-{Guid.NewGuid():N}");
        try
        {
            var sequence = Sequence.CreateDefault(); sequence.RootLayer.Children.Clear();
            var group = new LayerGroup("Media"); sequence.RootLayer.Children.Add(group);
            foreach (var type in new[] { PictureType.TwoD, PictureType.ThreeSixty_Equirect_Mono, PictureType.ThreeSixty_Equirect_Stereo })
                foreach (bool alpha in new[] { false, true })
                    group.Children.Add(new LayerPicture($"{type}-{alpha}")
                    {
                        PictureType = type, ViewerLocked = alpha, Opacity = 0.5f, Visible = !alpha,
                        Transform = new Transform { Scale = 2, Translation = new Vector3(3, 4, 5), Rotation = Quaternion.Identity },
                        Data = new PictureData { Width = 2, Height = 1, HasAlpha = alpha,
                            Pixels = alpha ? new byte[] { 1, 2, 3, 0, 4, 5, 6, 128 } : new byte[] { 1, 2, 3, 4, 5, 6 } }
                    });
            QuillSequenceWriter.Write(sequence, directory);
            var read = QuillSequenceReader.Read(directory);
            var pictures = ((LayerGroup)read.RootLayer.Children.Single()).Children.Cast<LayerPicture>().ToArray();
            Require(pictures.Length == 6, "Picture children were silently omitted.");
            using (var binary = new BinaryReader(File.OpenRead(Path.Combine(directory, "Quill.qbin"))))
                for (int index = 0; index < pictures.Length; index++)
                {
                    var expected = (LayerPicture)group.Children[index]; var actual = pictures[index];
                    Require(actual.PictureType == expected.PictureType && actual.ViewerLocked == expected.ViewerLocked &&
                        actual.Opacity == expected.Opacity && actual.Visible == expected.Visible && actual.Transform.Scale == 2 &&
                        actual.Transform.Translation.X == 3 && actual.Data.Width == 2 && actual.Data.Height == 1 &&
                        actual.Data.HasAlpha == expected.Data.HasAlpha && actual.Data.Pixels.SequenceEqual(expected.Data.Pixels),
                        "Picture metadata/pixels changed in project read-back.");
                    binary.BaseStream.Position = actual.DataFileOffset;
                    Require(binary.ReadBytes(8).SequenceEqual(new byte[] { 0, 0, 0, 1, 2, (byte)(actual.Data.HasAlpha ? 7 : 6), 0, 0 }) &&
                        binary.ReadUInt32() == 2 && binary.ReadUInt32() == 1 && binary.ReadUInt32() == 1 &&
                        binary.ReadBytes(actual.Data.Pixels.Length).SequenceEqual(actual.Data.Pixels), "Picture header/payload differs from the observed Quill layout.");
                }
            var files = new[] { "Quill.json", "Quill.qbin", "State.json" }.ToDictionary(name => name, name => File.ReadAllBytes(Path.Combine(directory, name)));
            var first = (LayerPicture)group.Children[0];
            PictureData valid = first.Data;
            foreach (Action corrupt in new Action[]
            {
                () => first.Data = null,
                () => first.Data = new PictureData { Width = 2, Height = 1, Pixels = new byte[5] },
                () => first.Data = new PictureData { Width = int.MaxValue, Height = int.MaxValue, HasAlpha = true, Pixels = new byte[1] },
                () => first.PictureType = PictureType.TwoDDepth,
                () => first.PictureType = PictureType.Unknown,
                () => group.Children.Add(new LayerModel("Unsupported"))
            })
            {
                first.Data = valid; first.PictureType = PictureType.TwoD;
                group.Children.RemoveAll(layer => layer is LayerModel);
                corrupt(); bool rejected = false;
                try { QuillSequenceWriter.Write(sequence, directory); }
                catch (Exception exception) when (exception is InvalidDataException || exception is NotSupportedException) { rejected = true; }
                Require(rejected && files.All(file => file.Value.SequenceEqual(File.ReadAllBytes(Path.Combine(directory, file.Key)))),
                    "Invalid/unsupported media overwrote an existing project.");
            }
            using (var stream = new MemoryStream())
            using (var writer = new QBinWriter(stream))
            {
                bool rejected = false;
                try { writer.Write(new PictureData { Width = 2, Height = 1, Pixels = new byte[5] }); }
                catch (InvalidDataException) { rejected = true; }
                Require(rejected && stream.Length == 0, "Invalid direct picture write emitted a partial header.");
            }
            Console.WriteLine("[IMM_QUILL_PICTURE_WRITER] Passed nested RGB/RGBA picture projection/metadata/pixel read-back, observed headers and invalid-media output preservation.");
        }
        finally
        {
            Require(Path.GetDirectoryName(Path.GetFullPath(directory)) == tempRoot, "Unexpected test cleanup target.");
            if (Directory.Exists(directory)) Directory.Delete(directory, true);
        }
    }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
}
