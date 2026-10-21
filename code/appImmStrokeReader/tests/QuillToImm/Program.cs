using System;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using ImmPlayer.Authoring;

if (args.Length == 3 && args[0] == "--reader-only-pcm")
{
    PcmDecodeChecks.ReaderOnly(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]));
    return 0;
}
if (args.Length == 4 && (args[0] == "--compare-paint" || args[0] == "--compare-animated-paint" || args[0] == "--compare-project"))
{
    string library = Path.GetFullPath(args[3]);
    NativeLibrary.SetDllImportResolver(typeof(QuillToImmConverter).Assembly, (name, assembly, searchPath) =>
        name == "ImmStrokeReader" ? NativeLibrary.Load(library) : IntPtr.Zero);
    ConverterChecks.ComparePaint(args[1], args[2], args[0] != "--compare-paint", args[0] == "--compare-project");
    return 0;
}
if (args.Length == 4 && args[0] == "--self-test")
{
    string exporterLibrary = Path.GetFullPath(args[1]);
    string readerLibrary = Path.GetFullPath(args[2]);
    NativeLibrary.SetDllImportResolver(typeof(QuillToImmConverter).Assembly, (name, assembly, searchPath) =>
        name == "ImmUnityPlugin" ? NativeLibrary.Load(exporterLibrary) : name == "ImmStrokeReader" ? NativeLibrary.Load(readerLibrary) : IntPtr.Zero);
    WindowsDependencyChecks.Run(exporterLibrary);
    PaintRepeatChecks.Run(args[3]);
    RootPropertyChecks.Run(args[3]);
    RootAnimationChecks.Run(args[3]);
    ViewpointConverterChecks.Run(args[3]);
    PreviewAssetChecks.Run(args[3]);
    PngMemoryBoundsChecks.Run();
    PictureAuthoringChecks.Run(args[3]);
    SoundAuthoringChecks.Run(args[3]);
    PcmDecodeChecks.LifetimeAndOffset(args[3]);
    PcmDecodeChecks.WavFormats(args[3]);
    PcmDecodeChecks.Standalone(readerLibrary, args[3]);
    PictureConverterChecks.Run(args[3]);
    SoundConverterChecks.Run(args[3]);
    ConverterChecks.Run(args[3]);
    HierarchyReadChecks.Run(args[3]);
    return 0;
}
if (args.Length < 4 || args.Length > 8)
    throw new ArgumentException("Expected project folder, output IMM, exporter DLL, reader DLL, and optional --animated, --with-viewpoints, --allow-omissions, --input-mib=N (legacy --paint-only).");
if (args.Skip(4).Any(x => x != "--paint-only" && x != "--animated" && x != "--with-viewpoints" && x != "--allow-omissions" && !x.StartsWith("--input-mib="))) throw new ArgumentException("Unknown option.");
string budget = args.Skip(4).SingleOrDefault(x => x.StartsWith("--input-mib="));
if (budget != null && !args.Skip(4).Contains("--with-viewpoints")) throw new ArgumentException("--input-mib requires the project profile (--with-viewpoints).");
var options = new ImmAuthoringOperationOptions(limits: budget == null ? null :
    new ImmAuthoringLimits(maxInputBytes: checked(long.Parse(budget.Substring("--input-mib=".Length)) * 1024 * 1024)));
string exporter = Path.GetFullPath(args[2]);
string reader = Path.GetFullPath(args[3]);
NativeLibrary.SetDllImportResolver(typeof(QuillToImmConverter).Assembly, (name, assembly, searchPath) =>
    name == "ImmUnityPlugin" ? NativeLibrary.Load(exporter) : name == "ImmStrokeReader" ? NativeLibrary.Load(reader) : IntPtr.Zero);
bool allowOmittedLayers = args.Skip(4).Contains("--paint-only") || args.Skip(4).Contains("--allow-omissions");
var result = args.Skip(4).Contains("--with-viewpoints")
    ? QuillToImmConverter.ConvertProject(args[0], args[1], allowOmittedLayers, options)
    : args.Skip(4).Contains("--animated")
    ? QuillToImmConverter.ConvertAnimatedPaintProject(args[0], args[1], allowOmittedLayers)
    : QuillToImmConverter.ConvertStaticPaintProject(args[0], args[1], allowOmittedLayers);
foreach (var issue in result.Issues)
    Console.WriteLine($"[IMM_QUILL_CONVERTER] {issue.Severity} {issue.Code} {issue.SourcePath}: {issue.Message}");
Console.WriteLine($"[IMM_QUILL_CONVERTER] success={result.Succeeded}, partial={result.IsPartial}, bytes={result.Export?.BytesWritten ?? 0}");
return result.Succeeded ? 0 : 1;
