#include "world/ChunkMesher.h"

#include <array>

#include "gfx/TextureAtlas.h"
#include "world/World.h"

namespace {

// --------------------------------------------------------------- face tables
//
// Each face is a quad, listed so that its corners wind counter-clockwise when
// seen from OUTSIDE the cube. That matters because we enable back-face
// culling: the GPU throws away triangles whose winding looks clockwise, which
// for free discards every face pointing away from the camera. Get the winding
// backwards and your blocks turn inside out.
//
// (u, v) are texture corners, with v = 1 meaning the TOP of the tile.
struct FaceVertex {
    int px, py, pz;
    int u, v;
};

struct FaceInfo {
    int nx, ny, nz;    // outward normal
    float light;       // fake directional light, baked per face
    FaceVertex verts[4];
};

// Faces pointing up catch the most "sky", faces pointing down the least.
// Four distinct values are enough to read a cube's shape without any real
// lighting calculation - the same trick Minecraft uses.
constexpr FaceInfo kFaces[FaceCount] = {
    // -X
    {-1, 0, 0, 0.72f, {{0, 0, 0, 0, 0}, {0, 0, 1, 1, 0}, {0, 1, 1, 1, 1}, {0, 1, 0, 0, 1}}},
    // +X
    {1, 0, 0, 0.72f, {{1, 0, 0, 0, 0}, {1, 1, 0, 0, 1}, {1, 1, 1, 1, 1}, {1, 0, 1, 1, 0}}},
    // -Y (bottom)
    {0, -1, 0, 0.50f, {{0, 0, 0, 0, 0}, {1, 0, 0, 1, 0}, {1, 0, 1, 1, 1}, {0, 0, 1, 0, 1}}},
    // +Y (top)
    {0, 1, 0, 1.00f, {{0, 1, 0, 0, 0}, {0, 1, 1, 0, 1}, {1, 1, 1, 1, 1}, {1, 1, 0, 1, 0}}},
    // -Z
    {0, 0, -1, 0.86f, {{0, 0, 0, 0, 0}, {0, 1, 0, 0, 1}, {1, 1, 0, 1, 1}, {1, 0, 0, 1, 0}}},
    // +Z
    {0, 0, 1, 0.86f, {{0, 0, 1, 0, 0}, {1, 0, 1, 1, 0}, {1, 1, 1, 1, 1}, {0, 1, 1, 0, 1}}},
};

// ------------------------------------------------------------- padded cache
//
// The mesher reads each block's 26 neighbours (6 for face culling, more for
// ambient occlusion). Going through World::getBlock() for every one of those
// would mean millions of hash-map lookups per chunk. So we first copy the
// chunk PLUS a one-block skin of its neighbours into a flat array, then do
// all the work against that. ~42 KB, and every read is a single index.
constexpr int kPadX = kChunkSizeX + 2;
constexpr int kPadY = kChunkSizeY + 2;
constexpr int kPadZ = kChunkSizeZ + 2;

struct PaddedBlocks {
    std::array<uint8_t, static_cast<size_t>(kPadX) * kPadY * kPadZ> data{};

    static size_t index(int x, int y, int z) {
        // Shift by +1 so local coordinate -1 maps to array slot 0.
        return (static_cast<size_t>(y + 1) * kPadZ + static_cast<size_t>(z + 1)) * kPadX +
               static_cast<size_t>(x + 1);
    }

    BlockId get(int x, int y, int z) const { return static_cast<BlockId>(data[index(x, y, z)]); }
    void set(int x, int y, int z, BlockId id) { data[index(x, y, z)] = static_cast<uint8_t>(id); }
};

void fillPaddedBlocks(const World& world, ChunkPos pos, PaddedBlocks& cache) {
    const int originX = pos.x * kChunkSizeX;
    const int originZ = pos.z * kChunkSizeZ;

    for (int lz = -1; lz <= kChunkSizeZ; ++lz) {
        for (int lx = -1; lx <= kChunkSizeX; ++lx) {
            const int worldX = originX + lx;
            const int worldZ = originZ + lz;

            // One map lookup per column instead of per block.
            const Chunk* chunk = world.chunkAt({floorDiv(worldX, kChunkSizeX), floorDiv(worldZ, kChunkSizeZ)});
            const int bx = floorMod(worldX, kChunkSizeX);
            const int bz = floorMod(worldZ, kChunkSizeZ);

            if (chunk && chunk->terrainReady) {
                for (int y = 0; y < kChunkSizeY; ++y) cache.set(lx, y, lz, chunk->get(bx, y, bz));
            } else {
                // Neighbour not loaded yet: pretend it is solid so we do not
                // emit a wall of faces that will be hidden a moment later.
                for (int y = 0; y < kChunkSizeY; ++y) cache.set(lx, y, lz, BlockId::Stone);
            }

            // Below the world: solid, so bottom faces of the floor are culled.
            cache.set(lx, -1, lz, BlockId::Bedrock);
            // Above the world: open sky.
            cache.set(lx, kChunkSizeY, lz, BlockId::Air);
        }
    }
}

// ------------------------------------------------------------------- culling
// Should we emit the face of `self` that points at `neighbour`?
bool faceVisible(BlockId self, BlockId neighbour) {
    if (isOpaque(neighbour)) return false;             // buried
    if (isLiquid(self) && neighbour == self) return false;  // interior of a lake
    return true;
}

// -------------------------------------------------------- ambient occlusion
//
// Corners tucked into a crevice should be darker. For each quad corner we
// look at the three blocks that touch it in the layer in front of the face:
// two edge neighbours and the diagonal.
//
//   side1 side2 corner -> brightness level 0 (darkest) .. 3 (open)
//
// Both sides solid means the corner is fully pinched, which is darker than
// the count alone would suggest - hence the special case.
int vertexAo(bool side1, bool side2, bool corner) {
    if (side1 && side2) return 0;
    return 3 - (static_cast<int>(side1) + static_cast<int>(side2) + static_cast<int>(corner));
}

}  // namespace

void buildChunkMesh(const World& world, ChunkPos pos, ChunkMeshData& out) {
    out.clear();

    // Static, not local: 42 KB on the stack would be rude, and reusing the
    // buffer avoids an allocation per chunk. (Make this thread_local the day
    // meshing moves to worker threads - see docs/06.)
    static PaddedBlocks cache;
    fillPaddedBlocks(world, pos, cache);

    for (int y = 0; y < kChunkSizeY; ++y) {
        for (int z = 0; z < kChunkSizeZ; ++z) {
            for (int x = 0; x < kChunkSizeX; ++x) {
                const BlockId block = cache.get(x, y, z);
                if (isAir(block)) continue;

                const bool liquid = isLiquid(block);

                // Liquid surfaces sit slightly below the block top, so a lake
                // reads as a surface you can stand next to rather than a
                // solid glass cube.
                float topY = 1.0f;
                if (liquid && cache.get(x, y + 1, z) != block) topY = 0.875f;

                for (int faceIndex = 0; faceIndex < FaceCount; ++faceIndex) {
                    const FaceInfo& face = kFaces[faceIndex];
                    const BlockId neighbour = cache.get(x + face.nx, y + face.ny, z + face.nz);
                    if (!faceVisible(block, neighbour)) continue;

                    const int tile = tileForFace(block, static_cast<Face>(faceIndex));
                    const float tileU0 = TextureAtlas::tileU0(tile);
                    const float tileV0 = TextureAtlas::tileV0(tile);

                    // Which axis the face points along, and the two axes
                    // spanning the face - needed to find each corner's
                    // neighbours for ambient occlusion.
                    const int normalAxis = face.nx != 0 ? 0 : (face.ny != 0 ? 1 : 2);
                    const int tangentA = (normalAxis + 1) % 3;
                    const int tangentB = (normalAxis + 2) % 3;

                    BlockVertex quad[4];
                    int ao[4];

                    for (int corner = 0; corner < 4; ++corner) {
                        const FaceVertex& fv = face.verts[corner];

                        // --- ambient occlusion for this corner ---
                        if (liquid) {
                            ao[corner] = 3;  // flat lighting on water
                        } else {
                            const int cornerUnit[3] = {fv.px, fv.py, fv.pz};
                            const int offsetA = cornerUnit[tangentA] ? 1 : -1;
                            const int offsetB = cornerUnit[tangentB] ? 1 : -1;

                            // Start at the open cell in front of the face.
                            int side1[3] = {x + face.nx, y + face.ny, z + face.nz};
                            int side2[3] = {side1[0], side1[1], side1[2]};
                            int diagonal[3] = {side1[0], side1[1], side1[2]};
                            side1[tangentA] += offsetA;
                            side2[tangentB] += offsetB;
                            diagonal[tangentA] += offsetA;
                            diagonal[tangentB] += offsetB;

                            ao[corner] = vertexAo(isOpaque(cache.get(side1[0], side1[1], side1[2])),
                                                  isOpaque(cache.get(side2[0], side2[1], side2[2])),
                                                  isOpaque(cache.get(diagonal[0], diagonal[1], diagonal[2])));
                        }

                        // ao 0..3 maps to 0.55 .. 1.0 brightness.
                        const float aoShade = 0.55f + 0.15f * static_cast<float>(ao[corner]);

                        quad[corner].x = static_cast<float>(x + fv.px);
                        quad[corner].y = static_cast<float>(y) + (fv.py ? topY : 0.0f);
                        quad[corner].z = static_cast<float>(z + fv.pz);
                        // Inset the UVs so a face never samples the tile next
                        // door across the atlas seam.
                        quad[corner].u = tileU0 + (fv.u ? TextureAtlas::kTileUv - TextureAtlas::kInset
                                                        : TextureAtlas::kInset);
                        quad[corner].v = tileV0 + (fv.v ? TextureAtlas::kInset
                                                        : TextureAtlas::kTileUv - TextureAtlas::kInset);
                        quad[corner].shade = face.light * aoShade;
                    }

                    std::vector<BlockVertex>& sink = liquid ? out.water : out.opaque;

                    // A quad is two triangles, and WHICH diagonal we split on
                    // is visible: splitting across the bright pair makes the
                    // shading bend the wrong way and produces a telltale
                    // gradient seam. Pick the diagonal joining the two most
                    // similar corners.
                    if (ao[0] + ao[2] >= ao[1] + ao[3]) {
                        sink.push_back(quad[0]); sink.push_back(quad[1]); sink.push_back(quad[2]);
                        sink.push_back(quad[0]); sink.push_back(quad[2]); sink.push_back(quad[3]);
                    } else {
                        sink.push_back(quad[1]); sink.push_back(quad[2]); sink.push_back(quad[3]);
                        sink.push_back(quad[1]); sink.push_back(quad[3]); sink.push_back(quad[0]);
                    }
                }
            }
        }
    }
}
