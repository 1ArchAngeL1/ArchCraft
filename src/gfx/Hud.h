#pragma once
// ---------------------------------------------------------------------------
// The 2D overlay: crosshair and hotbar.
//
// The only difference from 3D rendering is the matrix. An orthographic
// projection maps pixel coordinates straight to the screen with no
// perspective divide, so a quad at (10,10)-(50,50) lands exactly there. Depth
// testing is off, because "on top" is simply "drawn later".
// ---------------------------------------------------------------------------
#include <vector>

#include "gfx/Mesh.h"
#include "gfx/Shader.h"
#include "gfx/TextureAtlas.h"
#include "world/Block.h"

class Hud {
public:
    bool initialise(const char* assetDir);
    void render(int pixelWidth, int pixelHeight, int hotbarIndex, TextureAtlas& atlas);

private:
    struct HudVertex {
        float x, y;
        float u, v;
        float r, g, b, a;
    };

    void pushQuad(std::vector<HudVertex>& sink, float x, float y, float w, float h, float u0,
                  float v0, float u1, float v1, float r, float g, float bl, float a);

    Shader shader_;
    Mesh solidMesh_;
    Mesh texturedMesh_;
    std::vector<HudVertex> solid_;
    std::vector<HudVertex> textured_;
};
