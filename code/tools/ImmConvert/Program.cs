using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Threading;
using ImmPlayer.Authoring;
using ImmPlayer.Conversion;

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
        StandaloneConversionRequest request;
        try
        {
            Options options = Parse(args);
            if (options.Help) { Console.WriteLine(Usage); return 0; }
            request = new StandaloneConversionRequest(options.Source, options.Output,
                options.AllowOmissions, options.Overwrite, options.Report, options.NativeDirectory,
                options.MaxInputBytes, typeof(Program).Assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion);
            StandaloneConversion.Validate(request);
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
        try
        {
            var outcome = StandaloneConversion.Run(request, cancellation.Token, new ConsoleProgress());
            foreach (var issue in outcome.Conversion?.Issues ?? Array.Empty<QuillToImmIssue>())
                Console.Error.WriteLine($"{issue.Severity} {issue.Code} [{issue.SourcePath}]: {issue.Message}");
            if (outcome.OutputPublished)
                Console.WriteLine($"{(outcome.IsPartial ? "Converted with omissions" : "Converted")}: {request.Output} ({outcome.BytesWritten} bytes)");
            else if (outcome.ExitCode == 130) Console.Error.WriteLine("Conversion cancelled.");
            else if (outcome.Error != null) Console.Error.WriteLine($"{(outcome.ExitCode == 2 ? "Error" : "Runtime error")}: {outcome.Error}");
            else Console.Error.WriteLine("Conversion failed; output was not replaced.");
            if (outcome.ReportError != null) Console.Error.WriteLine($"Could not write report: {outcome.ReportError}");
            if (outcome.CleanupError != null) Console.Error.WriteLine(outcome.CleanupError);
            return outcome.ExitCode;
        }
        finally { Console.CancelKeyPress -= cancel; }
    }

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

}
