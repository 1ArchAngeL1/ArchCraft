#pragma once
// ---------------------------------------------------------------------------
// The world: a hash map of chunks that loads and unloads itself around the
// player, giving the illusion of infinite terrain.
//
// Each chunk moves through a small pipeline, and spreading that work over
// frames instead of doing it all at once is what keeps the frame rate smooth:
//
//   (missing) --generate--> terrainReady --mesh--> drawable --too far--> freed
//
// Per-frame budgets (kChunksGeneratedPerFrame / kChunksMeshedPerFrame) cap
// how much of that pipeline runs each frame. Without them, flying fast would
// stall for half a second whenever a batch of chunks came into range.
// ---------------------------------------------------------------------------
#include <memory>
#include <unordered_map>

#include "world/Chunk.h"
#include "world/ChunkMesher.h"
#include "world/TerrainGen.h"

class World {
public:
    using ChunkMap = std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash>;

    struct UpdateStats {
        int generated = 0;
        int meshed = 0;
        int unloaded = 0;
    };

    explicit World(uint32_t seed) : terrain_(seed) {}

    // --- block access, in world coordinates ---
    // Reads outside loaded chunks return Air; writes there are ignored.
    BlockId getBlock(int x, int y, int z) const;
    bool setBlock(int x, int y, int z, BlockId id);
    bool isSolidAt(int x, int y, int z) const { return isSolid(getBlock(x, y, z)); }

    const Chunk* chunkAt(ChunkPos pos) const;
    Chunk* chunkAt(ChunkPos pos);

    // Runs the streaming pipeline around `center`. Call once per frame.
    UpdateStats update(Vec3 center, int renderDistance);

    // Searches outwards from the origin for dry land above sea level and
    // returns somewhere safe to stand. Generates chunks synchronously, so call
    // it once, at startup.
    Vec3 findSpawnPosition();

    const ChunkMap& chunks() const { return chunks_; }
    size_t chunkCount() const { return chunks_.size(); }
    const TerrainGen& terrain() const { return terrain_; }

    static ChunkPos chunkPosFor(int worldX, int worldZ) {
        return {floorDiv(worldX, kChunkSizeX), floorDiv(worldZ, kChunkSizeZ)};
    }

private:
    static constexpr int kChunksGeneratedPerFrame = 4;
    static constexpr int kChunksMeshedPerFrame = 3;

    Chunk& ensureChunk(ChunkPos pos);
    void uploadMesh(Chunk& chunk);
    void markNeighbourDirty(ChunkPos pos);

    ChunkMap chunks_;
    TerrainGen terrain_;
    ChunkMeshData scratch_;  // reused between chunks to avoid reallocating
};
