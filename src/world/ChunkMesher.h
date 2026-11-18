#pragma once
// ---------------------------------------------------------------------------
// Turns a 3D grid of block ids into triangles.
//
// THE CENTRAL IDEA: a naive engine would draw a cube per block - 32768 cubes
// per chunk, each with 12 triangles, most of them buried inside solid rock
// where nobody can see them. Instead we walk the grid once and emit a quad
// ONLY where a block touches something see-through. A hill of 32768 blocks
// becomes a few thousand visible faces, uploaded once and drawn in ONE call.
//
// That single idea - "mesh the surface, not the volume" - is what makes voxel
// worlds possible at all. Everything else here is polish on top.
// ---------------------------------------------------------------------------
#include <vector>

#include "world/Chunk.h"

class World;

// The vertex format the block shader expects: 6 floats = 24 bytes.
//   position (3) - in chunk-local space; the model matrix moves it into place
//   uv       (2) - a point inside the texture atlas
//   shade    (1) - baked lighting: face direction x ambient occlusion
//
// Baking light into the vertex means the fragment shader does almost nothing,
// which matters because there are vastly more fragments than vertices.
struct BlockVertex {
    float x, y, z;
    float u, v;
    float shade;
};

struct ChunkMeshData {
    std::vector<BlockVertex> opaque;  // solid blocks + alpha-cut (leaves, glass)
    std::vector<BlockVertex> water;   // blended, drawn in a second pass

    void clear() {
        opaque.clear();
        water.clear();
    }
};

// Builds the mesh for `pos`. Reads neighbouring chunks too: a face on a chunk
// border can only be culled if we know what is on the other side.
void buildChunkMesh(const World& world, ChunkPos pos, ChunkMeshData& out);
