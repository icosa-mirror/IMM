using System;
using System.IO;
using System.Linq;
using System.Threading;
using ImmPlayer.Authoring;
using ImmPlayer.Exporter;
using NUnit.Framework;
using UnityEditor;
using UnityEngine;

namespace ImmPlayer.Editor.Tests
{
    public sealed class QuillToImmEditorTests
    {
        private string source;
        private string assets;

        [SetUp]
        public void SetUp()
        {
            source = Path.Combine(Application.temporaryCachePath, $"imm-quill-editor-{Guid.NewGuid():N}");
            Directory.CreateDirectory(source);
            assets = $"Assets/IMMQuillEditorTests-{Guid.NewGuid():N}";
        }

        [TearDown]
        public void TearDown()
        {
            AssetDatabase.DeleteAsset(assets);
            if (Directory.Exists(source)) Directory.Delete(source, true);
        }

        [Test]
        public void OutputMustBeAnImmFileInsideAssets()
        {
            Assert.Throws<ArgumentException>(() => QuillToImmAssetConversion.ResolveAssetOutput("Packages/out.imm"));
            Assert.Throws<ArgumentException>(() => QuillToImmAssetConversion.ResolveAssetOutput("Assets/../out.imm"));
            Assert.Throws<ArgumentException>(() => QuillToImmAssetConversion.ResolveAssetOutput("Assets/out.json"));
            Assert.Throws<ArgumentException>(() => QuillToImmAssetConversion.ResolveAssetOutput("Assets/.hidden/out.imm"));
            Assert.Throws<ArgumentException>(() => QuillToImmAssetConversion.ResolveAssetOutput("Assets/.out.imm"));
            Assert.That(QuillToImmAssetConversion.ResolveAssetOutput($"{assets}/out.imm"), Does.EndWith("out.imm"));
        }

        [Test]
        public void ExistingOutputAndPreCancellationArePreserved()
        {
            string output = QuillToImmAssetConversion.ResolveAssetOutput($"{assets}/out.imm");
            Directory.CreateDirectory(Path.GetDirectoryName(output));
            File.WriteAllText(output, "keep original");
            var refused = QuillToImmAssetConversion.ConvertAndImport(source, $"{assets}/out.imm");
            Assert.That(refused.OutputPublished, Is.False);
            Assert.That(refused.Error, Does.Contain("already exists"));
            Assert.That(File.ReadAllText(output), Is.EqualTo("keep original"));
            var cancelled = QuillToImmAssetConversion.ConvertAndImport(source, $"{assets}/out.imm", overwrite: true,
                options: new ImmAuthoringOperationOptions(new CancellationToken(true)));
            Assert.That(cancelled.Cancelled, Is.True);
            Assert.That(cancelled.OutputPublished, Is.False);
            Assert.That(File.ReadAllText(output), Is.EqualTo("keep original"));
        }

        [Test]
        public void NativeProjectConversionImportsAnAssetAndProtectsExistingOutput()
        {
            string original = Path.Combine(source, "fixture.imm");
            using (var document = ImmAuthoringDocument.Create(ExportSequenceType.Animated, 30, Color.black).Value)
            {
                long paint = document.CreatePaintLayer(0, ImmAuthoringLayerProperties.Default("Editor fixture")).Value;
                long drawing = document.CreateDrawing(paint).Value;
                var points = new[] {
                    new PaintPoint { Position = Vector3.zero, Normal = Vector3.forward, Direction = Vector3.up, Color = Color.red, Alpha = 1, Width = 0.05f },
                    new PaintPoint { Position = Vector3.right, Normal = Vector3.forward, Direction = Vector3.up, Color = Color.blue, Alpha = 1, Width = 0.05f }
                };
                Assert.That(document.CreateStroke(drawing, BrushSectionType.Circle, VisibilityType.Always, points).Succeeded, Is.True);
                Assert.That(document.AppendFrame(paint, drawing).Succeeded, Is.True);
                var exported = ImmAuthoringCompiler.ExportToFile(document, original);
                Assert.That(exported.Succeeded, Is.True, exported.Message);
            }
            var read = global::ImmStrokeReader.SharpQuillCompat.ReadImmAsHierarchyWithReport(original);
            Assert.That(read.Succeeded, Is.True, string.Join("; ", read.Issues.Select(issue => issue.Message)));
            string project = Path.Combine(source, "Quill source");
            SharpQuill.QuillSequenceWriter.Write(read.Sequence, project);
            var result = QuillToImmAssetConversion.ConvertAndImport(project, $"{assets}/out.imm");
            string issues = result.Conversion == null ? result.Error : string.Join("; ", result.Conversion.Issues.Select(issue => issue.Message));
            Assert.That(result.Succeeded, Is.True, issues);
            Assert.That(result.OutputPublished && result.AssetImported, Is.True);
            Assert.That(AssetDatabase.AssetPathToGUID(result.AssetPath), Is.Not.Empty);
            string guid = AssetDatabase.AssetPathToGUID(result.AssetPath);
            Assert.That(AssetDatabase.LoadMainAssetAtPath(result.AssetPath), Is.Not.Null);
            string full = QuillToImmAssetConversion.ResolveAssetOutput(result.AssetPath);
            byte[] before = File.ReadAllBytes(full);
            var repeat = QuillToImmAssetConversion.ConvertAndImport(project, result.AssetPath);
            Assert.That(repeat.Succeeded, Is.False);
            Assert.That(File.ReadAllBytes(full), Is.EqualTo(before));
            var replaced = QuillToImmAssetConversion.ConvertAndImport(project, result.AssetPath, overwrite: true);
            Assert.That(replaced.Succeeded, Is.True, replaced.Error);
            Assert.That(AssetDatabase.AssetPathToGUID(result.AssetPath), Is.EqualTo(guid));
            Assert.That(Directory.GetFiles(Path.GetDirectoryName(full), ".imm-quill-*.imm"), Is.Empty);
        }
    }
}
