#pragma once
// ---------------------------------------------------------------------------
// Terrain generation: deciding what block sits at every coordinate in an
// infinite world, without ever storing the world.
//
// The one rule that makes this work: generation must be a PURE FUNCTION of
// (coordinate, seed). No state, no order dependence. Then a chunk generated
// now looks identical to the same chunk generated an hour from now, after a
// restart, on another machine - and neighbouring chunks line up seamlessly
// even though neither knew about the other.
// ---------------------------------------------------------------------------
#include <cstdint>

#include "world/Noise.h"

class Chunk;

class TerrainGen {
public:
    static constexpr int kSeaLevel = 62;

    explicit TerrainGen(uint32_t seed);

    // Fills every block of the chunk. Must be deterministic.
    void generate(Chunk& chunk) const;

    // The y of the topmost terrain block in this column (ignoring water and
    // trees). Cheap enough to call directly - used to place the player.
    int surfaceHeight(int worldX, int worldZ) const;

    uint32_t seed() const { return seed_; }

private:
    enum class Biome { Plains, Forest, Desert, Snow };

    Biome biomeAt(int worldX, int worldZ, int height) const;
    bool isCave(int worldX, int y, int worldZ) const;
    void plantTree(Chunk& chunk, int localX, int baseY, int localZ) const;

    uint32_t seed_;
    Noise continent_;    // where land and ocean are
    Noise hills_;        // medium bumps and ridges
    Noise detail_;       // fine roughness
    Noise caves_;        // 3D noise, carves tunnels
    Noise temperature_;  // biome selection
};
