using System;
using System.IO;
using System.Text.Json;
using System.Threading;
using ImmPlayer.Authoring;
using ImmPlayer.Conversion;

internal static class HostChecks
{
    private sealed class CallbackProgress : IProgress<ImmAuthoringProgress>
    {
        private readonly Action<ImmAuthoringProgress> callback;
        public CallbackProgress(Action<ImmAuthoringProgress> callback) { this.callback = callback; }
        public void Report(ImmAuthoringProgress value) => callback(value);
    }

    private static void Require(bool condition, string message)
    { if (!condition) throw new InvalidOperationException(message); }

    public static void Run(string directory, string source, string native)
    {
        string report = Path.Combine(directory, "host-report.json");
        File.WriteAllText(report, "{\"passed\":false}");
        StandaloneConversionRequest Request(string name, bool overwrite = false) => new StandaloneConversionRequest(
            source, Path.Combine(directory, $"{name}.imm"), overwrite: overwrite,
            report: Path.Combine(directory, $"{name}.json"), nativeDirectory: native);

        var before = StandaloneConversion.Run(Request("cancel-before"), new CancellationToken(true));
        Require(before.ExitCode == 130 && !before.OutputPublished && !File.Exists(before.Request.Output), "Pre-cancelled run published output");

        StandaloneConversionOutcome nested = null;
        var first = StandaloneConversion.Run(Request("first"), progress: new CallbackProgress(value =>
        {
            if (nested == null) nested = StandaloneConversion.Run(Request("nested"));
        }));
        Require(first.ExitCode == 0 && first.OutputPublished && first.BytesWritten > 0, "Initial shared-host conversion failed");
        Require(nested != null && nested.ExitCode == 3 && !nested.OutputPublished &&
            nested.Error.Contains("already running") && !File.Exists(nested.Request.Output), "Re-entrant native conversion was not rejected");

        var repeated = StandaloneConversion.Run(Request("repeated"));
        Require(repeated.ExitCode == 0 && repeated.OutputPublished, "Repeated native configuration/conversion failed");

        using var cancellation = new CancellationTokenSource();
        var cancelRequest = Request("cancel-during", overwrite: true);
        File.WriteAllText(cancelRequest.Output, "keep existing output");
        var during = StandaloneConversion.Run(cancelRequest, cancellation.Token,
            new CallbackProgress(value => cancellation.Cancel()));
        Require(during.ExitCode == 130 && !during.OutputPublished &&
            File.ReadAllText(cancelRequest.Output) == "keep existing output", "Mid-conversion cancellation replaced existing output");
        var recovered = StandaloneConversion.Run(Request("recovered"));
        Require(recovered.ExitCode == 0 && recovered.OutputPublished, "Host did not recover after cancellation");
        Require(Directory.GetFiles(directory, ".imm-convert-*.imm").Length == 0, "Host left private staging files behind");
        File.WriteAllText(report, JsonSerializer.Serialize(new { passed = true, cases = 6,
            scope = "real native host repeats, re-entrancy rejection, cancellation and recovery; not GUI acceptance" }));
        Console.WriteLine("[IMM_CONVERT_HOST] repeated conversions, rejected re-entrancy, cancellation and recovery passed");
    }
}
