#pragma once
// ---------------------------------------------------------------------------
// 4x4 matrices in COLUMN-MAJOR order, which is what OpenGL expects, so we can
// hand the raw float array straight to glUniformMatrix4fv without transposing.
//
//   element (row r, column c)  ->  m[c * 4 + r]
//
// A 4x4 matrix can express translation, rotation, scale AND the perspective
// divide all at once. That last one is the trick that makes 3D look 3D.
// ---------------------------------------------------------------------------
#include "math/Vec.h"

struct Mat4 {
    float m[16] = {1, 0, 0, 0,
                   0, 1, 0, 0,
                   0, 0, 1, 0,
                   0, 0, 0, 1};

    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }

    static Mat4 identity() { return Mat4{}; }

    static Mat4 translation(Vec3 t) {
        Mat4 r;
        r.at(0, 3) = t.x;
        r.at(1, 3) = t.y;
        r.at(2, 3) = t.z;
        return r;
    }

    static Mat4 scale(Vec3 s) {
        Mat4 r;
        r.at(0, 0) = s.x;
        r.at(1, 1) = s.y;
        r.at(2, 2) = s.z;
        return r;
    }

    // Right-handed perspective projection mapping the view frustum into the
    // [-1,1] clip cube. `fovY` is the vertical field of view in radians.
    //
    // The magic lives in the last column: it copies -z into w, so the GPU's
    // automatic divide-by-w shrinks distant geometry. That IS perspective.
    static Mat4 perspective(float fovY, float aspect, float zNear, float zFar) {
        const float f = 1.0f / std::tan(fovY * 0.5f);
        Mat4 r;
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zFar + zNear) / (zNear - zFar);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
        r.m[15] = 0.0f;
        return r;
    }

    // The view matrix moves the world so that the camera sits at the origin
    // looking down -Z. It is just the camera's basis vectors, transposed,
    // followed by a translation by -eye.
    static Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
        const Vec3 f = normalize(target - eye);   // forward
        const Vec3 s = normalize(cross(f, up));   // right ("side")
        const Vec3 u = cross(s, f);               // true up

        Mat4 r;
        r.at(0, 0) = s.x;  r.at(0, 1) = s.y;  r.at(0, 2) = s.z;
        r.at(1, 0) = u.x;  r.at(1, 1) = u.y;  r.at(1, 2) = u.z;
        r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z;
        r.at(0, 3) = -dot(s, eye);
        r.at(1, 3) = -dot(u, eye);
        r.at(2, 3) = dot(f, eye);
        return r;
    }

    // 2D screen-space projection for the HUD: pixels in, clip space out.
    static Mat4 ortho(float left, float right, float bottom, float top) {
        Mat4 r;
        r.at(0, 0) = 2.0f / (right - left);
        r.at(1, 1) = 2.0f / (top - bottom);
        r.at(2, 2) = -1.0f;
        r.at(0, 3) = -(right + left) / (right - left);
        r.at(1, 3) = -(top + bottom) / (top - bottom);
        return r;
    }
};

// Matrix product. Note the order: `a * b` applies b first, then a, so a full
// transform reads right-to-left:  projection * view * model * vertex.
inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.at(row, k) * b.at(k, c);
            r.at(row, c) = sum;
        }
    }
    return r;
}

// Transform a point (w = 1) without the perspective divide.
inline Vec3 transformPoint(const Mat4& a, Vec3 p) {
    return {a.at(0, 0) * p.x + a.at(0, 1) * p.y + a.at(0, 2) * p.z + a.at(0, 3),
            a.at(1, 0) * p.x + a.at(1, 1) * p.y + a.at(1, 2) * p.z + a.at(1, 3),
            a.at(2, 0) * p.x + a.at(2, 1) * p.y + a.at(2, 2) * p.z + a.at(2, 3)};
}
