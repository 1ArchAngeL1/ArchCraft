#pragma once
// ---------------------------------------------------------------------------
// Perlin noise - the reason procedural terrain looks like terrain.
//
// Plain random numbers give you static: every point is independent, so a
// heightmap made of rand() is a spiky mess. Perlin noise is *smooth* and
// *continuous*: nearby points get nearby values, so it reads as hills.
//
// How it works, in four steps:
//   1. Chop space into a grid of integer cells.
//   2. Give every grid corner a pseudo-random gradient direction (from a
//      shuffled permutation table, so it is reproducible and needs no memory).
//   3. For a sample point, dot each corner's gradient with the offset to the
//      point - that is each corner's "opinion" of the height here.
//   4. Blend those opinions with a smooth fade curve.
//
// Stacking several frequencies on top of each other (fbm) adds the big shapes
// and the fine detail of real landscapes.
// ---------------------------------------------------------------------------
#include <array>
#include <cstdint>

class Noise {
public:
    explicit Noise(uint32_t seed);

    // Both return roughly [-1, 1].
    float perlin2(float x, float y) const;
    float perlin3(float x, float y, float z) const;

    // Fractal Brownian motion: sum octaves of noise, each one double the
    // frequency ("lacunarity") and half the amplitude ("gain").
    float fbm2(float x, float y, int octaves, float lacunarity = 2.0f, float gain = 0.5f) const;
    float fbm3(float x, float y, float z, int octaves, float lacunarity = 2.0f,
               float gain = 0.5f) const;

private:
    std::array<uint8_t, 512> perm_{};  // doubled, so we can index without wrapping
};

// Deterministic hash for "random but reproducible" decisions, e.g. "is there
// a tree at this column?". Unlike rand(), it depends only on its inputs, so
// the same world coordinate always gives the same answer - which is exactly
// what you need when chunks are generated in any order, at any time.
uint32_t hashCoords(int x, int z, uint32_t seed);
float hashCoords01(int x, int z, uint32_t seed);
