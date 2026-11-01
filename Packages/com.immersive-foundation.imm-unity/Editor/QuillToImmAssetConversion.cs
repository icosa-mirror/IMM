using System;
using System.IO;
using ImmPlayer.Authoring;
using UnityEditor;
using UnityEngine;

namespace ImmPlayer.Editor
{
    public sealed class QuillToImmAssetConversionResult
    {
        public QuillToImmConversionResult Conversion { get; internal set; }
        public string AssetPath { get; internal set; }
        public bool OutputPublished { get; internal set; }
        public bool AssetImported { get; internal set; }
        public bool Cancelled { get; internal set; }
        public string Error { get; internal set; }
        public bool Succeeded => OutputPublished && AssetImported;
    }

    // Editor-thread adapter only. Reading, mapping and serialization remain in
    // the same runtime converter used by the standalone library.
    public static class QuillToImmAssetConversion
    {
        public static string ResolveAssetOutput(string assetPath)
        {
            if (string.IsNullOrWhiteSpace(assetPath)) throw new ArgumentException("Choose an IMM output file inside Assets.");
            string assets = Path.GetFullPath(Application.dataPath);
            string root = Path.GetDirectoryName(assets);
            string full = Path.GetFullPath(Path.IsPathRooted(assetPath) ? assetPath : Path.Combine(root, assetPath));
#if UNITY_EDITOR_WIN
            const StringComparison pathComparison = StringComparison.OrdinalIgnoreCase;
#else
            const StringComparison pathComparison = StringComparison.Ordinal;
#endif
            if (!full.StartsWith($"{assets}{Path.DirectorySeparatorChar}", pathComparison) ||
                !string.Equals(Path.GetExtension(full), ".imm", StringComparison.OrdinalIgnoreCase))
                throw new ArgumentException("IMM output must be an .imm file inside this project's Assets folder.");
            foreach (string part in full.Substring(assets.Length + 1).Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar))
                if (part.StartsWith(".", StringComparison.Ordinal))
                    throw new ArgumentException("Choose an output path without hidden files or folders; Unity ignores names beginning with a dot.");
            return full;
        }

        public static QuillToImmAssetConversionResult ConvertAndImport(string projectFolder, string assetPath,
            bool allowOmissions = false, bool overwrite = false, ImmAuthoringOperationOptions options = null)
        {
            options = options ?? ImmAuthoringOperationOptions.Default;
            var result = new QuillToImmAssetConversionResult();
            string temporary = null;
            try
            {
                if (EditorApplication.isPlayingOrWillChangePlaymode)
                    throw new InvalidOperationException("Stop Play mode before converting a Quill project.");
                string output = ResolveAssetOutput(assetPath);
                result.AssetPath = $"Assets/{output.Substring(Path.GetFullPath(Application.dataPath).Length + 1).Replace('\\', '/')}";
                if (File.Exists(output) && !overwrite)
                    throw new IOException("Output already exists. Enable replacement to overwrite it after successful conversion.");
                options.CancellationToken.ThrowIfCancellationRequested();
                Directory.CreateDirectory(Path.GetDirectoryName(output));
                temporary = Path.Combine(Path.GetDirectoryName(output), $".imm-quill-{Guid.NewGuid():N}.imm");
                result.Conversion = QuillToImmConverter.ConvertProject(projectFolder, temporary, allowOmissions, options);
                result.Cancelled = options.CancellationToken.IsCancellationRequested ||
                    result.Conversion.Export?.ErrorCode == ImmAuthoringErrorCode.Cancelled;
                if (!result.Conversion.Succeeded || result.Cancelled) return result;
                // Without overwrite, Move itself rejects a destination created
                // after validation. Never use an existence check as that guard.
                if (overwrite && File.Exists(output)) File.Replace(temporary, output, null);
                else File.Move(temporary, output);
                result.OutputPublished = true;
                AssetDatabase.ImportAsset(result.AssetPath, ImportAssetOptions.ForceSynchronousImport);
                result.AssetImported = AssetDatabase.LoadMainAssetAtPath(result.AssetPath) != null;
                if (!result.AssetImported) result.Error = "IMM saved, but Unity did not import the output asset.";
            }
            catch (OperationCanceledException) { result.Cancelled = true; }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException ||
                exception is ArgumentException || exception is InvalidOperationException || exception is NotSupportedException ||
                exception is DllNotFoundException || exception is EntryPointNotFoundException ||
                exception is BadImageFormatException || exception is UnityException)
            { result.Error = exception.Message; }
            finally
            {
                try { if (temporary != null && File.Exists(temporary)) File.Delete(temporary); }
                catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
                { if (result.Error == null) result.Error = $"Could not remove private staging file: {exception.Message}"; }
            }
            return result;
        }
    }
}
