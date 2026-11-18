#pragma once
// ---------------------------------------------------------------------------
// Frustum culling: the cheapest optimisation in all of 3D graphics.
//
// The camera can only see inside a truncated pyramid (the "view frustum").
// Anything outside it is invisible, so we should never send it to the GPU.
// We extract the frustum's six planes directly from the view-projection
// matrix, then test each chunk's bounding box against them.
// ---------------------------------------------------------------------------
#include "math/Mat4.h"

struct Plane {
    Vec3 normal{0.0f, 1.0f, 0.0f};
    float d = 0.0f;  // plane equation: dot(normal, p) + d = 0

    float distance(Vec3 p) const { return dot(normal, p) + d; }
};

class Frustum {
public:
    // Build the six planes from a combined projection * view matrix.
    void update(const Mat4& viewProj);

    // True if the axis-aligned box might be visible. This is a conservative
    // test: it can return true for boxes that are actually hidden, which is
    // fine (we draw a few extra chunks) but never false for visible ones.
    bool isBoxVisible(Vec3 min, Vec3 max) const;

private:
    Plane planes_[6];
};
