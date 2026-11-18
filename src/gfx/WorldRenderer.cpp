#include "gfx/WorldRenderer.h"

#include <string>
#include <vector>

#include "world/World.h"

namespace {

// The 12 edges of a unit cube as line endpoints, used for the block outline.
std::vector<float> buildCubeEdges() {
    const int edges[12][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0},  // bottom
                              {4, 5}, {5, 7}, {7, 6}, {6, 4},  // top
                              {0, 4}, {1, 5}, {2, 6}, {3, 7}}; // verticals
    const float corners[8][3] = {{0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {1, 0, 1},
                                 {0, 1, 0}, {1, 1, 0}, {0, 1, 1}, {1, 1, 1}};
    std::vector<float> data;
    data.reserve(12 * 2 * 3);
    for (const auto& edge : edges) {
        for (const int corner : edge) {
            data.push_back(corners[corner][0]);
            data.push_back(corners[corner][1]);
            data.push_back(corners[corner][2]);
        }
    }
    return data;
}

}  // namespace

bool WorldRenderer::initialise(const char* assetDir) {
    const std::string shaders = std::string(assetDir) + "/shaders/";
    if (!blockShader_.loadFromFiles(shaders + "block.vert", shaders + "block.frag")) return false;
    if (!lineShader_.loadFromFiles(shaders + "line.vert", shaders + "line.frag")) return false;
    if (!atlas_.create()) return false;

    const std::vector<float> edges = buildCubeEdges();
    selectionMesh_.upload(edges.data(), edges.size() * sizeof(float),
                          static_cast<int>(edges.size() / 3), {3});

    // Depth testing: the GPU keeps the nearest fragment per pixel. Without
    // it, whatever was drawn last wins and the world looks inside-out.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    // Back-face culling: skip triangles facing away from us. Free ~50% of the
    // rasterisation work, since a cube's far faces are never visible.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

void WorldRenderer::resize(int pixelWidth, int pixelHeight) {
    glViewport(0, 0, pixelWidth, pixelHeight);
    aspect_ = static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight > 0 ? pixelHeight : 1);
}

void WorldRenderer::clear() const {
    glClearColor(kSkyColour.x, kSkyColour.y, kSkyColour.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

WorldRenderer::Stats WorldRenderer::renderWorld(const World& world, const Camera& camera,
                                                float renderDistanceBlocks) {
    Stats stats;

    projection_ = camera.projectionMatrix(aspect_);
    const Mat4 view = camera.viewMatrix();
    frustum_.update(projection_ * view);

    atlas_.bind(0);
    blockShader_.bind();
    blockShader_.setMat4("uProjection", projection_);
    blockShader_.setMat4("uView", view);
    blockShader_.setInt("uAtlas", 0);
    blockShader_.setVec3("uFogColour", kSkyColour);
    // Start the fog well before the edge so chunks fade in rather than pop.
    blockShader_.setFloat("uFogStart", renderDistanceBlocks * 0.50f);
    blockShader_.setFloat("uFogEnd", renderDistanceBlocks * 0.88f);
    blockShader_.setVec3("uCameraPos", camera.position);

    // ---- pass 1: opaque ----
    blockShader_.setFloat("uAlphaCutoff", 0.5f);  // leaves/glass: discard holes
    for (const auto& [pos, chunk] : world.chunks()) {
        if (!chunk || chunk->opaqueMesh.empty()) continue;
        if (!frustum_.isBoxVisible(chunk->boundsMin(), chunk->boundsMax())) {
            ++stats.chunksCulled;
            continue;
        }
        // A chunk's vertices are chunk-local, so all we need is a translation.
        // Sending three floats beats sending a whole 16-float matrix.
        blockShader_.setVec3("uChunkOrigin",
                             {static_cast<float>(chunk->originX()), 0.0f,
                              static_cast<float>(chunk->originZ())});
        chunk->opaqueMesh.draw();
        ++stats.chunksDrawn;
        stats.triangles += chunk->opaqueTriangles;
    }

    // ---- pass 2: water ----
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);  // test against depth, but do not write it
    glDisable(GL_CULL_FACE);  // see the surface from below as well
    blockShader_.setFloat("uAlphaCutoff", 0.0f);
    for (const auto& [pos, chunk] : world.chunks()) {
        if (!chunk || chunk->waterMesh.empty()) continue;
        if (!frustum_.isBoxVisible(chunk->boundsMin(), chunk->boundsMax())) continue;
        blockShader_.setVec3("uChunkOrigin",
                             {static_cast<float>(chunk->originX()), 0.0f,
                              static_cast<float>(chunk->originZ())});
        chunk->waterMesh.draw();
        stats.triangles += chunk->waterTriangles;
    }
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    return stats;
}

void WorldRenderer::renderSelection(const RayHit& hit, const Camera& camera) {
    if (!hit.hit) return;

    lineShader_.bind();
    // Scale the box up a hair and recentre it, so its edges sit just outside
    // the block's faces instead of fighting them for the same depth values
    // (that fight is "z-fighting", and it looks like flickering stripes).
    constexpr float kBloat = 1.004f;
    const Mat4 model = Mat4::translation({static_cast<float>(hit.x) - (kBloat - 1.0f) * 0.5f,
                                          static_cast<float>(hit.y) - (kBloat - 1.0f) * 0.5f,
                                          static_cast<float>(hit.z) - (kBloat - 1.0f) * 0.5f}) *
                       Mat4::scale(Vec3{kBloat});
    lineShader_.setMat4("uMvp", projection_ * camera.viewMatrix() * model);
    lineShader_.setVec4("uColour", 0.05f, 0.05f, 0.07f, 0.9f);
    selectionMesh_.draw(GL_LINES);
}
