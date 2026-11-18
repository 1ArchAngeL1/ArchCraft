#pragma once
// ---------------------------------------------------------------------------
// A chunk: a fixed-size column of blocks, and the unit of everything.
//
// WHY CHUNKS? An infinite world cannot live in one array, and a GPU cannot be
// handed one cube at a time (a draw call per block would mean millions of
// calls per frame). Chunks solve both: they are the unit of
//    * generation  - make terrain one chunk at a time, on demand
//    * meshing      - one vertex buffer per chunk, rebuilt only when it changes
//    * culling      - test one bounding box instead of 32768 blocks
//    * streaming   - load near the player, free far away
//
// 16 x 128 x 16 = 32768 blocks = 32 KB of block ids per chunk.
// ---------------------------------------------------------------------------
#include <array>
#include <cstddef>
#include <cstdint>

#include "gfx/Mesh.h"
#include "math/Vec.h"
#include "world/Block.h"

constexpr int kChunkSizeX = 16;
constexpr int kChunkSizeY = 128;
constexpr int kChunkSizeZ = 16;
constexpr int kChunkBlockCount = kChunkSizeX * kChunkSizeY * kChunkSizeZ;

// Which chunk, in chunk-grid coordinates (not blocks).
struct ChunkPos {
    int x = 0;
    int z = 0;

    bool operator==(const ChunkPos& other) const { return x == other.x && z == other.z; }
};

struct ChunkPosHash {
    size_t operator()(const ChunkPos& p) const {
        // Mix the two halves so that (1,0) and (0,1) do not collide.
        const uint64_t a = static_cast<uint32_t>(p.x);
        const uint64_t b = static_cast<uint32_t>(p.z);
        uint64_t h = a * 0x9E3779B97F4A7C15ull ^ (b + 0xBF58476D1CE4E5B9ull);
        h ^= h >> 31;
        h *= 0xD6E8FEB86659FD93ull;
        h ^= h >> 32;
        return static_cast<size_t>(h);
    }
};

class Chunk {
public:
    explicit Chunk(ChunkPos pos) : pos_(pos) {}

    ChunkPos pos() const { return pos_; }

    // World-space position of this chunk's (0,0,0) corner.
    int originX() const { return pos_.x * kChunkSizeX; }
    int originZ() const { return pos_.z * kChunkSizeZ; }

    // Local coordinates. Out-of-range reads return Air so callers do not have
    // to bounds-check every access.
    BlockId get(int x, int y, int z) const {
        if (!inBounds(x, y, z)) return BlockId::Air;
        return static_cast<BlockId>(blocks_[index(x, y, z)]);
    }

    void set(int x, int y, int z, BlockId id) {
        if (!inBounds(x, y, z)) return;
        blocks_[index(x, y, z)] = static_cast<uint8_t>(id);
    }

    static bool inBounds(int x, int y, int z) {
        return x >= 0 && x < kChunkSizeX && y >= 0 && y < kChunkSizeY && z >= 0 && z < kChunkSizeZ;
    }

    // Y-major ordering: blocks in the same column are adjacent in memory,
    // which is the access pattern terrain generation uses.
    static constexpr size_t index(int x, int y, int z) {
        return (static_cast<size_t>(y) * kChunkSizeZ + static_cast<size_t>(z)) * kChunkSizeX +
               static_cast<size_t>(x);
    }

    Vec3 boundsMin() const { return {static_cast<float>(originX()), 0.0f, static_cast<float>(originZ())}; }
    Vec3 boundsMax() const {
        return {static_cast<float>(originX() + kChunkSizeX), static_cast<float>(kChunkSizeY),
                static_cast<float>(originZ() + kChunkSizeZ)};
    }

    // --- state flags driving the streaming pipeline (see World::update) ---
    bool terrainReady = false;  // blocks have been generated
    bool meshDirty = true;      // vertex buffers need rebuilding

    // One mesh for solid blocks, one for water. They must be drawn in
    // separate passes because transparency needs different GL state.
    Mesh opaqueMesh;
    Mesh waterMesh;
    int opaqueTriangles = 0;
    int waterTriangles = 0;

private:
    ChunkPos pos_;
    std::array<uint8_t, kChunkBlockCount> blocks_{};  // all Air (0) initially
};
