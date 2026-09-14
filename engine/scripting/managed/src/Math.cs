using System.Runtime.InteropServices;

namespace Chained
{

// ── Math ─────────────────────────────────────────────────────────────────────

/// <summary>2D vector shared with native code.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct Vector2
{
    public float X, Y;

    public Vector2(float x, float y) { X = x; Y = y; }

    public static readonly Vector2 Zero = new Vector2(0, 0);
    public static readonly Vector2 One  = new Vector2(1, 1);

    public static Vector2 operator +(Vector2 left, Vector2 right) => new Vector2(left.X + right.X, left.Y + right.Y);
    public static Vector2 operator -(Vector2 left, Vector2 right) => new Vector2(left.X - right.X, left.Y - right.Y);
    public static Vector2 operator -(Vector2 vector)              => new Vector2(-vector.X, -vector.Y);
    public static Vector2 operator *(Vector2 vector, float scalar) => new Vector2(vector.X * scalar, vector.Y * scalar);
    public static Vector2 operator *(float scalar, Vector2 vector) => new Vector2(scalar * vector.X, scalar * vector.Y);
    public static Vector2 operator /(Vector2 vector, float scalar) => new Vector2(vector.X / scalar, vector.Y / scalar);

    public float LengthSquared() => X * X + Y * Y;
    public float Length()        => (float)System.Math.Sqrt(LengthSquared());

    public override string ToString() => $"({X:F2}, {Y:F2})";
}

/// <summary>3D vector shared with native code.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct Vector3
{
    public float X, Y, Z;

    /// <summary>Creates a vector from X, Y, and Z.</summary>
    public Vector3(float x, float y, float z) { X = x; Y = y; Z = z; }

    public static readonly Vector3 Zero  = new Vector3(0, 0, 0);
    public static readonly Vector3 One   = new Vector3(1, 1, 1);
    public static readonly Vector3 Up    = new Vector3(0, 1, 0);

    public static Vector3 operator +(Vector3 left, Vector3 right) => new Vector3(left.X + right.X, left.Y + right.Y, left.Z + right.Z);
    public static Vector3 operator -(Vector3 left, Vector3 right) => new Vector3(left.X - right.X, left.Y - right.Y, left.Z - right.Z);
    public static Vector3 operator -(Vector3 vector)              => new Vector3(-vector.X, -vector.Y, -vector.Z);
    public static Vector3 operator *(Vector3 vector, float scalar) => new Vector3(vector.X * scalar, vector.Y * scalar, vector.Z * scalar);
    public static Vector3 operator *(float scalar, Vector3 vector) => new Vector3(scalar * vector.X, scalar * vector.Y, scalar * vector.Z);
    public static Vector3 operator /(Vector3 vector, float scalar) => new Vector3(vector.X / scalar, vector.Y / scalar, vector.Z / scalar);

    public float LengthSquared() => X * X + Y * Y + Z * Z;
    public float Length()        => (float)System.Math.Sqrt(LengthSquared());

    /// <summary>Returns a normalized copy, or zero for tiny inputs.</summary>
    public static Vector3 Normalize(Vector3 vector)
    {
        float length = vector.Length();
        return length > 0.00001f ? vector / length : Zero;
    }

    /// <summary>Dot product.</summary>
    public static float Dot(Vector3 left, Vector3 right) => left.X * right.X + left.Y * right.Y + left.Z * right.Z;

    /// <summary>Cross product.</summary>
    public static Vector3 Cross(Vector3 left, Vector3 right) => new Vector3(
        left.Y * right.Z - left.Z * right.Y,
        left.Z * right.X - left.X * right.Z,
        left.X * right.Y - left.Y * right.X);

    /// <summary>Linear interpolation.</summary>
    public static float Lerp(float start, float end, float factor) => start + (end - start) * factor;

    public override string ToString() => $"({X:F2}, {Y:F2}, {Z:F2})";
}

/// <summary>4D vector shared with native code (useful for colors).</summary>
[StructLayout(LayoutKind.Sequential)]
public struct Vector4
{
    public float X, Y, Z, W;

    public Vector4(float x, float y, float z, float w) { X = x; Y = y; Z = z; W = w; }
    public Vector4(float value) { X = Y = Z = W = value; }

    public static readonly Vector4 White = new Vector4(1, 1, 1, 1);
    public static readonly Vector4 Black = new Vector4(0, 0, 0, 1);
}

/// <summary>Scalar math helpers.</summary>
public static class Mathf
{
    public const float PI      = (float)System.Math.PI;
    public const float Deg2Rad = PI / 180.0f;
    public const float Rad2Deg = 180.0f / PI;

    public static float Clamp(float value, float min, float max)
        => value < min ? min : (value > max ? max : value);
    public static float Abs(float value) => value < 0 ? -value : value;
    public static float Sin(float angle) => (float)System.Math.Sin(angle);
    public static float Cos(float angle) => (float)System.Math.Cos(angle);
    public static float Atan2(float y, float x) => (float)System.Math.Atan2(y, x);
    public static float Sqrt(float value) => (float)System.Math.Sqrt(value);
    public static float Lerp(float start, float end, float factor) => start + (end - start) * factor;
}

// ── Key / MouseButton enums ───────────────────────────────────────────────────

/// <summary>Keyboard keys exposed to managed scripts.</summary>
public enum Key : int
{
    Null = 0,
    A = 1,  B = 2,  C = 3,
    D = 4,  E = 5,  F = 6,
    G = 7,  H = 8,  I = 9,
    J = 10, K = 11, L = 12,
    M = 13, N = 14, O = 15,
    P = 16, Q = 17, R = 18,
    S = 19, T = 20, U = 21,
    V = 22, W = 23, X = 24,
    Y = 25, Z = 26,

    Space     = 27,
    Escape    = 28,
    Enter     = 29,
    Tab       = 30,
    Backspace = 31,
    Insert    = 32,
    Delete    = 33,
    Right     = 34,
    Left      = 35,
    Down      = 36,
    Up        = 37,
    PageUp    = 38,
    PageDown  = 39,
    Home      = 40,
    End       = 41,
    CapsLock  = 42,
    ScrollLock = 43,
    NumLock   = 44,
    PrintScreen = 45,
    Pause     = 46,

    F1  = 47, F2  = 48, F3  = 49,
    F4  = 50, F5  = 51, F6  = 52,
    F7  = 53, F8  = 54, F9  = 55,
    F10 = 56, F11 = 57, F12 = 58,

    LeftShift   = 59,
    LeftControl = 60,
    LeftAlt     = 61,
    RightShift  = 62,
    RightControl = 63,
    RightAlt    = 64,

    D0 = 65, D1 = 66, D2 = 67,
    D3 = 68, D4 = 69, D5 = 70,
    D6 = 71, D7 = 72, D8 = 73,
    D9 = 74,
}

/// <summary>Mouse buttons exposed to managed scripts.</summary>
public enum MouseButton : int
{
    Left   = 0,
    Right  = 1,
    Middle = 2
}

} // namespace Chained
