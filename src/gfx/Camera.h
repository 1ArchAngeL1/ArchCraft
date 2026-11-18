#pragma once
// ---------------------------------------------------------------------------
// A first-person camera stored as position + two angles (Euler angles).
//
//   yaw   - rotation around the vertical axis (looking left/right)
//   pitch - rotation up/down, clamped so you can never flip over the top
//
// Storing angles rather than a matrix keeps the state tiny and makes mouse
// input trivial: just add the deltas.
// ---------------------------------------------------------------------------
#include "math/Mat4.h"

class Camera {
public:
    Vec3 position{0.0f, 80.0f, 0.0f};
    float yaw = 0.0f;    // radians, 0 = looking along +X
    float pitch = 0.0f;  // radians, + = looking up
    float fovDegrees = 72.0f;
    float nearPlane = 0.08f;
    float farPlane = 1000.0f;

    // Unit vectors describing the camera's orientation.
    Vec3 forward() const;
    Vec3 right() const;
    // Forward flattened onto the XZ plane: what "walk forward" should use, so
    // looking at the sky does not slow you down.
    Vec3 forwardHorizontal() const;

    void applyMouseLook(float dx, float dy, float sensitivity);

    Mat4 viewMatrix() const;
    Mat4 projectionMatrix(float aspect) const;
};
