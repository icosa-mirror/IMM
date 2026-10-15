using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using ImmPlayer;
namespace UnityEngine
{
    public enum RuntimeInitializeLoadType { BeforeSceneLoad }
    public sealed class RuntimeInitializeOnLoadMethodAttribute : Attribute
    { public RuntimeInitializeOnLoadMethodAttribute(RuntimeInitializeLoadType _) {} }
    public static class Application
    {
        public static string temporaryCachePath;
        public static event Action onBeforeRender;
        public static void Frame() => onBeforeRender?.Invoke();
    }
    public static class Debug
    {
        public static readonly List<string> Warnings = new List<string>();
        public static readonly List<string> Errors = new List<string>();
        public static void LogWarning(object message) => Warnings.Add(message.ToString());
        public static void LogError(object message) => Errors.Add(message.ToString());
    }
}
namespace UnityEditor
{
    public sealed class InitializeOnLoadMethodAttribute : Attribute {}
    public static class EditorApplication
    { public static event Action update; public static void Tick() => update?.Invoke(); }
}
internal static class NativeLoggingPolicySmoke
{
    private static readonly Queue<Tuple<int,string>> messages = new Queue<Tuple<int,string>>();
    private static int Read(byte[] output, int capacity, out int severity)
    {
        severity = 0;
        if (messages.Count == 0) return 0;
        var message = messages.Peek();
        byte[] utf8 = Encoding.UTF8.GetBytes(message.Item2);
        if (capacity < utf8.Length + 1) return -(utf8.Length + 1);
        Array.Copy(utf8, output, utf8.Length); output[utf8.Length] = 0;
        severity = message.Item1; messages.Dequeue(); return utf8.Length + 1;
    }
    private static void Require(bool condition, string message)
    { if (!condition) throw new Exception(message); }
    private static int Main(string[] args)
    {
        UnityEngine.Application.temporaryCachePath = args[0];
        typeof(ImmNativeLogging).GetMethod("Initialize", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null,null);
        string root = Path.Combine(args[0], "IMM");
        Require(ImmNativeLogging.ResolveLogPath("default.log") == null && !Directory.Exists(root), "default policy creates neither files nor directory");
        ImmNativeLogging.EnableFileLogging = true;
        Require(ImmNativeLogging.ResolveLogPath("enabled.log") == Path.Combine(root,"enabled.log") && Directory.Exists(root), "opt-in uses IMM cache directory");
        Require(ImmNativeLogging.ResolveLogPath("ignored.log", "") == null, "empty explicit path disables file");
        ImmNativeLogging.EnableFileLogging = false;
        Require(ImmNativeLogging.ResolveLogPath("ignored.log", "relative.log") == Path.Combine(root,"relative.log"), "explicit relative name stays in cache");
        string blocker = Path.Combine(args[0],"not-a-directory"); File.WriteAllText(blocker,"x");
        Require(ImmNativeLogging.ResolveLogPath("ignored.log",Path.Combine(blocker,"failure.log")) == null, "file preparation failure is nonfatal");
        Task.Run(() => ImmNativeLogging.Register("test",Read)).Wait();
        ImmNativeLogging.Register("test",Read);
        messages.Enqueue(Tuple.Create(2,"Unicode \u8b66\u544a 100% complete"));
        messages.Enqueue(Tuple.Create(1,"native error"));
        Task.Run(() => ImmNativeLogging.Flush()).Wait();
        Require(messages.Count == 2, "worker thread cannot call Unity logger");
        UnityEditor.EditorApplication.Tick();
        Require(messages.Count == 0 && UnityEngine.Debug.Errors.Count == 1 &&
            UnityEngine.Debug.Warnings.Exists(x=>x.Contains("\u8b66\u544a 100% complete")), "main-thread severity/UTF-8 forwarding");
        Require(UnityEngine.Debug.Warnings.Exists(x=>x.Contains("continuing without it")), "file failure reported through Unity");
        UnityEngine.Application.Frame();
        Require(UnityEngine.Debug.Errors.Count == 1, "duplicate registration does not duplicate output");
        Console.WriteLine("[IMM_NATIVE_LOG_TEST] managed default-off/cache paths, nonfatal setup and main-thread forwarding passed");
        return 0;
    }
}
