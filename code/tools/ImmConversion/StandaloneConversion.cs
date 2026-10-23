using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Threading;
using ImmPlayer.Authoring;

namespace ImmPlayer.Conversion
{
    public sealed class StandaloneConversionRequest
    {
        public string Source { get; }
        public string Output { get; }
        public string Report { get; }
        public string NativeDirectory { get; }
        public long MaxInputBytes { get; }
        public bool AllowOmissions { get; }
        public bool Overwrite { get; }
        public string ToolVersion { get; }

        public StandaloneConversionRequest(string source, string output, bool allowOmissions = false,
            bool overwrite = false, string report = null, string nativeDirectory = null,
            long? maxInputBytes = null, string toolVersion = null)
        {
            if (string.IsNullOrWhiteSpace(source)) throw new ArgumentException("Choose a Quill project folder.");
            if (string.IsNullOrWhiteSpace(output)) throw new ArgumentException("Choose an IMM output file.");
            Source = Path.GetFullPath(source);
            Output = Path.GetFullPath(output);
            Report = report == null ? null : Path.GetFullPath(report);
            NativeDirectory = Path.TrimEndingDirectorySeparator(Path.GetFullPath(nativeDirectory ?? AppContext.BaseDirectory));
            MaxInputBytes = maxInputBytes ?? ImmAuthoringLimits.Default.MaxInputBytes;
            if (MaxInputBytes <= 0) throw new ArgumentException("The input-size limit must be positive.");
            AllowOmissions = allowOmissions;
            Overwrite = overwrite;
            ToolVersion = toolVersion ?? typeof(StandaloneConversion).Assembly
                .GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion;
        }
    }

    public sealed class StandaloneConversionOutcome
    {
        public StandaloneConversionRequest Request { get; }
        public DateTimeOffset StartedUtc { get; } = DateTimeOffset.UtcNow;
        public DateTimeOffset FinishedUtc { get; internal set; }
        public int ExitCode { get; internal set; } = 3;
        public bool OutputPublished { get; internal set; }
        public QuillToImmConversionResult Conversion { get; internal set; }
        public string Error { get; internal set; }
        public string ReportError { get; internal set; }
        public string CleanupError { get; internal set; }
        public bool IsPartial => Conversion?.IsPartial ?? false;
        public long BytesWritten => OutputPublished ? Conversion.Export.BytesWritten : 0;

        internal StandaloneConversionOutcome(StandaloneConversionRequest request) { Request = request; }
    }

    // Standalone hosts only: Unity continues to compile the original package
    // sources. CLI and desktop use this one native/output/report lifecycle.
    public static class StandaloneConversion
    {
        private static int running;
        private static string nativeDirectory;

        public static void Validate(StandaloneConversionRequest request)
        {
            if (request == null) throw new ArgumentNullException(nameof(request));
            if (!Directory.Exists(request.Source)) throw new ArgumentException($"Project folder does not exist: {request.Source}");
            if (Directory.Exists(request.Output) || Path.EndsInDirectorySeparator(request.Output))
                throw new ArgumentException("Output must be a file, not a directory.");
            if (File.Exists(request.Output) && !request.Overwrite)
                throw new ArgumentException("Output already exists. Enable replacement to overwrite it after successful conversion.");
            foreach (string file in new[] { "Quill.json", "Quill.qbin" })
            {
                string path = Path.Combine(request.Source, file);
                if (SamePath(request.Output, path) || SamePath(request.Report, path))
                    throw new ArgumentException($"Output/report must not replace {file}.");
            }
            if (SamePath(request.Report, request.Output)) throw new ArgumentException("Report and IMM output must use different files.");
            if (request.Report != null && Directory.Exists(request.Report)) throw new ArgumentException("Report must be a file, not a directory.");
        }

        public static StandaloneConversionOutcome Run(StandaloneConversionRequest request,
            CancellationToken cancellationToken = default, IProgress<ImmAuthoringProgress> progress = null)
        {
            if (request == null) throw new ArgumentNullException(nameof(request));
            var outcome = new StandaloneConversionOutcome(request);
            string stage = null;
            bool entered = false;
            try
            {
                Validate(request);
                cancellationToken.ThrowIfCancellationRequested();
                if (Interlocked.CompareExchange(ref running, 1, 0) != 0)
                    throw new InvalidOperationException("Another conversion is already running in this process.");
                entered = true;
                if (!OperatingSystem.IsWindows() || RuntimeInformation.ProcessArchitecture != Architecture.X64)
                    throw new PlatformNotSupportedException("This standalone release requires Windows x64.");
                if (request.Report != null) Directory.CreateDirectory(Path.GetDirectoryName(request.Report));
                ConfigureNativeLibraries(request.NativeDirectory);
                string directory = Path.GetDirectoryName(request.Output);
                Directory.CreateDirectory(directory);
                stage = Path.Combine(directory, $".imm-convert-{Guid.NewGuid():N}.imm");
                var operation = new ImmAuthoringOperationOptions(cancellationToken, progress,
                    new ImmAuthoringLimits(maxInputBytes: request.MaxInputBytes));
                outcome.Conversion = QuillToImmConverter.ConvertProject(request.Source, stage, request.AllowOmissions, operation);
                bool cancelled = cancellationToken.IsCancellationRequested ||
                    outcome.Conversion.Export?.ErrorCode == ImmAuthoringErrorCode.Cancelled ||
                    outcome.Conversion.Issues.Any(issue => issue.Code == "Cancelled");
                if (outcome.Conversion.Succeeded && !cancelled)
                {
                    PublishOutput(stage, request.Output, request.Overwrite);
                    outcome.OutputPublished = true;
                }
                outcome.ExitCode = cancelled ? 130 : outcome.OutputPublished ? 0 : 1;
            }
            catch (OperationCanceledException) { outcome.ExitCode = 130; }
            catch (ArgumentException exception) { outcome.ExitCode = 2; outcome.Error = exception.Message; }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException ||
                exception is DllNotFoundException || exception is BadImageFormatException ||
                exception is EntryPointNotFoundException || exception is InvalidOperationException || exception is NotSupportedException)
            { outcome.ExitCode = 3; outcome.Error = exception.Message; }
            finally
            {
                try { if (stage != null && File.Exists(stage)) File.Delete(stage); }
                catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
                { outcome.CleanupError = $"Could not remove staging file {stage}: {exception.Message}"; }
                if (entered) Interlocked.Exchange(ref running, 0);
            }
            outcome.FinishedUtc = DateTimeOffset.UtcNow;
            if (request.Report != null && outcome.ExitCode != 2)
            {
                try { WriteReport(outcome); }
                catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
                { outcome.ExitCode = 3; outcome.ReportError = exception.Message; }
            }
            return outcome;
        }

        internal static void PublishOutput(string stage, string output, bool overwrite) => File.Move(stage, output, overwrite);

        private static bool SamePath(string a, string b) => a != null && string.Equals(a, b, StringComparison.OrdinalIgnoreCase);

        private static void ConfigureNativeLibraries(string directory)
        {
            string[] required = { "ImmUnityPlugin.dll", "ImmStrokeReader.dll", "Audio360.dll", "opusenc.dll", "vorbisenc.dll",
                "zlib1.dll", "jpeg62.dll", "libpng16.dll", "ogg.dll", "vorbis.dll", "opus.dll" };
            var missing = required.Where(name => !File.Exists(Path.Combine(directory, name))).ToArray();
            if (missing.Length != 0)
                throw new DllNotFoundException($"Missing native dependencies in {directory}: {string.Join(", ", missing)}. Use the complete application folder.");
            if (nativeDirectory != null)
            {
                if (!SamePath(nativeDirectory, directory)) throw new InvalidOperationException("Native libraries are already configured from another directory. Restart before changing it.");
                return;
            }
            NativeLibrary.SetDllImportResolver(typeof(QuillToImmConverter).Assembly, (name, assembly, searchPath) =>
                name == "ImmUnityPlugin" || name == "ImmStrokeReader"
                    ? NativeLibrary.Load(Path.Combine(directory, $"{name}.dll")) : IntPtr.Zero);
            nativeDirectory = directory;
        }

        private static void WriteReport(StandaloneConversionOutcome outcome)
        {
            var request = outcome.Request;
            var report = new
            {
                Schema = "imm-quill-conversion-report-v1", request.Source, request.Output, request.ToolVersion,
                Runtime = RuntimeDetails(), outcome.StartedUtc, outcome.FinishedUtc, outcome.ExitCode,
                Succeeded = outcome.OutputPublished, outcome.OutputPublished, Partial = outcome.IsPartial,
                outcome.BytesWritten, outcome.Error, outcome.CleanupError,
                Issues = outcome.Conversion?.Issues.Select(issue => new
                { Severity = issue.Severity.ToString(), issue.Code, issue.SourcePath, issue.Message }).ToArray()
            };
            Directory.CreateDirectory(Path.GetDirectoryName(request.Report));
            string temporary = Path.Combine(Path.GetDirectoryName(request.Report), $".{Path.GetFileName(request.Report)}.{Guid.NewGuid():N}.tmp");
            try
            {
                File.WriteAllText(temporary, JsonSerializer.Serialize(report, new JsonSerializerOptions
                { WriteIndented = true, PropertyNamingPolicy = JsonNamingPolicy.CamelCase }));
                File.Move(temporary, request.Report, true);
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }

        private static object RuntimeDetails()
        {
            var modules = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            string moduleError = null;
            try
            {
                using Process process = Process.GetCurrentProcess();
                foreach (ProcessModule module in process.Modules)
                {
                    string name = module.ModuleName.ToLowerInvariant();
                    if (name == "coreclr.dll" || name == "vcruntime140.dll" || name == "immunityplugin.dll" || name == "immstrokereader.dll")
                        modules[name] = module.FileName;
                }
            }
            catch (Exception exception) when (exception is Win32Exception || exception is NotSupportedException || exception is InvalidOperationException)
            { moduleError = exception.Message; }
            return new { Framework = RuntimeInformation.FrameworkDescription,
                Directory = RuntimeEnvironment.GetRuntimeDirectory(), Modules = modules, ModuleInspectionError = moduleError };
        }
    }
}
