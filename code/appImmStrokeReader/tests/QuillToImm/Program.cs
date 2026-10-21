using System.Runtime.InteropServices;
using ImmPlayer.Authoring;

if (args.Length == 4 && args[0] == "--compare-paint")
{
    string library = Path.GetFullPath(args[3]);
    NativeLibrary.SetDllImportResolver(typeof(QuillToImmConverter).Assembly, (name, assembly, searchPath) =>
        name == "ImmStrokeReader" ? NativeLibrary.Load(library) : IntPtr.Zero);
    ConverterChecks.ComparePaint(args[1], args[2]);
    return 0;
}
if (args.Length == 4 && args[0] == "--self-test")
{
    string exporterLibrary = Path.GetFullPath(args[1]);
    string readerLibrary = Path.GetFullPath(args[2]);
    NativeLibrary.SetDllImportResolver(typeof(QuillToImmConverter).Assembly, (name, assembly, searchPath) =>
        name == "ImmUnityPlugin" ? NativeLibrary.Load(exporterLibrary) : name == "ImmStrokeReader" ? NativeLibrary.Load(readerLibrary) : IntPtr.Zero);
    PaintRepeatChecks.Run(args[3]);
    ConverterChecks.Run(args[3]);
    return 0;
}
if (args.Length != 4 && args.Length != 5)
    throw new ArgumentException("Expected project folder, output IMM, exporter DLL, reader DLL, and optional --paint-only.");
if (args.Length == 5 && args[4] != "--paint-only") throw new ArgumentException("Unknown option.");
string exporter = Path.GetFullPath(args[2]);
string reader = Path.GetFullPath(args[3]);
NativeLibrary.SetDllImportResolver(typeof(QuillToImmConverter).Assembly, (name, assembly, searchPath) =>
    name == "ImmUnityPlugin" ? NativeLibrary.Load(exporter) : name == "ImmStrokeReader" ? NativeLibrary.Load(reader) : IntPtr.Zero);
var result = QuillToImmConverter.ConvertStaticPaintProject(args[0], args[1], args.Length == 5);
foreach (var issue in result.Issues)
    Console.WriteLine($"[IMM_QUILL_CONVERTER] {issue.Severity} {issue.Code} {issue.SourcePath}: {issue.Message}");
Console.WriteLine($"[IMM_QUILL_CONVERTER] success={result.Succeeded}, partial={result.IsPartial}, bytes={result.Export?.BytesWritten ?? 0}");
return result.Succeeded ? 0 : 1;
