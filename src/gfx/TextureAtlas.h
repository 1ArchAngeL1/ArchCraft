#pragma once
// ---------------------------------------------------------------------------
// One texture holding every block face, laid out as a 16x16 grid of 16x16
// pixel tiles (so 256x256 total).
//
// WHY AN ATLAS? Switching textures between draw calls is expensive, and a
// chunk mesh contains many block types in one buffer. With an atlas there is
// exactly one texture bound for the whole world, and each vertex's UV simply
// points at the right tile. One bind, thousands of blocks.
//
// The pixels are *generated in code* (see paintTile) - no image files, no
// image loader, and you can read exactly how each texture is made.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <vector>

#include "gfx/gl.h"

// Tile indices into the atlas grid (row-major, 16 per row).
enum Tile : uint8_t {
    TILE_GRASS_TOP = 0,
    TILE_GRASS_SIDE,
    TILE_DIRT,
    TILE_STONE,
    TILE_COBBLE,
    TILE_SAND,
    TILE_GRAVEL,
    TILE_LOG_SIDE,
    TILE_LOG_TOP,
    TILE_LEAVES,
    TILE_PLANKS,
    TILE_BRICK,
    TILE_GLASS,
    TILE_WATER,
    TILE_SNOW,
    TILE_BEDROCK,
};

class TextureAtlas {
public:
    static constexpr int kTileSize = 16;
    static constexpr int kTilesPerSide = 16;
    static constexpr int kAtlasSize = kTileSize * kTilesPerSide;
    static constexpr float kTileUv = 1.0f / static_cast<float>(kTilesPerSide);

    // A half-texel inset stops a face's UVs from landing exactly on the seam
    // between two tiles, where filtering/rounding could sample the neighbour.
    static constexpr float kInset = 0.25f / static_cast<float>(kAtlasSize);

    ~TextureAtlas();

    bool create();
    void bind(int unit = 0) const;
    void destroy();

    // Write the generated atlas to an uncompressed TGA so you can open it and
    // see what the code drew. Run with --dump-atlas.
    bool saveTga(const char* path) const;

    static float tileU0(int tile) { return static_cast<float>(tile % kTilesPerSide) * kTileUv; }
    static float tileV0(int tile) { return static_cast<float>(tile / kTilesPerSide) * kTileUv; }

private:
    GLuint texture_ = 0;
    std::vector<uint8_t> pixels_;  // RGBA8, kAtlasSize^2
};
