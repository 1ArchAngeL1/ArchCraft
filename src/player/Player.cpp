#include "player/Player.h"

#include <algorithm>
#include <cmath>

#include "world/World.h"

namespace {

// --- movement tuning (blocks per second, blocks per second squared) ---
constexpr float kWalkSpeed = 4.6f;
constexpr float kSprintSpeed = 7.4f;
constexpr float kFlySpeed = 16.0f;
constexpr float kSwimSpeed = 3.2f;
constexpr float kGravity = 28.0f;
constexpr float kJumpSpeed = 8.4f;     // -> ~1.25 blocks of jump height
constexpr float kSwimUpSpeed = 3.6f;
constexpr float kMaxFallSpeed = 60.0f;
constexpr float kMouseSensitivity = 0.0022f;

// A sliver of separation left between the player and a surface after a
// collision. Without it, floating point puts the box exactly on the boundary
// and the next frame's floor() may land inside the block - the cause of the
// classic "sinking through the floor" and "stuck in a wall" bugs.
constexpr float kSkin = 1e-3f;

float& component(Vec3& v, int axis) { return axis == 0 ? v.x : (axis == 1 ? v.y : v.z); }

// How far the box extends below/above `position_` on each axis.
constexpr float extentLow(int axis) { return axis == 1 ? 0.0f : Player::kHalfWidth; }
constexpr float extentHigh(int axis) { return axis == 1 ? Player::kHeight : Player::kHalfWidth; }

}  // namespace

void Player::spawn(Vec3 feetPosition) {
    position_ = feetPosition;
    velocity_ = {};
    camera.position = eyePosition();
}

void Player::update(float dt, const Input& input, World& world) {
    updateLook(input);
    updateHotbar(input);
    updateMovement(dt, input, world);

    camera.position = eyePosition();

    // Re-cast every frame: the crosshair target drives both the highlight box
    // and the next click.
    target_ = raycastVoxels(world, eyePosition(), camera.forward(), kReach);
    updateBlockEditing(input, world);
}

void Player::updateLook(const Input& input) {
    const Vec2 delta = input.mouseDelta();
    camera.applyMouseLook(delta.x, delta.y, kMouseSensitivity);
}

void Player::updateHotbar(const Input& input) {
    for (int slot = 0; slot < 9; ++slot) {
        const SDL_Scancode code = static_cast<SDL_Scancode>(SDL_SCANCODE_1 + slot);
        if (input.keyPressed(code)) hotbarIndex_ = slot;
    }
    const float scroll = input.scrollDelta();
    if (scroll != 0.0f) {
        hotbarIndex_ = (hotbarIndex_ - static_cast<int>(scroll) % 9 + 9) % 9;
    }
}

void Player::updateMovement(float dt, const Input& input, const World& world) {
    if (input.keyPressed(SDL_SCANCODE_F)) {
        flying_ = !flying_;
        velocity_.y = 0.0f;
    }

    // Are we in a liquid? Sampled at hip height, so standing in a 1-deep
    // puddle does not count as swimming.
    inWater_ = isLiquid(world.getBlock(floorToInt(position_.x), floorToInt(position_.y + 0.9f),
                                       floorToInt(position_.z)));

    // --- build the desired horizontal direction from WASD ---
    Vec3 wish{};
    if (input.keyDown(SDL_SCANCODE_W)) wish += camera.forwardHorizontal();
    if (input.keyDown(SDL_SCANCODE_S)) wish -= camera.forwardHorizontal();
    if (input.keyDown(SDL_SCANCODE_D)) wish += camera.right();
    if (input.keyDown(SDL_SCANCODE_A)) wish -= camera.right();
    // Normalising matters: without it, holding W+D would be 1.41x faster than
    // holding W alone. Every first-person game has shipped this bug once.
    if (lengthSq(wish) > 0.0f) wish = normalize(wish);

    const bool sprinting = input.keyDown(SDL_SCANCODE_LCTRL) || input.keyDown(SDL_SCANCODE_RCTRL);
    float speed = flying_ ? kFlySpeed : (inWater_ ? kSwimSpeed : (sprinting ? kSprintSpeed : kWalkSpeed));
    if (flying_ && sprinting) speed *= 2.5f;

    velocity_.x = wish.x * speed;
    velocity_.z = wish.z * speed;

    // --- vertical motion ---
    if (flying_) {
        // No gravity in fly mode: vertical speed is pure input.
        float vertical = 0.0f;
        if (input.keyDown(SDL_SCANCODE_SPACE)) vertical += 1.0f;
        if (input.keyDown(SDL_SCANCODE_LSHIFT)) vertical -= 1.0f;
        velocity_.y = vertical * speed;
    } else if (inWater_) {
        // Buoyancy: weak gravity, and space swims upwards.
        velocity_.y -= kGravity * 0.28f * dt;
        if (input.keyDown(SDL_SCANCODE_SPACE)) velocity_.y = kSwimUpSpeed;
        velocity_.y = clampf(velocity_.y, -kSwimSpeed, kSwimUpSpeed);
    } else {
        if (onGround_ && input.keyDown(SDL_SCANCODE_SPACE)) velocity_.y = kJumpSpeed;
        // v += a * dt, the entire integration step.
        velocity_.y -= kGravity * dt;
        if (velocity_.y < -kMaxFallSpeed) velocity_.y = -kMaxFallSpeed;
    }

    // --- integrate position, one axis at a time ---
    onGround_ = false;
    moveAxis(0, velocity_.x * dt, world);
    moveAxis(1, velocity_.y * dt, world);
    moveAxis(2, velocity_.z * dt, world);

    // Safety net: if the chunk under us has not finished loading, hold still
    // rather than falling through the floor of a world that does not exist yet.
    if (!flying_ && position_.y < 1.0f) {
        position_.y = 1.0f;
        velocity_.y = 0.0f;
    }
}

bool Player::boxOverlapsBlock(int bx, int by, int bz) const {
    const float minX = position_.x - kHalfWidth;
    const float maxX = position_.x + kHalfWidth;
    const float minZ = position_.z - kHalfWidth;
    const float maxZ = position_.z + kHalfWidth;
    const float minY = position_.y;
    const float maxY = position_.y + kHeight;

    return maxX > static_cast<float>(bx) && minX < static_cast<float>(bx + 1) &&
           maxY > static_cast<float>(by) && minY < static_cast<float>(by + 1) &&
           maxZ > static_cast<float>(bz) && minZ < static_cast<float>(bz + 1);
}

void Player::moveAxis(int axis, float amount, const World& world) {
    if (amount == 0.0f) return;

    component(position_, axis) += amount;

    // The box, after the tentative move.
    const Vec3 boxMin{position_.x - kHalfWidth, position_.y, position_.z - kHalfWidth};
    const Vec3 boxMax{position_.x + kHalfWidth, position_.y + kHeight, position_.z + kHalfWidth};

    // Every block the box now overlaps. The epsilon keeps a box whose face
    // sits exactly on a block boundary from claiming the next block too.
    constexpr float kEps = 1e-4f;
    const int minBX = floorToInt(boxMin.x + kEps), maxBX = floorToInt(boxMax.x - kEps);
    const int minBY = floorToInt(boxMin.y + kEps), maxBY = floorToInt(boxMax.y - kEps);
    const int minBZ = floorToInt(boxMin.z + kEps), maxBZ = floorToInt(boxMax.z - kEps);

    bool collided = false;
    float resolved = component(position_, axis);

    for (int by = minBY; by <= maxBY; ++by) {
        for (int bz = minBZ; bz <= maxBZ; ++bz) {
            for (int bx = minBX; bx <= maxBX; ++bx) {
                if (!world.isSolidAt(bx, by, bz)) continue;
                if (!boxOverlapsBlock(bx, by, bz)) continue;

                const int blockCoord = (axis == 0) ? bx : (axis == 1 ? by : bz);
                if (amount > 0.0f) {
                    // Moving positive: stop with our leading face at the
                    // block's near side.
                    const float stop = static_cast<float>(blockCoord) - extentHigh(axis) - kSkin;
                    resolved = collided ? std::min(resolved, stop) : stop;
                } else {
                    const float stop = static_cast<float>(blockCoord + 1) + extentLow(axis) + kSkin;
                    resolved = collided ? std::max(resolved, stop) : stop;
                }
                collided = true;
            }
        }
    }

    if (!collided) return;

    component(position_, axis) = resolved;
    component(velocity_, axis) = 0.0f;  // the wall absorbed our momentum

    // Hitting something while moving down means we landed: that is the only
    // signal "can I jump?" needs.
    if (axis == 1 && amount < 0.0f) onGround_ = true;
}

void Player::updateBlockEditing(const Input& input, World& world) {
    if (!target_.hit) return;

    // Left click: break.
    if (input.mousePressed(SDL_BUTTON_LEFT) && target_.block != BlockId::Bedrock) {
        world.setBlock(target_.x, target_.y, target_.z, BlockId::Air);
        return;
    }

    // Right click: place against the face we are looking at.
    if (input.mousePressed(SDL_BUTTON_RIGHT)) {
        const int px = target_.x + target_.normalX;
        const int py = target_.y + target_.normalY;
        const int pz = target_.z + target_.normalZ;

        const BlockId existing = world.getBlock(px, py, pz);
        if (!isAir(existing) && !isLiquid(existing)) return;

        // Never place a block inside yourself - that is how you get stuck.
        if (boxOverlapsBlock(px, py, pz)) return;

        world.setBlock(px, py, pz, selectedBlock());
    }
}
