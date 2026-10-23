using System;
using System.IO;
using System.Text.Json;
using ImmPlayer.Conversion;

internal static class PublicationChecks
{
    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }

    private static void Main(string[] args)
    {
        string directory = Path.GetFullPath(args[0]);
        Directory.CreateDirectory(directory);
        string report = Path.Combine(directory, "publication-report.json");
        File.WriteAllText(report, "{\"passed\":false}");
        string stage = Path.Combine(directory, "staged.imm");
        string destination = Path.Combine(directory, "requested.imm");
        File.WriteAllText(stage, "new conversion");
        // Simulate a destination created by another process after conversion.
        File.WriteAllText(destination, "other process output");
        bool rejected = false;
        try { StandaloneConversion.PublishOutput(stage, destination, false); }
        catch (IOException) { rejected = true; }
        Require(rejected, "Late-created output must reject no-overwrite publication");
        Require(File.ReadAllText(destination) == "other process output", "Late output was overwritten");
        Require(File.ReadAllText(stage) == "new conversion", "Rejected staging content was lost");
        StandaloneConversion.PublishOutput(stage, destination, true);
        Require(File.ReadAllText(destination) == "new conversion" && !File.Exists(stage), "Explicit overwrite failed");
        string fresh = Path.Combine(directory, $"fresh-{Guid.NewGuid():N}.imm");
        File.WriteAllText(stage, "fresh conversion");
        StandaloneConversion.PublishOutput(stage, fresh, false);
        Require(File.ReadAllText(fresh) == "fresh conversion" && !File.Exists(stage), "Fresh publication failed");
        File.WriteAllText(report, JsonSerializer.Serialize(new { passed = true, cases = 3 }));
        Console.WriteLine("[IMM_CONVERT_PUBLICATION] late destination preserved; explicit overwrite and fresh publish passed");
        if (args.Length == 3) HostChecks.Run(directory, args[1], args[2]);
    }
}
