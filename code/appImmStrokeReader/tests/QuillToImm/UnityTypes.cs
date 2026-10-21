// Data-only stand-ins for the .NET native-export harness. Production graph,
// converter, compiler and P/Invoke declarations are compiled unchanged.
namespace UnityEngine
{
    public enum RuntimeInitializeLoadType { BeforeSceneLoad }
    [System.AttributeUsage(System.AttributeTargets.Method)]
    public sealed class RuntimeInitializeOnLoadMethodAttribute : System.Attribute
    {
        public RuntimeInitializeOnLoadMethodAttribute(RuntimeInitializeLoadType type) { }
    }
    public static class Application
    {
        public static string temporaryCachePath => System.IO.Path.GetTempPath();
        public static event System.Action onBeforeRender { add { } remove { } }
    }
    public static class Debug
    {
        public static void Log(object value) => System.Console.WriteLine(value);
        public static void LogWarning(object value) => System.Console.WriteLine(value);
        public static void LogError(object value) => System.Console.WriteLine(value);
    }
    public struct Vector3
    {
        public float x, y, z;
        public Vector3(float x, float y, float z) { this.x = x; this.y = y; this.z = z; }
        public static Vector3 zero => new Vector3(0, 0, 0);
        public static Vector3 up => new Vector3(0, 1, 0);
        public float magnitude => (float)System.Math.Sqrt(x * x + y * y + z * z);
        public static Vector3 operator +(Vector3 a, Vector3 b) => new Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
        public static Vector3 operator -(Vector3 a, Vector3 b) => new Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
        public static Vector3 operator *(Vector3 a, float b) => new Vector3(a.x * b, a.y * b, a.z * b);
        public static bool operator ==(Vector3 a, Vector3 b) => (a - b).magnitude < 0.00001f;
        public static bool operator !=(Vector3 a, Vector3 b) => !(a == b);
        public override bool Equals(object value) => value is Vector3 other && x == other.x && y == other.y && z == other.z;
        public override int GetHashCode() => System.HashCode.Combine(x, y, z);
    }
    public struct Quaternion
    {
        public float x, y, z, w;
        public Quaternion(float x, float y, float z, float w) { this.x = x; this.y = y; this.z = z; this.w = w; }
        public static Quaternion identity => new Quaternion(0, 0, 0, 1);
        public static float Dot(Quaternion a, Quaternion b) => a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }
    public static class Mathf
    {
        public static float Abs(float value) => System.Math.Abs(value);
        public static float Sqrt(float value) => (float)System.Math.Sqrt(value);
        public static float Max(float a, float b) => System.Math.Max(a, b);
        public static float Clamp01(float value) => System.Math.Clamp(value, 0, 1);
        public static bool Approximately(float a, float b) => Abs(a - b) <= 0.000001f * Max(1, Max(Abs(a), Abs(b)));
    }
    public struct Color
    {
        public float r, g, b, a;
        public Color(float r, float g, float b, float a) { this.r = r; this.g = g; this.b = b; this.a = a; }
    }
    public struct Bounds { public Bounds(Vector3 center, Vector3 size) { } }
    public class Transform
    {
        public Vector3 position;
        public Quaternion rotation;
        public Vector3 lossyScale;
    }
}
