#include "world/TerrainGen.h"

#include <cmath>

#include "world/Chunk.h"

TerrainGen::TerrainGen(uint32_t seed)
    : seed_(seed),
      // Each noise field gets its own permutation table. Reusing one table
      // with different frequencies would correlate the fields: your mountains
      // would secretly line up with your biomes.
      continent_(seed + 1),
      hills_(seed + 2),
      detail_(seed + 3),
      caves_(seed + 4),
      temperature_(seed + 5) {}

int TerrainGen::surfaceHeight(int worldX, int worldZ) const {
    const float fx = static_cast<float>(worldX);
    const float fz = static_cast<float>(worldZ);

    // Three scales of noise, each doing a different job. Tuning these numbers
    // is most of what "world generation" feels like in practice - try it.
    const float continent = continent_.fbm2(fx * 0.0021f, fz * 0.0021f, 4);  // km-scale
    const float hills = hills_.fbm2(fx * 0.011f, fz * 0.011f, 4);            // 100m-scale
    const float detail = detail_.fbm2(fx * 0.047f, fz * 0.047f, 2);          // 10m-scale

    // Ridged noise: fold the absolute value so valleys become sharp crests.
    // Applied only where the continent noise is already high, so mountain
    // ranges appear inland rather than sprinkled over the whole map.
    const float mountainMask = clampf((continent - 0.12f) / 0.45f, 0.0f, 1.0f);
    const float ridge = 1.0f - std::fabs(hills);

    float height = static_cast<float>(kSeaLevel) - 3.0f + continent * 24.0f + hills * 6.0f +
                   detail * 2.0f + mountainMask * ridge * ridge * 38.0f;

    const int result = static_cast<int>(height);
    return result < 2 ? 2 : (result > kChunkSizeY - 10 ? kChunkSizeY - 10 : result);
}

TerrainGen::Biome TerrainGen::biomeAt(int worldX, int worldZ, int height) const {
    const float temperature =
        temperature_.fbm2(static_cast<float>(worldX) * 0.0016f, static_cast<float>(worldZ) * 0.0016f, 3);

    // High ground is always cold, regardless of the temperature field.
    if (height > kSeaLevel + 34) return Biome::Snow;
    if (temperature > 0.30f) return Biome::Desert;
    if (temperature < -0.26f) return Biome::Snow;
    if (temperature > 0.04f) return Biome::Forest;
    return Biome::Plains;
}

bool TerrainGen::isCave(int worldX, int y, int worldZ) const {
    const float fx = static_cast<float>(worldX);
    const float fy = static_cast<float>(y);
    const float fz = static_cast<float>(worldZ);

    // "Worm" caves: take two independent noise fields and carve only where
    // BOTH are near zero. The zero-set of one 3D noise field is a surface;
    // the intersection of two surfaces is a line - so you get tunnels rather
    // than blobby swiss cheese. Stretching y makes them run horizontally.
    const float a = caves_.fbm3(fx * 0.020f, fy * 0.042f, fz * 0.020f, 2);
    const float b = caves_.fbm3(fx * 0.020f + 31.7f, fy * 0.042f + 11.3f, fz * 0.020f + 7.9f, 2);
    return std::fabs(a) < 0.055f && std::fabs(b) < 0.055f;
}

void TerrainGen::plantTree(Chunk& chunk, int localX, int baseY, int localZ) const {
    const int worldX = chunk.originX() + localX;
    const int worldZ = chunk.originZ() + localZ;
    const int trunkHeight = 4 + static_cast<int>(hashCoords(worldX, worldZ, seed_ + 77) % 3);
    const int topY = baseY + trunkHeight;

    // Canopy first, trunk second, so the trunk wins any overlap.
    for (int dy = -2; dy <= 1; ++dy) {
        const int radius = (dy <= 0) ? 2 : 1;
        for (int dz = -radius; dz <= radius; ++dz) {
            for (int dx = -radius; dx <= radius; ++dx) {
                // Trim the corners of the widest layers to round the canopy.
                if (radius == 2 && std::abs(dx) == 2 && std::abs(dz) == 2) continue;
                if (dx == 0 && dz == 0 && dy <= 0) continue;  // leave room for the trunk
                const int y = topY + dy;
                if (y < 0 || y >= kChunkSizeY) continue;
                if (isAir(chunk.get(localX + dx, y, localZ + dz))) {
                    chunk.set(localX + dx, y, localZ + dz, BlockId::Leaves);
                }
            }
        }
    }

    for (int y = baseY; y <= topY; ++y) {
        if (y >= 0 && y < kChunkSizeY) chunk.set(localX, y, localZ, BlockId::Log);
    }
}

void TerrainGen::generate(Chunk& chunk) const {
    for (int localZ = 0; localZ < kChunkSizeZ; ++localZ) {
        for (int localX = 0; localX < kChunkSizeX; ++localX) {
            const int worldX = chunk.originX() + localX;
            const int worldZ = chunk.originZ() + localZ;
            const int height = surfaceHeight(worldX, worldZ);
            const Biome biome = biomeAt(worldX, worldZ, height);
            const bool coastal = height <= kSeaLevel + 1;

            for (int y = 0; y <= height; ++y) {
                BlockId block;

                if (y == 0) {
                    block = BlockId::Bedrock;  // unbreakable world floor
                } else if (y < 3 && hashCoords01(worldX * 31 + y, worldZ, seed_ + 13) < 0.55f) {
                    block = BlockId::Bedrock;  // ragged bedrock layer
                } else if (y == height) {
                    // The surface block.
                    if (coastal) {
                        block = BlockId::Sand;
                    } else {
                        switch (biome) {
                            case Biome::Desert: block = BlockId::Sand; break;
                            case Biome::Snow: block = BlockId::Snow; break;
                            default: block = BlockId::Grass; break;
                        }
                    }
                } else if (y >= height - 3) {
                    // Soil layer under the surface.
                    block = (biome == Biome::Desert || coastal) ? BlockId::Sand : BlockId::Dirt;
                } else {
                    block = BlockId::Stone;
                }

                // Carve caves, but never the bedrock floor (so you cannot fall
                // out of the world) and never the surface skin above sea level
                // (so lakes do not drain into tunnels).
                if (block != BlockId::Bedrock && y > 1 && y < height - 2 && isCave(worldX, y, worldZ)) {
                    continue;  // leave it Air
                }

                chunk.set(localX, y, localZ, block);
            }

            // Flood everything below sea level that is still air.
            for (int y = height + 1; y <= kSeaLevel; ++y) {
                if (isAir(chunk.get(localX, y, localZ))) chunk.set(localX, y, localZ, BlockId::Water);
            }
        }
    }

    // --- features (trees) ---
    //
    // Trees are placed in a second pass, and only when they fit entirely
    // inside the chunk (hence the 2-block inset). A tree straddling a chunk
    // border would need the neighbour to exist already, which would make
    // generation order-dependent - exactly what we promised not to do. Real
    // engines solve this with a deferred "feature" queue; see docs/06.
    for (int localZ = 2; localZ < kChunkSizeZ - 2; ++localZ) {
        for (int localX = 2; localX < kChunkSizeX - 2; ++localX) {
            const int worldX = chunk.originX() + localX;
            const int worldZ = chunk.originZ() + localZ;
            const int height = surfaceHeight(worldX, worldZ);
            if (height <= kSeaLevel + 1) continue;
            if (chunk.get(localX, height, localZ) != BlockId::Grass) continue;

            const Biome biome = biomeAt(worldX, worldZ, height);
            const float density = (biome == Biome::Forest) ? 0.035f : 0.006f;
            if (hashCoords01(worldX, worldZ, seed_ + 101) >= density) continue;

            plantTree(chunk, localX, height + 1, localZ);
        }
    }

    chunk.terrainReady = true;
    chunk.meshDirty = true;
}
