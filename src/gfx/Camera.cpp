#include "gfx/Camera.h"

namespace {
constexpr float kPitchLimit = 1.55334f;  // ~89 degrees
}

Vec3 Camera::forward() const {
    const float cp = std::cos(pitch);
    return {std::cos(yaw) * cp, std::sin(pitch), std::sin(yaw) * cp};
}

Vec3 Camera::right() const {
    // Perpendicular to forward, always level with the horizon.
    return {-std::sin(yaw), 0.0f, std::cos(yaw)};
}

Vec3 Camera::forwardHorizontal() const { return {std::cos(yaw), 0.0f, std::sin(yaw)}; }

void Camera::applyMouseLook(float dx, float dy, float sensitivity) {
    yaw += dx * sensitivity;
    pitch -= dy * sensitivity;  // screen Y grows downwards
    pitch = clampf(pitch, -kPitchLimit, kPitchLimit);

    // Keep yaw bounded so it cannot drift into float-precision mush after a
    // long session of spinning.
    constexpr float kTwoPi = 6.28318530718f;
    if (yaw > kTwoPi) yaw -= kTwoPi;
    if (yaw < -kTwoPi) yaw += kTwoPi;
}

Mat4 Camera::viewMatrix() const {
    return Mat4::lookAt(position, position + forward(), {0.0f, 1.0f, 0.0f});
}

Mat4 Camera::projectionMatrix(float aspect) const {
    return Mat4::perspective(radians(fovDegrees), aspect, nearPlane, farPlane);
}
