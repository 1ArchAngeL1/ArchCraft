#pragma once
// ---------------------------------------------------------------------------
// Minimal vector maths. Hand-written on purpose: every operation a 3D engine
// needs fits in ~100 lines, and seeing them spelled out beats importing GLM
// while you are still learning what a dot product is *for*.
// ---------------------------------------------------------------------------
#include <cmath>

struct Vec2 {
    float x = 0.0f, y = 0.0f;
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    explicit constexpr Vec3(float s) : x(s), y(s), z(s) {}
};

inline constexpr Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline constexpr Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline constexpr Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline constexpr Vec3 operator*(float s, Vec3 a) { return a * s; }
inline constexpr Vec3 operator*(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline constexpr Vec3 operator/(Vec3 a, float s) { return {a.x / s, a.y / s, a.z / s}; }
inline constexpr Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3& operator+=(Vec3& a, Vec3 b) { a = a + b; return a; }
inline Vec3& operator-=(Vec3& a, Vec3 b) { a = a - b; return a; }
inline Vec3& operator*=(Vec3& a, float s) { a = a * s; return a; }

// The dot product: how much two directions agree. Used for lighting, for
// projecting a point onto a plane, for "is this behind me?" tests.
inline constexpr float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// The cross product: a vector perpendicular to both inputs. Used to build an
// orthonormal camera basis (right = forward x up).
inline constexpr Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }
inline float lengthSq(Vec3 v) { return dot(v, v); }

inline Vec3 normalize(Vec3 v) {
    const float len = length(v);
    return len > 1e-8f ? v / len : Vec3{0.0f, 0.0f, 0.0f};
}

inline Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
};

// Handy scalar helpers used all over the engine.
inline constexpr float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline constexpr float radians(float degrees) { return degrees * 3.14159265358979323846f / 180.0f; }

// Floor division / modulo that behave correctly for negative coordinates.
// Vital for voxel worlds: world x = -1 must map to chunk -1, local x = 15.
inline constexpr int floorDiv(int a, int b) { return (a >= 0) ? (a / b) : -(((-a) + b - 1) / b); }
inline constexpr int floorMod(int a, int b) { const int m = a % b; return (m < 0) ? m + b : m; }
inline int floorToInt(float v) { return static_cast<int>(std::floor(v)); }
