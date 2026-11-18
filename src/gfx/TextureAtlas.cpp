#include "gfx/TextureAtlas.h"

#include <cmath>
#include <cstdio>

namespace {

// A cheap integer hash. Deterministic, so the world looks identical every run
// (and you can diff screenshots). This is the same trick used for noise.
uint32_t hash3(int x, int y, uint32_t salt) {
    uint32_t h = salt * 0x9E3779B9u;
    h ^= static_cast<uint32_t>(x) * 0x85EBCA6Bu;
    h ^= static_cast<uint32_t>(y) * 0xC2B2AE35u;
    h ^= h >> 15;
    h *= 0x2545F491u;
    h ^= h >> 13;
    return h;
}

// Pseudo-random in [0,1) for a pixel.
float noise01(int x, int y, uint32_t salt) {
    return static_cast<float>(hash3(x, y, salt) & 0xFFFFFFu) / 16777216.0f;
}

int clampByte(float v) { return v < 0.0f ? 0 : (v > 255.0f ? 255 : static_cast<int>(v)); }

// A view onto one tile of the atlas, with tile-local pixel coordinates.
struct Painter {
    uint8_t* pixels;
    int originX;
    int originY;

    void set(int x, int y, float r, float g, float b, float a = 255.0f) {
        const int px = originX + x;
        const int py = originY + y;
        uint8_t* p = pixels + (static_cast<size_t>(py) * TextureAtlas::kAtlasSize + px) * 4;
        p[0] = static_cast<uint8_t>(clampByte(r));
        p[1] = static_cast<uint8_t>(clampByte(g));
        p[2] = static_cast<uint8_t>(clampByte(b));
        p[3] = static_cast<uint8_t>(clampByte(a));
    }

    // Flat colour plus per-pixel brightness jitter: the one trick that makes
    // 16x16 pixel art read as "stone" instead of "grey square".
    void speckle(float r, float g, float b, float amount, uint32_t salt) {
        for (int y = 0; y < TextureAtlas::kTileSize; ++y) {
            for (int x = 0; x < TextureAtlas::kTileSize; ++x) {
                const float n = (noise01(x, y, salt) - 0.5f) * 2.0f * amount;
                set(x, y, r + n, g + n, b + n);
            }
        }
    }
};

void paintTile(uint8_t* pixels, int tile) {
    constexpr int S = TextureAtlas::kTileSize;
    Painter p{pixels, (tile % TextureAtlas::kTilesPerSide) * S,
              (tile / TextureAtlas::kTilesPerSide) * S};

    switch (tile) {
        case TILE_GRASS_TOP:
            p.speckle(96, 152, 68, 22, 11);
            // Scatter a few brighter blades of grass.
            for (int i = 0; i < 26; ++i) {
                const int x = static_cast<int>(noise01(i, 0, 71) * S);
                const int y = static_cast<int>(noise01(i, 1, 73) * S);
                p.set(x, y, 118, 176, 82);
            }
            break;

        case TILE_GRASS_SIDE:
            // Dirt body with a grass lip whose depth wobbles per column, so
            // the seam does not look like a ruler-straight line.
            for (int x = 0; x < S; ++x) {
                const int lip = 3 + static_cast<int>(noise01(x, 5, 23) * 2.5f);
                for (int y = 0; y < S; ++y) {
                    const float n = (noise01(x, y, 31) - 0.5f) * 36.0f;
                    if (y < lip) {
                        p.set(x, y, 92 + n, 148 + n, 64 + n);
                    } else {
                        p.set(x, y, 134 + n, 100 + n, 72 + n);
                    }
                }
            }
            break;

        case TILE_DIRT:
            p.speckle(134, 100, 72, 24, 41);
            break;

        case TILE_STONE:
            p.speckle(128, 128, 132, 16, 53);
            break;

        case TILE_COBBLE: {
            // Cobblestone = irregular cells. Quantise position into 4x4 cells,
            // give each cell its own shade, then darken the cell borders.
            p.speckle(120, 120, 124, 10, 61);
            for (int y = 0; y < S; ++y) {
                for (int x = 0; x < S; ++x) {
                    const int cx = x / 4;
                    const int cy = y / 4;
                    const float shade = 96.0f + noise01(cx, cy, 67) * 52.0f;
                    const bool border = (x % 4 == 0) || (y % 4 == 0);
                    const float v = border ? 72.0f : shade + (noise01(x, y, 79) - 0.5f) * 14.0f;
                    p.set(x, y, v, v, v * 1.02f);
                }
            }
            break;
        }

        case TILE_SAND:
            p.speckle(218, 204, 152, 14, 83);
            break;

        case TILE_GRAVEL:
            p.speckle(126, 118, 112, 30, 89);
            break;

        case TILE_LOG_SIDE:
            // Vertical bark grain.
            for (int x = 0; x < S; ++x) {
                const float grain = (noise01(x, 0, 97) - 0.5f) * 30.0f;
                for (int y = 0; y < S; ++y) {
                    const float n = (noise01(x, y, 101) - 0.5f) * 12.0f;
                    p.set(x, y, 104 + grain + n, 76 + grain * 0.7f + n, 46 + grain * 0.5f + n);
                }
            }
            break;

        case TILE_LOG_TOP:
            // Growth rings: quantise the distance from the centre.
            for (int y = 0; y < S; ++y) {
                for (int x = 0; x < S; ++x) {
                    const float dx = x - 7.5f;
                    const float dy = y - 7.5f;
                    const float d = std::sqrt(dx * dx + dy * dy);
                    const float ring = std::fmod(d, 2.6f) < 1.3f ? 1.0f : 0.82f;
                    const float n = (noise01(x, y, 103) - 0.5f) * 10.0f;
                    p.set(x, y, 168 * ring + n, 132 * ring + n, 84 * ring + n);
                }
            }
            break;

        case TILE_LEAVES:
            // Note the alpha-0 pixels: the fragment shader discards them, so
            // you can see through the gaps in foliage. See docs/02.
            for (int y = 0; y < S; ++y) {
                for (int x = 0; x < S; ++x) {
                    const float n = noise01(x, y, 107);
                    if (n < 0.16f) {
                        p.set(x, y, 0, 0, 0, 0);  // hole
                    } else {
                        const float v = (n - 0.5f) * 44.0f;
                        p.set(x, y, 58 + v, 112 + v, 50 + v);
                    }
                }
            }
            break;

        case TILE_PLANKS:
            for (int y = 0; y < S; ++y) {
                const int plank = y / 4;
                const bool edge = (y % 4 == 3);
                for (int x = 0; x < S; ++x) {
                    // Each plank row gets its own seam offset, like real boards.
                    const bool seam = ((x + plank * 5) % 8) == 0;
                    const float n = (noise01(x, y, 109 + plank) - 0.5f) * 18.0f;
                    const float dark = (edge || seam) ? 0.72f : 1.0f;
                    p.set(x, y, (164 + n) * dark, (126 + n) * dark, (82 + n) * dark);
                }
            }
            break;

        case TILE_BRICK:
            for (int y = 0; y < S; ++y) {
                const int row = y / 4;
                const int offset = (row % 2) ? 4 : 0;  // running bond
                for (int x = 0; x < S; ++x) {
                    const bool mortar = (y % 4 == 0) || ((x + offset) % 8 == 0);
                    const float n = (noise01(x, y, 127) - 0.5f) * 16.0f;
                    if (mortar) {
                        p.set(x, y, 176 + n, 170 + n, 162 + n);
                    } else {
                        p.set(x, y, 152 + n, 74 + n, 60 + n);
                    }
                }
            }
            break;

        case TILE_GLASS:
            // Mostly empty, with a frame and one highlight streak.
            for (int y = 0; y < S; ++y) {
                for (int x = 0; x < S; ++x) {
                    const bool frame = x == 0 || y == 0 || x == S - 1 || y == S - 1;
                    const bool highlight = (x + y == 6) || (x + y == 7);
                    if (frame) {
                        p.set(x, y, 206, 224, 232, 210);
                    } else if (highlight) {
                        p.set(x, y, 236, 248, 255, 92);
                    } else {
                        p.set(x, y, 220, 236, 244, 26);
                    }
                }
            }
            break;

        case TILE_WATER:
            // Semi-transparent: the alpha here is what the blending pass uses.
            for (int y = 0; y < S; ++y) {
                for (int x = 0; x < S; ++x) {
                    const float wave = std::sin((x + y * 0.5f) * 0.9f) * 8.0f;
                    const float n = (noise01(x, y, 131) - 0.5f) * 10.0f;
                    p.set(x, y, 56 + wave + n, 98 + wave + n, 182 + wave + n, 170.0f);
                }
            }
            break;

        case TILE_SNOW:
            p.speckle(240, 244, 250, 8, 137);
            break;

        case TILE_BEDROCK:
            // Chaotic dark blocks - deliberately ugly, it marks the world floor.
            for (int y = 0; y < S; ++y) {
                for (int x = 0; x < S; ++x) {
                    const float v = 40.0f + noise01(x / 2, y / 2, 139) * 60.0f;
                    p.set(x, y, v, v, v);
                }
            }
            break;

        default:
            // Unused tiles: magenta/black checker, the universal "missing
            // texture" so mistakes are obvious instead of subtle.
            for (int y = 0; y < S; ++y) {
                for (int x = 0; x < S; ++x) {
                    const bool a = ((x / 4) + (y / 4)) % 2 == 0;
                    p.set(x, y, a ? 240.0f : 20.0f, 20.0f, a ? 240.0f : 20.0f);
                }
            }
            break;
    }
}

}  // namespace

TextureAtlas::~TextureAtlas() { destroy(); }

void TextureAtlas::destroy() {
    if (texture_) glDeleteTextures(1, &texture_);
    texture_ = 0;
}

bool TextureAtlas::create() {
    pixels_.assign(static_cast<size_t>(kAtlasSize) * kAtlasSize * 4, 0);
    for (int tile = 0; tile < kTilesPerSide * kTilesPerSide; ++tile) {
        paintTile(pixels_.data(), tile);
    }

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kAtlasSize, kAtlasSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels_.data());

    // GL_NEAREST: no smoothing, so 16x16 art stays crisp pixel art instead of
    // turning into mush. Mipmaps are deliberately NOT used here: they would
    // blend neighbouring atlas tiles into each other at distance (the classic
    // "atlas bleeding" bug). docs/06 explains the padded-atlas fix.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture_ != 0;
}

void TextureAtlas::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
    glBindTexture(GL_TEXTURE_2D, texture_);
}

bool TextureAtlas::saveTga(const char* path) const {
    if (pixels_.empty()) return false;
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;

    uint8_t header[18] = {};
    header[2] = 2;  // uncompressed true-colour
    header[12] = static_cast<uint8_t>(kAtlasSize & 0xFF);
    header[13] = static_cast<uint8_t>((kAtlasSize >> 8) & 0xFF);
    header[14] = static_cast<uint8_t>(kAtlasSize & 0xFF);
    header[15] = static_cast<uint8_t>((kAtlasSize >> 8) & 0xFF);
    header[16] = 32;    // bits per pixel
    header[17] = 0x28;  // top-left origin, 8 alpha bits
    std::fwrite(header, 1, sizeof(header), f);

    // TGA wants BGRA.
    for (size_t i = 0; i < pixels_.size(); i += 4) {
        const uint8_t bgra[4] = {pixels_[i + 2], pixels_[i + 1], pixels_[i], pixels_[i + 3]};
        std::fwrite(bgra, 1, 4, f);
    }
    std::fclose(f);
    return true;
}
