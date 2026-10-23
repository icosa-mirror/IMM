using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Threading;
using ImmPlayer.Authoring;

internal static class Program
{
    private const string Usage = """
        Usage: imm-convert <Quill project folder> <output.imm> [options]

        Convert the supported contents of a Quill project without Unity.

          --allow-omissions          Allow partial output and report omitted content.
          --overwrite                Replace an existing output after successful conversion.
          --report <file.json>       Write a machine-readable conversion report.
          --input-mib <number>       Set the input-size limit (default: 256 MiB).
          --native-directory <dir>   Load native DLLs from this directory.
          --help                     Show this help.

        Native DLLs default to the application's directory. Paths with spaces must
        be quoted. Use -- to end option parsing. Ctrl+C requests cancellation.
        Exit codes: 0 converted (including explicitly allowed partial output),
        1 conversion rejected, 2 invalid arguments, 3 runtime/report error,
        130 cancelled. A report is not written for invalid arguments.
        """;

    private sealed class Options
    {
        public string Source;
        public string Output;
        public string Report;
        public string NativeDirectory = AppContext.BaseDirectory;
        public long MaxInputBytes = ImmAuthoringLimits.Default.MaxInputBytes;
        public bool AllowOmissions;
        public bool Overwrite;
        public bool Help;
    }

    private sealed class UsageException : Exception
    {
        public UsageException(string message) : base(message) { }
    }

    private sealed class ConsoleProgress : IProgress<ImmAuthoringProgress>
    {
        private ImmAuthoringProgressStage? lastStage;
        public void Report(ImmAuthoringProgress value)
        {
            if (lastStage == value.Stage) return;
            lastStage = value.Stage;
            Console.Error.WriteLine($"{value.Stage}: {value.Message}");
        }
    }

    private static int Main(string[] args)
    {
        Options options;
        try
        {
            options = Parse(args);
            if (options.Help) { Console.WriteLine(Usage); return 0; }
            ValidatePaths(options);
        }
        catch (Exception exception) when (exception is UsageException || exception is ArgumentException ||
            exception is NotSupportedException || exception is PathTooLongException)
        {
            Console.Error.WriteLine($"Error: {exception.Message}\nUse --help for usage.");
            return 2;
        }

        using var cancellation = new CancellationTokenSource();
        ConsoleCancelEventHandler cancel = (sender, signal) => { signal.Cancel = true; cancellation.Cancel(); };
        Console.CancelKeyPress += cancel;
        DateTimeOffset started = DateTimeOffset.UtcNow;
        QuillToImmConversionResult result = null;
        string stagedOutput = null;
        bool published = false;
        try
        {
            if (!OperatingSystem.IsWindows() || RuntimeInformation.ProcessArchitecture != Architecture.X64)
                throw new PlatformNotSupportedException("This standalone release requires Windows x64.");
            if (options.Report != null) Directory.CreateDirectory(Path.GetDirectoryName(options.Report));
            ConfigureNativeLibraries(options.NativeDirectory);
            string outputDirectory = Path.GetDirectoryName(options.Output);
            Directory.CreateDirectory(outputDirectory);
            stagedOutput = Path.Combine(outputDirectory, $".imm-convert-{Guid.NewGuid():N}.imm");
            var operation = new ImmAuthoringOperationOptions(cancellation.Token, new ConsoleProgress(),
                new ImmAuthoringLimits(maxInputBytes: options.MaxInputBytes));
            result = QuillToImmConverter.ConvertProject(options.Source, stagedOutput, options.AllowOmissions, operation);
            bool cancelled = result.Export?.ErrorCode == ImmAuthoringErrorCode.Cancelled ||
                result.Issues.Any(issue => issue.Code == "Cancelled") || cancellation.IsCancellationRequested;
            if (result.Succeeded && !cancelled)
            {
                PublishOutput(stagedOutput, options.Output, options.Overwrite);
                published = true;
            }
            int exitCode = cancelled ? 130 : result.Succeeded ? 0 : 1;
            foreach (var issue in result.Issues)
                Console.Error.WriteLine($"{issue.Severity} {issue.Code} [{issue.SourcePath}]: {issue.Message}");
            if (published)
                Console.WriteLine($"{(result.IsPartial ? "Converted with omissions" : "Converted")}: {options.Output} ({result.Export.BytesWritten} bytes)");
            else
                Console.Error.WriteLine(cancelled ? "Conversion cancelled." : "Conversion failed; output was not replaced.");
            WriteReport(options, started, exitCode, result, published, null);
            return exitCode;
        }
        catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException ||
            exception is DllNotFoundException || exception is BadImageFormatException ||
            exception is EntryPointNotFoundException || exception is InvalidOperationException ||
            exception is NotSupportedException)
        {
            Console.Error.WriteLine($"Runtime error: {exception.Message}");
            try { WriteReport(options, started, 3, result, published, exception.Message); }
            catch (Exception reportError) when (reportError is IOException || reportError is UnauthorizedAccessException)
            { Console.Error.WriteLine($"Could not write report: {reportError.Message}"); }
            return 3;
        }
        finally
        {
            Console.CancelKeyPress -= cancel;
            try { if (stagedOutput != null && File.Exists(stagedOutput)) File.Delete(stagedOutput); }
            catch (Exception cleanupError) when (cleanupError is IOException || cleanupError is UnauthorizedAccessException)
            { Console.Error.WriteLine($"Could not remove staging file {stagedOutput}: {cleanupError.Message}"); }
        }
    }

    // The destination may appear after argument validation. No-overwrite must
    // be enforced by the final filesystem operation, not another existence check.
    internal static void PublishOutput(string stagedOutput, string requestedOutput, bool overwrite) =>
        File.Move(stagedOutput, requestedOutput, overwrite);

    private static Options Parse(string[] args)
    {
        var options = new Options();
        var positional = new List<string>();
        var seen = new HashSet<string>(StringComparer.Ordinal);
        bool endOptions = false;
        for (int i = 0; i < args.Length; i++)
        {
            string arg = args[i];
            if (!endOptions && arg == "--") { endOptions = true; continue; }
            if (endOptions || !arg.StartsWith("-", StringComparison.Ordinal)) { positional.Add(arg); continue; }
            if (arg == "--help" || arg == "-h") { options.Help = true; continue; }
            if (!seen.Add(arg)) throw new UsageException($"Option repeated: {arg}");
            if (arg == "--allow-omissions") { options.AllowOmissions = true; continue; }
            if (arg == "--overwrite") { options.Overwrite = true; continue; }
            if (arg != "--report" && arg != "--input-mib" && arg != "--native-directory")
                throw new UsageException($"Unknown option: {arg}");
            if (++i >= args.Length || args[i].StartsWith("--", StringComparison.Ordinal))
                throw new UsageException($"Missing value for {arg}.");
            string value = args[i];
            if (arg == "--report") options.Report = Path.GetFullPath(value);
            else if (arg == "--native-directory") options.NativeDirectory = Path.GetFullPath(value);
            else
            {
                if (!long.TryParse(value, NumberStyles.None, CultureInfo.InvariantCulture, out long mib) ||
                    mib <= 0 || mib > long.MaxValue / ImmAuthoringLimits.MiB)
                    throw new UsageException("--input-mib requires a positive whole number within the supported range.");
                options.MaxInputBytes = mib * ImmAuthoringLimits.MiB;
            }
        }
        if (options.Help) return options;
        if (positional.Count != 2) throw new UsageException("Specify a Quill project folder and an output IMM file.");
        options.Source = Path.GetFullPath(positional[0]);
        options.Output = Path.GetFullPath(positional[1]);
        return options;
    }

    private static void ValidatePaths(Options options)
    {
        if (!Directory.Exists(options.Source)) throw new UsageException($"Project folder does not exist: {options.Source}");
        if (Directory.Exists(options.Output)) throw new UsageException("Output must be a file, not a directory.");
        if (File.Exists(options.Output) && !options.Overwrite)
            throw new UsageException("Output already exists. Use --overwrite to replace it after successful conversion.");
        foreach (string sourceFile in new[] { "Quill.json", "Quill.qbin" })
        {
            string path = Path.Combine(options.Source, sourceFile);
            if (SamePath(options.Output, path) || SamePath(options.Report, path))
                throw new UsageException($"Output/report must not replace {sourceFile}.");
        }
        if (SamePath(options.Report, options.Output)) throw new UsageException("Report and IMM output must use different files.");
        if (options.Report != null && Directory.Exists(options.Report)) throw new UsageException("Report must be a file, not a directory.");
    }

    private static bool SamePath(string a, string b) => a != null &&
        string.Equals(a, b, StringComparison.OrdinalIgnoreCase);

    private static void ConfigureNativeLibraries(string directory)
    {
        string[] required = { "ImmUnityPlugin.dll", "ImmStrokeReader.dll", "Audio360.dll", "opusenc.dll", "vorbisenc.dll",
            "zlib1.dll", "jpeg62.dll", "libpng16.dll", "ogg.dll", "vorbis.dll", "opus.dll" };
        var missing = required.Where(name => !File.Exists(Path.Combine(directory, name))).ToArray();
        if (missing.Length != 0)
            throw new DllNotFoundException($"Missing native dependencies in {directory}: {string.Join(", ", missing)}. Use the complete distribution or --native-directory.");
        NativeLibrary.SetDllImportResolver(typeof(QuillToImmConverter).Assembly, (name, assembly, searchPath) =>
            name == "ImmUnityPlugin" || name == "ImmStrokeReader"
                ? NativeLibrary.Load(Path.Combine(directory, $"{name}.dll")) : IntPtr.Zero);
    }

    private static void WriteReport(Options options, DateTimeOffset started, int exitCode,
        QuillToImmConversionResult result, bool published, string error)
    {
        if (options.Report == null) return;
        var report = new
        {
            Schema = "imm-quill-conversion-report-v1", Source = options.Source, Output = options.Output,
            ToolVersion = typeof(Program).Assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion,
            StartedUtc = started, FinishedUtc = DateTimeOffset.UtcNow, ExitCode = exitCode,
            Succeeded = published, OutputPublished = published, Partial = result?.IsPartial ?? false,
            BytesWritten = published ? result.Export.BytesWritten : 0, Error = error,
            Issues = result?.Issues.Select(issue => new
            { Severity = issue.Severity.ToString(), issue.Code, issue.SourcePath, issue.Message }).ToArray()
        };
        string temporary = Path.Combine(Path.GetDirectoryName(options.Report), $".{Path.GetFileName(options.Report)}.{Guid.NewGuid():N}.tmp");
        try
        {
            File.WriteAllText(temporary, JsonSerializer.Serialize(report, new JsonSerializerOptions
            { WriteIndented = true, PropertyNamingPolicy = JsonNamingPolicy.CamelCase }));
            File.Move(temporary, options.Report, true);
        }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }
}
