#include "math/Frustum.h"

namespace {
Plane makePlane(float a, float b, float c, float d) {
    Plane p;
    const float len = std::sqrt(a * a + b * b + c * c);
    if (len > 1e-8f) {
        p.normal = {a / len, b / len, c / len};
        p.d = d / len;
    }
    return p;
}
}  // namespace

void Frustum::update(const Mat4& vp) {
    // A point is inside the frustum when, in clip space, -w <= x,y,z <= w.
    // Each of those six inequalities rearranges into a plane in world space
    // built from rows of the view-projection matrix.
    const float m00 = vp.at(0, 0), m01 = vp.at(0, 1), m02 = vp.at(0, 2), m03 = vp.at(0, 3);
    const float m10 = vp.at(1, 0), m11 = vp.at(1, 1), m12 = vp.at(1, 2), m13 = vp.at(1, 3);
    const float m20 = vp.at(2, 0), m21 = vp.at(2, 1), m22 = vp.at(2, 2), m23 = vp.at(2, 3);
    const float m30 = vp.at(3, 0), m31 = vp.at(3, 1), m32 = vp.at(3, 2), m33 = vp.at(3, 3);

    planes_[0] = makePlane(m30 + m00, m31 + m01, m32 + m02, m33 + m03);  // left
    planes_[1] = makePlane(m30 - m00, m31 - m01, m32 - m02, m33 - m03);  // right
    planes_[2] = makePlane(m30 + m10, m31 + m11, m32 + m12, m33 + m13);  // bottom
    planes_[3] = makePlane(m30 - m10, m31 - m11, m32 - m12, m33 - m13);  // top
    planes_[4] = makePlane(m30 + m20, m31 + m21, m32 + m22, m33 + m23);  // near
    planes_[5] = makePlane(m30 - m20, m31 - m21, m32 - m22, m33 - m23);  // far
}

bool Frustum::isBoxVisible(Vec3 min, Vec3 max) const {
    for (const Plane& plane : planes_) {
        // Pick the box corner furthest along the plane normal (the "positive
        // vertex"). If even that corner is behind the plane, the whole box is.
        const Vec3 positive{plane.normal.x >= 0.0f ? max.x : min.x,
                            plane.normal.y >= 0.0f ? max.y : min.y,
                            plane.normal.z >= 0.0f ? max.z : min.z};
        if (plane.distance(positive) < 0.0f) return false;
    }
    return true;
}
