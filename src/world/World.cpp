#include "world/World.h"

#include <algorithm>
#include <vector>

const Chunk* World::chunkAt(ChunkPos pos) const {
    const auto it = chunks_.find(pos);
    return it == chunks_.end() ? nullptr : it->second.get();
}

Chunk* World::chunkAt(ChunkPos pos) {
    const auto it = chunks_.find(pos);
    return it == chunks_.end() ? nullptr : it->second.get();
}

BlockId World::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= kChunkSizeY) return BlockId::Air;
    const Chunk* chunk = chunkAt(chunkPosFor(x, z));
    if (!chunk) return BlockId::Air;
    return chunk->get(floorMod(x, kChunkSizeX), y, floorMod(z, kChunkSizeZ));
}

bool World::setBlock(int x, int y, int z, BlockId id) {
    if (y < 0 || y >= kChunkSizeY) return false;
    const ChunkPos pos = chunkPosFor(x, z);
    Chunk* chunk = chunkAt(pos);
    if (!chunk || !chunk->terrainReady) return false;

    const int localX = floorMod(x, kChunkSizeX);
    const int localZ = floorMod(z, kChunkSizeZ);
    if (chunk->get(localX, y, localZ) == id) return false;

    chunk->set(localX, y, localZ, id);
    chunk->meshDirty = true;

    // A block on a chunk border is part of its neighbour's mesh too: the
    // neighbour culled (or kept) a face based on what used to be here.
    // Forgetting this is the classic "hole in the world at chunk edges" bug.
    if (localX == 0) markNeighbourDirty({pos.x - 1, pos.z});
    if (localX == kChunkSizeX - 1) markNeighbourDirty({pos.x + 1, pos.z});
    if (localZ == 0) markNeighbourDirty({pos.x, pos.z - 1});
    if (localZ == kChunkSizeZ - 1) markNeighbourDirty({pos.x, pos.z + 1});
    return true;
}

void World::markNeighbourDirty(ChunkPos pos) {
    if (Chunk* neighbour = chunkAt(pos)) neighbour->meshDirty = true;
}

Chunk& World::ensureChunk(ChunkPos pos) {
    auto it = chunks_.find(pos);
    if (it == chunks_.end()) {
        it = chunks_.emplace(pos, std::make_unique<Chunk>(pos)).first;
    }
    Chunk& chunk = *it->second;
    if (!chunk.terrainReady) {
        terrain_.generate(chunk);
        // Our own faces changed, and so did our neighbours' border faces:
        // they were meshed against "unloaded = solid" and must be redone.
        markNeighbourDirty({pos.x - 1, pos.z});
        markNeighbourDirty({pos.x + 1, pos.z});
        markNeighbourDirty({pos.x, pos.z - 1});
        markNeighbourDirty({pos.x, pos.z + 1});
    }
    return chunk;
}

void World::uploadMesh(Chunk& chunk) {
    buildChunkMesh(*this, chunk.pos(), scratch_);

    const auto layout = {3, 2, 1};  // vec3 position, vec2 uv, float shade
    chunk.opaqueMesh.upload(scratch_.opaque.data(), scratch_.opaque.size() * sizeof(BlockVertex),
                            static_cast<int>(scratch_.opaque.size()), layout);
    chunk.waterMesh.upload(scratch_.water.data(), scratch_.water.size() * sizeof(BlockVertex),
                           static_cast<int>(scratch_.water.size()), layout);
    chunk.opaqueTriangles = static_cast<int>(scratch_.opaque.size() / 3);
    chunk.waterTriangles = static_cast<int>(scratch_.water.size() / 3);
    chunk.meshDirty = false;
}

World::UpdateStats World::update(Vec3 center, int renderDistance) {
    UpdateStats stats;

    const ChunkPos centerChunk = chunkPosFor(floorToInt(center.x), floorToInt(center.z));

    // Walk the square around the player, keep what is inside the circle, and
    // sort by distance so the chunk you are about to walk into is built first.
    struct Candidate {
        ChunkPos pos;
        int distanceSq;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(static_cast<size_t>((2 * renderDistance + 1) * (2 * renderDistance + 1)));

    for (int dz = -renderDistance; dz <= renderDistance; ++dz) {
        for (int dx = -renderDistance; dx <= renderDistance; ++dx) {
            const int distanceSq = dx * dx + dz * dz;
            if (distanceSq > renderDistance * renderDistance) continue;
            candidates.push_back({{centerChunk.x + dx, centerChunk.z + dz}, distanceSq});
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.distanceSq < b.distanceSq; });

    // Stage 1: generate terrain, nearest first, up to the budget.
    for (const Candidate& candidate : candidates) {
        if (stats.generated >= kChunksGeneratedPerFrame) break;
        const Chunk* existing = chunkAt(candidate.pos);
        if (existing && existing->terrainReady) continue;
        ensureChunk(candidate.pos);
        ++stats.generated;
    }

    // Stage 2: (re)build meshes, but only for chunks whose four neighbours
    // already have terrain - otherwise we would mesh against missing data and
    // have to throw the result away immediately.
    for (const Candidate& candidate : candidates) {
        if (stats.meshed >= kChunksMeshedPerFrame) break;
        Chunk* chunk = chunkAt(candidate.pos);
        if (!chunk || !chunk->terrainReady || !chunk->meshDirty) continue;

        const ChunkPos neighbours[4] = {{candidate.pos.x - 1, candidate.pos.z},
                                        {candidate.pos.x + 1, candidate.pos.z},
                                        {candidate.pos.x, candidate.pos.z - 1},
                                        {candidate.pos.x, candidate.pos.z + 1}};
        bool ready = true;
        for (const ChunkPos& neighbour : neighbours) {
            const Chunk* other = chunkAt(neighbour);
            if (!other || !other->terrainReady) {
                ready = false;
                break;
            }
        }
        if (!ready) continue;

        uploadMesh(*chunk);
        ++stats.meshed;
    }

    // Stage 3: free chunks that drifted out of range. The +2 margin stops a
    // chunk from being freed and regenerated every time you step back and
    // forth across a boundary (hysteresis).
    const int keepDistance = renderDistance + 2;
    for (auto it = chunks_.begin(); it != chunks_.end();) {
        const int dx = it->first.x - centerChunk.x;
        const int dz = it->first.z - centerChunk.z;
        if (dx * dx + dz * dz > keepDistance * keepDistance) {
            it = chunks_.erase(it);  // the Mesh destructor frees the GPU buffers
            ++stats.unloaded;
        } else {
            ++it;
        }
    }

    return stats;
}

Vec3 World::findSpawnPosition() {
    // Spawning underwater or inside a tree is a bad first impression, so walk
    // outwards in rings until a column looks habitable. surfaceHeight() is
    // just noise - no chunk is generated until a candidate passes.
    constexpr int kSampleSpacing = 8;
    for (int ring = 0; ring < 64; ++ring) {
        for (int offset = -ring; offset <= ring; ++offset) {
            const int perimeter[4][2] = {{offset, -ring}, {offset, ring},
                                         {-ring, offset}, {ring, offset}};
            for (const auto& cell : perimeter) {
                const int x = cell[0] * kSampleSpacing;
                const int z = cell[1] * kSampleSpacing;
                const int height = terrain_.surfaceHeight(x, z);
                if (height <= TerrainGen::kSeaLevel + 1) continue;  // sea or beach

                ensureChunk(chunkPosFor(x, z));
                for (int y = kChunkSizeY - 1; y > 0; --y) {
                    if (!isSolidAt(x, y, z)) continue;
                    // Landing on a tree canopy counts as a miss: try elsewhere.
                    if (y > height + 1) break;
                    return {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 1.2f,
                            static_cast<float>(z) + 0.5f};
                }
            }
        }
    }
    // Every column within 4000 blocks was ocean. Float above the water.
    return {0.5f, static_cast<float>(TerrainGen::kSeaLevel) + 3.0f, 0.5f};
}
