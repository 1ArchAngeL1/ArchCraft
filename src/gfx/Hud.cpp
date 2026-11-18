#include "gfx/Hud.h"

#include <string>

namespace {
constexpr float kSlotSize = 52.0f;
constexpr float kSlotPadding = 4.0f;
constexpr float kHotbarBottomMargin = 18.0f;
constexpr float kCrosshairLength = 11.0f;
constexpr float kCrosshairThickness = 2.0f;
}  // namespace

bool Hud::initialise(const char* assetDir) {
    const std::string shaders = std::string(assetDir) + "/shaders/";
    return shader_.loadFromFiles(shaders + "hud.vert", shaders + "hud.frag");
}

void Hud::pushQuad(std::vector<HudVertex>& sink, float x, float y, float w, float h, float u0,
                   float v0, float u1, float v1, float r, float g, float bl, float a) {
    // Two triangles, counter-clockwise. Note v is flipped (v1 at the top) so
    // atlas tiles appear the right way up on screen.
    const HudVertex topLeft{x, y + h, u0, v0, r, g, bl, a};
    const HudVertex bottomLeft{x, y, u0, v1, r, g, bl, a};
    const HudVertex bottomRight{x + w, y, u1, v1, r, g, bl, a};
    const HudVertex topRight{x + w, y + h, u1, v0, r, g, bl, a};

    sink.push_back(topLeft);
    sink.push_back(bottomLeft);
    sink.push_back(bottomRight);
    sink.push_back(topLeft);
    sink.push_back(bottomRight);
    sink.push_back(topRight);
}

void Hud::render(int pixelWidth, int pixelHeight, int hotbarIndex, TextureAtlas& atlas) {
    solid_.clear();
    textured_.clear();

    const float width = static_cast<float>(pixelWidth);
    const float height = static_cast<float>(pixelHeight);
    const float centreX = width * 0.5f;
    const float centreY = height * 0.5f;

    // --- crosshair: two bars, so it reads on any background ---
    pushQuad(solid_, centreX - kCrosshairLength, centreY - kCrosshairThickness * 0.5f,
             kCrosshairLength * 2.0f, kCrosshairThickness, 0, 0, 0, 0, 1, 1, 1, 0.85f);
    pushQuad(solid_, centreX - kCrosshairThickness * 0.5f, centreY - kCrosshairLength,
             kCrosshairThickness, kCrosshairLength * 2.0f, 0, 0, 0, 0, 1, 1, 1, 0.85f);

    // --- hotbar ---
    const float barWidth = 9.0f * kSlotSize + 10.0f * kSlotPadding;
    const float barX = centreX - barWidth * 0.5f;
    const float barY = kHotbarBottomMargin;
    pushQuad(solid_, barX, barY, barWidth, kSlotSize + 2.0f * kSlotPadding, 0, 0, 0, 0, 0.07f,
             0.07f, 0.09f, 0.62f);

    for (int slot = 0; slot < 9; ++slot) {
        const float slotX = barX + kSlotPadding + static_cast<float>(slot) * (kSlotSize + kSlotPadding);
        const float slotY = barY + kSlotPadding;

        // The selected slot gets a bright frame drawn behind its icon.
        if (slot == hotbarIndex) {
            pushQuad(solid_, slotX - 3.0f, slotY - 3.0f, kSlotSize + 6.0f, kSlotSize + 6.0f, 0, 0, 0,
                     0, 1.0f, 1.0f, 1.0f, 0.9f);
        }
        pushQuad(solid_, slotX, slotY, kSlotSize, kSlotSize, 0, 0, 0, 0, 0.16f, 0.16f, 0.2f, 0.75f);

        // The icon is just the block's side texture, sampled from the atlas.
        const int tile = tileForFace(kHotbarBlocks[slot], FacePosZ);
        const float u0 = TextureAtlas::tileU0(tile) + TextureAtlas::kInset;
        const float v0 = TextureAtlas::tileV0(tile) + TextureAtlas::kInset;
        const float u1 = u0 + TextureAtlas::kTileUv - 2.0f * TextureAtlas::kInset;
        const float v1 = v0 + TextureAtlas::kTileUv - 2.0f * TextureAtlas::kInset;
        const float inset = 6.0f;
        pushQuad(textured_, slotX + inset, slotY + inset, kSlotSize - 2.0f * inset,
                 kSlotSize - 2.0f * inset, u0, v0, u1, v1, 1, 1, 1, 1);
    }

    // --- draw ---
    const auto layout = {2, 2, 4};  // vec2 position (pixels), vec2 uv, vec4 colour
    solidMesh_.upload(solid_.data(), solid_.size() * sizeof(HudVertex),
                      static_cast<int>(solid_.size()), layout, true);
    texturedMesh_.upload(textured_.data(), textured_.size() * sizeof(HudVertex),
                         static_cast<int>(textured_.size()), layout, true);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);

    shader_.bind();
    shader_.setMat4("uProjection", Mat4::ortho(0.0f, width, 0.0f, height));
    shader_.setInt("uAtlas", 0);
    atlas.bind(0);

    shader_.setInt("uUseTexture", 0);
    solidMesh_.draw();
    shader_.setInt("uUseTexture", 1);
    texturedMesh_.draw();

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}
