// The adapter only uses Unity logging. These stand-ins let its real source and
// native declarations run without an Editor. Vector/Bounds/Color convenience
// members in the native wrapper are not used by the adapter or this test.
namespace UnityEngine
{
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
        public static Vector3 operator +(Vector3 a, Vector3 b) => new Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
        public static Vector3 operator -(Vector3 a, Vector3 b) => new Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
        public static Vector3 operator *(Vector3 a, float b) => new Vector3(a.x * b, a.y * b, a.z * b);
    }

    public struct Bounds { public Bounds(Vector3 center, Vector3 size) {} }
    public struct Color { public Color(float r, float g, float b, float a) {} }
}
