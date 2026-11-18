#pragma once
// ---------------------------------------------------------------------------
// The player: a box that falls, walks into things, and edits the world.
//
// Collision uses an AABB (axis-aligned bounding box) swept one axis at a
// time. That per-axis trick is the whole reason this is only ~60 lines and
// still behaves: resolving X, then Y, then Z independently means "slide along
// the wall" falls out for free, instead of needing real contact resolution.
// ---------------------------------------------------------------------------
#include "core/Input.h"
#include "gfx/Camera.h"
#include "world/Raycast.h"

class World;

class Player {
public:
    // Dimensions in blocks (Minecraft's own numbers, which are tuned so that
    // a 1-block gap is walkable and a 2-block gap is not crawlable).
    static constexpr float kWidth = 0.6f;
    static constexpr float kHalfWidth = kWidth * 0.5f;
    static constexpr float kHeight = 1.8f;
    static constexpr float kEyeHeight = 1.62f;
    static constexpr float kReach = 5.0f;

    Camera camera;

    void spawn(Vec3 feetPosition);
    void update(float dt, const Input& input, World& world);

    Vec3 feetPosition() const { return position_; }
    Vec3 eyePosition() const { return {position_.x, position_.y + kEyeHeight, position_.z}; }
    Vec3 velocity() const { return velocity_; }
    bool onGround() const { return onGround_; }
    bool flying() const { return flying_; }
    bool inWater() const { return inWater_; }
    BlockId selectedBlock() const { return kHotbarBlocks[hotbarIndex_]; }
    int hotbarIndex() const { return hotbarIndex_; }
    const RayHit& target() const { return target_; }

private:
    void updateLook(const Input& input);
    void updateHotbar(const Input& input);
    void updateMovement(float dt, const Input& input, const World& world);
    void updateBlockEditing(const Input& input, World& world);

    // Moves along one axis and resolves any collision that results.
    void moveAxis(int axis, float amount, const World& world);
    bool boxOverlapsBlock(int bx, int by, int bz) const;

    Vec3 position_{0.0f, 80.0f, 0.0f};  // centre of the feet
    Vec3 velocity_{};
    bool flying_ = false;
    bool onGround_ = false;
    bool inWater_ = false;
    int hotbarIndex_ = 0;
    RayHit target_{};
};
