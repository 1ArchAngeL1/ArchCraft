#pragma once
// ---------------------------------------------------------------------------
// "Which block am I looking at?" - answered by walking the ray through the
// voxel grid one cell at a time (the Amanatides & Woo algorithm, 1987).
//
// You could step along the ray in small increments and sample, but that is
// both slower and WRONG: too large a step skips thin walls, too small a step
// wastes thousands of samples per frame. This walk visits every cell the ray
// actually touches, in order, exactly once - no misses, no duplicates.
// ---------------------------------------------------------------------------
#include "math/Vec.h"
#include "world/Block.h"

class World;

struct RayHit {
    bool hit = false;
    int x = 0, y = 0, z = 0;     // the block that was hit
    int normalX = 0, normalY = 0, normalZ = 0;  // which face was entered through
    float distance = 0.0f;
    BlockId block = BlockId::Air;
};

// Walks `dir` (normalised) from `origin` up to `maxDistance` blocks.
// `includeLiquid` picks whether water counts as a hit.
RayHit raycastVoxels(const World& world, Vec3 origin, Vec3 dir, float maxDistance,
                     bool includeLiquid = false);
