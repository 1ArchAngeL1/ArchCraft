#pragma once
// ---------------------------------------------------------------------------
// Draws the world.
//
// The frame is built in three passes, and the ORDER is not arbitrary:
//
//   1. opaque blocks   - depth test + depth write on, back faces culled.
//                        Nearest surface wins; everything behind is discarded.
//   2. water           - alpha blended, depth write OFF. Blending needs what
//                        is behind it to already be in the framebuffer, so it
//                        must come after (1); writing depth would let one water
//                        quad hide another.
//   3. selection box   - lines, drawn last so it is never swallowed by a face
//                        it is sitting on.
// ---------------------------------------------------------------------------
#include "gfx/Camera.h"
#include "gfx/Mesh.h"
#include "gfx/Shader.h"
#include "gfx/TextureAtlas.h"
#include "math/Frustum.h"
#include "world/Raycast.h"

class World;

class WorldRenderer {
public:
    struct Stats {
        int chunksDrawn = 0;
        int chunksCulled = 0;
        int triangles = 0;
    };

    // Sky colour doubles as the fog colour, which is what makes the fade at
    // the render distance invisible instead of looking like a grey wall.
    static constexpr Vec3 kSkyColour{0.55f, 0.72f, 0.93f};

    bool initialise(const char* assetDir);
    void resize(int pixelWidth, int pixelHeight);
    void clear() const;

    Stats renderWorld(const World& world, const Camera& camera, float renderDistanceBlocks);
    void renderSelection(const RayHit& hit, const Camera& camera);

    TextureAtlas& atlas() { return atlas_; }

private:
    Shader blockShader_;
    Shader lineShader_;
    TextureAtlas atlas_;
    Mesh selectionMesh_;
    Frustum frustum_;
    Mat4 projection_;
    float aspect_ = 1.0f;
};
