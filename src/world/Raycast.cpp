#include "world/Raycast.h"

#include <cmath>

#include "world/World.h"

RayHit raycastVoxels(const World& world, Vec3 origin, Vec3 dir, float maxDistance,
                     bool includeLiquid) {
    RayHit result;

    // Start in the cell containing the ray origin.
    int x = floorToInt(origin.x);
    int y = floorToInt(origin.y);
    int z = floorToInt(origin.z);

    // Which way we walk along each axis.
    const int stepX = dir.x > 0.0f ? 1 : (dir.x < 0.0f ? -1 : 0);
    const int stepY = dir.y > 0.0f ? 1 : (dir.y < 0.0f ? -1 : 0);
    const int stepZ = dir.z > 0.0f ? 1 : (dir.z < 0.0f ? -1 : 0);

    // How far along the ray (in t) one whole cell of travel costs, per axis.
    // Infinity for an axis the ray does not move along, so it never "wins".
    constexpr float kInfinity = 1e30f;
    const float deltaX = stepX != 0 ? std::fabs(1.0f / dir.x) : kInfinity;
    const float deltaY = stepY != 0 ? std::fabs(1.0f / dir.y) : kInfinity;
    const float deltaZ = stepZ != 0 ? std::fabs(1.0f / dir.z) : kInfinity;

    // t at which the ray crosses the NEXT boundary on each axis.
    const auto firstBoundary = [](float position, int cell, int step, float delta) {
        if (step == 0) return 1e30f;
        const float fraction = position - static_cast<float>(cell);
        return (step > 0 ? (1.0f - fraction) : fraction) * delta;
    };
    float nextX = firstBoundary(origin.x, x, stepX, deltaX);
    float nextY = firstBoundary(origin.y, y, stepY, deltaY);
    float nextZ = firstBoundary(origin.z, z, stepZ, deltaZ);

    float travelled = 0.0f;
    int enteredNormal[3] = {0, 0, 0};

    while (travelled <= maxDistance) {
        if (y >= 0 && y < kChunkSizeY) {
            const BlockId block = world.getBlock(x, y, z);
            const bool solid = isSolid(block) || (includeLiquid && isLiquid(block));
            if (solid) {
                result.hit = true;
                result.x = x;
                result.y = y;
                result.z = z;
                result.normalX = enteredNormal[0];
                result.normalY = enteredNormal[1];
                result.normalZ = enteredNormal[2];
                result.distance = travelled;
                result.block = block;
                return result;
            }
        }

        // Step across whichever axis boundary is nearest. The normal points
        // back the way we came - that is the face we just entered through, and
        // exactly where a newly placed block should go.
        if (nextX < nextY && nextX < nextZ) {
            x += stepX;
            travelled = nextX;
            nextX += deltaX;
            enteredNormal[0] = -stepX;
            enteredNormal[1] = 0;
            enteredNormal[2] = 0;
        } else if (nextY < nextZ) {
            y += stepY;
            travelled = nextY;
            nextY += deltaY;
            enteredNormal[0] = 0;
            enteredNormal[1] = -stepY;
            enteredNormal[2] = 0;
        } else {
            z += stepZ;
            travelled = nextZ;
            nextZ += deltaZ;
            enteredNormal[0] = 0;
            enteredNormal[1] = 0;
            enteredNormal[2] = -stepZ;
        }
    }

    return result;  // nothing within reach
}
