#include "world/Block.h"

#include "gfx/TextureAtlas.h"

namespace {

// Order must match the BlockId enum exactly.
constexpr BlockDef kBlockDefs[] = {
    // name       top              side             bottom          solid opaque liquid
    {"air",       0,               0,               0,              false, false, false},
    {"stone",     TILE_STONE,      TILE_STONE,      TILE_STONE,     true,  true,  false},
    {"dirt",      TILE_DIRT,       TILE_DIRT,       TILE_DIRT,      true,  true,  false},
    {"grass",     TILE_GRASS_TOP,  TILE_GRASS_SIDE, TILE_DIRT,      true,  true,  false},
    {"sand",      TILE_SAND,       TILE_SAND,       TILE_SAND,      true,  true,  false},
    {"gravel",    TILE_GRAVEL,     TILE_GRAVEL,     TILE_GRAVEL,    true,  true,  false},
    {"water",     TILE_WATER,      TILE_WATER,      TILE_WATER,     false, false, true},
    {"log",       TILE_LOG_TOP,    TILE_LOG_SIDE,   TILE_LOG_TOP,   true,  true,  false},
    // Leaves are solid but NOT opaque: you can see through the gaps, so the
    // mesher must keep the faces of blocks behind them.
    {"leaves",    TILE_LEAVES,     TILE_LEAVES,     TILE_LEAVES,    true,  false, false},
    {"planks",    TILE_PLANKS,     TILE_PLANKS,     TILE_PLANKS,    true,  true,  false},
    {"brick",     TILE_BRICK,      TILE_BRICK,      TILE_BRICK,     true,  true,  false},
    {"glass",     TILE_GLASS,      TILE_GLASS,      TILE_GLASS,     true,  false, false},
    {"snow",      TILE_SNOW,       TILE_SNOW,       TILE_SNOW,      true,  true,  false},
    {"cobble",    TILE_COBBLE,     TILE_COBBLE,     TILE_COBBLE,    true,  true,  false},
    {"bedrock",   TILE_BEDROCK,    TILE_BEDROCK,    TILE_BEDROCK,   true,  true,  false},
};

static_assert(sizeof(kBlockDefs) / sizeof(kBlockDefs[0]) == static_cast<size_t>(BlockId::Count),
              "every BlockId needs exactly one BlockDef row");

}  // namespace

const BlockDef& blockDef(BlockId id) {
    const auto index = static_cast<size_t>(id);
    return kBlockDefs[index < static_cast<size_t>(BlockId::Count) ? index : 0];
}

uint8_t tileForFace(BlockId id, Face face) {
    const BlockDef& def = blockDef(id);
    switch (face) {
        case FacePosY: return def.tileTop;
        case FaceNegY: return def.tileBottom;
        default: return def.tileSide;
    }
}

const BlockId kHotbarBlocks[9] = {
    BlockId::Grass, BlockId::Dirt,  BlockId::Stone,
    BlockId::Cobble, BlockId::Planks, BlockId::Brick,
    BlockId::Sand,  BlockId::Glass, BlockId::Log,
};
