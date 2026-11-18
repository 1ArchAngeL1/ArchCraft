#pragma once
// ---------------------------------------------------------------------------
// Block types.
//
// A voxel world stores one byte per block, so a block "is" just an id. All of
// its behaviour lives in a lookup table of BlockDef - this is the data-driven
// pattern Minecraft itself uses (its block registry is the same idea, grown
// up). Adding a block = adding one enum value and one table row.
// ---------------------------------------------------------------------------
#include <cstdint>

enum class BlockId : uint8_t {
    Air = 0,
    Stone,
    Dirt,
    Grass,
    Sand,
    Gravel,
    Water,
    Log,
    Leaves,
    Planks,
    Brick,
    Glass,
    Snow,
    Cobble,
    Bedrock,
    Count,
};

// The six faces of a cube, in the order used by the mesher's lookup tables.
enum Face : int {
    FaceNegX = 0,
    FacePosX,
    FaceNegY,
    FacePosY,
    FaceNegZ,
    FacePosZ,
    FaceCount,
};

struct BlockDef {
    const char* name;
    uint8_t tileTop;
    uint8_t tileSide;
    uint8_t tileBottom;
    bool solid;    // stops the player (and the raycast)
    bool opaque;   // hides the faces of neighbouring blocks
    bool liquid;   // rendered in the transparent pass, swimmable
};

const BlockDef& blockDef(BlockId id);

inline bool isAir(BlockId id) { return id == BlockId::Air; }
inline bool isSolid(BlockId id) { return blockDef(id).solid; }
inline bool isOpaque(BlockId id) { return blockDef(id).opaque; }
inline bool isLiquid(BlockId id) { return blockDef(id).liquid; }

uint8_t tileForFace(BlockId id, Face face);

// The blocks offered on the hotbar, in order.
extern const BlockId kHotbarBlocks[9];
