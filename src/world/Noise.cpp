#include "world/Noise.h"

#include <cmath>
#include <utility>

namespace {

// Smoothstep-like fade: 6t^5 - 15t^4 + 10t^3. Its first and second
// derivatives are zero at 0 and 1, which is why Perlin noise has no visible
// grid-aligned creases.
inline float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Turn a hash into one of 12 gradient directions and dot it with (x,y,z).
// The bit-twiddling just picks a gradient; the classic Perlin trick is that
// all 12 gradients can be applied with adds and subtracts, no table lookup.
inline float grad3(uint8_t hash, float x, float y, float z) {
    const int h = hash & 15;
    const float u = h < 8 ? x : y;
    const float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

}  // namespace

Noise::Noise(uint32_t seed) {
    std::array<uint8_t, 256> base{};
    for (int i = 0; i < 256; ++i) base[static_cast<size_t>(i)] = static_cast<uint8_t>(i);

    // Fisher-Yates shuffle driven by a tiny xorshift PRNG, so one seed value
    // deterministically produces one permutation table.
    uint32_t state = seed ? seed : 0x9E3779B9u;
    const auto next = [&state]() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    };
    for (int i = 255; i > 0; --i) {
        const int j = static_cast<int>(next() % static_cast<uint32_t>(i + 1));
        std::swap(base[static_cast<size_t>(i)], base[static_cast<size_t>(j)]);
    }
    for (int i = 0; i < 512; ++i) perm_[static_cast<size_t>(i)] = base[static_cast<size_t>(i & 255)];
}

float Noise::perlin3(float x, float y, float z) const {
    // Integer cell containing the point, and the fractional position inside it.
    const int xi = static_cast<int>(std::floor(x)) & 255;
    const int yi = static_cast<int>(std::floor(y)) & 255;
    const int zi = static_cast<int>(std::floor(z)) & 255;
    const float xf = x - std::floor(x);
    const float yf = y - std::floor(y);
    const float zf = z - std::floor(z);

    const float u = fade(xf);
    const float v = fade(yf);
    const float w = fade(zf);

    // Hash the 8 corners of the cell.
    const uint8_t a = perm_[static_cast<size_t>(xi)];
    const uint8_t b = perm_[static_cast<size_t>(xi + 1)];
    const uint8_t aa = perm_[static_cast<size_t>(a + yi)];
    const uint8_t ab = perm_[static_cast<size_t>(a + yi + 1)];
    const uint8_t ba = perm_[static_cast<size_t>(b + yi)];
    const uint8_t bb = perm_[static_cast<size_t>(b + yi + 1)];

    const float x1 = lerpf(grad3(perm_[static_cast<size_t>(aa + zi)], xf, yf, zf),
                           grad3(perm_[static_cast<size_t>(ba + zi)], xf - 1, yf, zf), u);
    const float x2 = lerpf(grad3(perm_[static_cast<size_t>(ab + zi)], xf, yf - 1, zf),
                           grad3(perm_[static_cast<size_t>(bb + zi)], xf - 1, yf - 1, zf), u);
    const float y1 = lerpf(x1, x2, v);

    const float x3 = lerpf(grad3(perm_[static_cast<size_t>(aa + zi + 1)], xf, yf, zf - 1),
                           grad3(perm_[static_cast<size_t>(ba + zi + 1)], xf - 1, yf, zf - 1), u);
    const float x4 = lerpf(grad3(perm_[static_cast<size_t>(ab + zi + 1)], xf, yf - 1, zf - 1),
                           grad3(perm_[static_cast<size_t>(bb + zi + 1)], xf - 1, yf - 1, zf - 1), u);
    const float y2 = lerpf(x3, x4, v);

    return lerpf(y1, y2, w);
}

float Noise::perlin2(float x, float y) const {
    // 2D is just 3D sliced at a fixed, non-integer z (integer slices would
    // land on grid planes where the noise is always zero).
    return perlin3(x, y, 0.3141592f);
}

float Noise::fbm2(float x, float y, int octaves, float lacunarity, float gain) const {
    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float normalisation = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += perlin2(x * frequency, y * frequency) * amplitude;
        normalisation += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return normalisation > 0.0f ? sum / normalisation : 0.0f;
}

float Noise::fbm3(float x, float y, float z, int octaves, float lacunarity, float gain) const {
    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float normalisation = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += perlin3(x * frequency, y * frequency, z * frequency) * amplitude;
        normalisation += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return normalisation > 0.0f ? sum / normalisation : 0.0f;
}

uint32_t hashCoords(int x, int z, uint32_t seed) {
    uint32_t h = seed * 0x9E3779B9u;
    h ^= static_cast<uint32_t>(x) * 0x85EBCA6Bu;
    h = (h << 13) | (h >> 19);
    h ^= static_cast<uint32_t>(z) * 0xC2B2AE35u;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    return h;
}

float hashCoords01(int x, int z, uint32_t seed) {
    return static_cast<float>(hashCoords(x, z, seed) & 0xFFFFFFu) / 16777216.0f;
}
