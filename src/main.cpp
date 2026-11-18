// ---------------------------------------------------------------------------
// ArchCraft - an educational voxel engine.
//
// THE GAME LOOP, which is the skeleton every real-time program shares:
//
//   while (running) {
//       1. read input      - drain the OS event queue
//       2. simulate        - advance the world by dt seconds
//       3. render          - draw one picture of the current state
//       4. present         - hand the finished frame to the display
//   }
//
// Everything else in this project is a detail hanging off one of those four
// lines. Start reading here, then follow whichever step interests you.
// ---------------------------------------------------------------------------
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/Clock.h"
#include "core/Input.h"
#include "core/Window.h"
#include "gfx/Hud.h"
#include "gfx/Screenshot.h"
#include "gfx/WorldRenderer.h"
#include "player/Player.h"
#include "world/World.h"

namespace {

struct Options {
    uint32_t seed = 1337;
    int renderDistance = 8;  // in chunks
    int exitAfterFrames = 0; // 0 = run until the user quits (used by tests)
    bool dumpAtlas = false;
    std::string screenshotPath;  // written on the final frame when set
};

void printHelp() {
    std::printf(
        "ArchCraft - a minimal Minecraft-style voxel engine\n\n"
        "Usage: archcraft [options]\n"
        "  --seed N             world seed (default 1337)\n"
        "  --render-distance N  chunks loaded around the player, 2..24 (default 8)\n"
        "  --frames N           render N frames then exit (for testing)\n"
        "  --dump-atlas         write the generated texture atlas to atlas.tga\n"
        "  --screenshot FILE    save the final frame as a TGA image\n"
        "  --help               show this message\n\n"
        "Controls:\n"
        "  move          W A S D          jump        Space\n"
        "  look          mouse            sprint      Left Ctrl\n"
        "  break block   left click       place block right click\n"
        "  pick block    1-9 / scroll     fly         F\n"
        "  descend (fly) Left Shift       screenshot  F2\n"
        "  release mouse / quit   Esc\n");
}

Options parseArguments(int argc, char** argv, bool& shouldExit) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        const auto nextInt = [&](int fallback) {
            return (i + 1 < argc) ? std::atoi(argv[++i]) : fallback;
        };
        if (std::strcmp(arg, "--seed") == 0) {
            options.seed = static_cast<uint32_t>(nextInt(1337));
        } else if (std::strcmp(arg, "--render-distance") == 0) {
            options.renderDistance = nextInt(8);
            if (options.renderDistance < 2) options.renderDistance = 2;
            if (options.renderDistance > 24) options.renderDistance = 24;
        } else if (std::strcmp(arg, "--frames") == 0) {
            options.exitAfterFrames = nextInt(0);
        } else if (std::strcmp(arg, "--dump-atlas") == 0) {
            options.dumpAtlas = true;
        } else if (std::strcmp(arg, "--screenshot") == 0) {
            if (i + 1 < argc) options.screenshotPath = argv[++i];
        } else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
            printHelp();
            shouldExit = true;
            return options;
        } else {
            std::fprintf(stderr, "unknown option: %s (try --help)\n", arg);
            shouldExit = true;
            return options;
        }
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    bool shouldExit = false;
    const Options options = parseArguments(argc, argv, shouldExit);
    if (shouldExit) return 0;

    // ---- 1. a window with a GPU context attached ----
    Window window;
    if (!window.create("ArchCraft", 1280, 760)) {
        std::fprintf(stderr, "fatal: %s\n", window.error().c_str());
        return 1;
    }
    std::printf("[archcraft] GL %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    // ---- 2. GPU resources: shaders, the generated atlas, the HUD ----
    WorldRenderer renderer;
    if (!renderer.initialise(ARCHCRAFT_ASSET_DIR)) {
        std::fprintf(stderr, "fatal: renderer initialisation failed (see shader errors above)\n");
        return 1;
    }
    Hud hud;
    if (!hud.initialise(ARCHCRAFT_ASSET_DIR)) {
        std::fprintf(stderr, "fatal: HUD initialisation failed\n");
        return 1;
    }
    renderer.resize(window.pixelWidth(), window.pixelHeight());

    if (options.dumpAtlas) {
        if (renderer.atlas().saveTga("atlas.tga")) std::printf("[archcraft] wrote atlas.tga\n");
    }

    // ---- 3. the world, and a player standing on it ----
    World world(options.seed);
    Player player;
    player.spawn(world.findSpawnPosition());
    std::printf("[archcraft] seed %u, spawn at %.1f %.1f %.1f, render distance %d chunks\n",
                options.seed, player.feetPosition().x, player.feetPosition().y,
                player.feetPosition().z, options.renderDistance);

    Input input;
    Clock clock;
    // In frame-limited test mode we leave the cursor alone; interactively we
    // grab it immediately so you can look around the moment the window opens.
    const bool testMode = options.exitAfterFrames > 0;
    if (!testMode) window.setMouseCaptured(true);

    float titleTimer = 0.0f;
    bool running = true;

    // ---------------------------- the game loop ----------------------------
    while (running) {
        const float dt = clock.tick();

        // --- step 1: input ---
        input.beginFrame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) input.handleEvent(event);

        if (input.quitRequested()) running = false;
        if (input.windowResized()) {
            window.refreshSize();
            renderer.resize(window.pixelWidth(), window.pixelHeight());
        }
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            // First Esc hands the mouse back to the OS, a second one quits.
            if (window.mouseCaptured()) {
                window.setMouseCaptured(false);
            } else {
                running = false;
            }
        }
        if (!window.mouseCaptured() && input.mousePressed(SDL_BUTTON_LEFT)) {
            window.setMouseCaptured(true);
            continue;  // swallow this click so it does not also break a block
        }

        // --- step 2: simulate ---
        // Only drive the player while we own the mouse, so clicking around in
        // another window does not rearrange the terrain.
        if (window.mouseCaptured() || testMode) player.update(dt, input, world);
        const World::UpdateStats streaming = world.update(player.feetPosition(), options.renderDistance);

        // --- step 3: render ---
        renderer.clear();
        const float renderDistanceBlocks =
            static_cast<float>(options.renderDistance * kChunkSizeX);
        const WorldRenderer::Stats stats =
            renderer.renderWorld(world, player.camera, renderDistanceBlocks);
        renderer.renderSelection(player.target(), player.camera);
        hud.render(window.pixelWidth(), window.pixelHeight(), player.hotbarIndex(),
                   renderer.atlas());

        // --- step 4: present ---
        const bool lastFrame =
            options.exitAfterFrames > 0 &&
            clock.frameCount() >= static_cast<unsigned long long>(options.exitAfterFrames);
        if (input.keyPressed(SDL_SCANCODE_F2) || (lastFrame && !options.screenshotPath.empty())) {
            const char* path = options.screenshotPath.empty() ? "screenshot.tga"
                                                              : options.screenshotPath.c_str();
            // Must happen BEFORE the swap: after it, the back buffer we just
            // drew into is no longer the one being read.
            if (saveScreenshot(path, window.pixelWidth(), window.pixelHeight())) {
                std::printf("[archcraft] wrote %s\n", path);
            }
        }
        window.swapBuffers();

        // Debug readout in the title bar: cheap, and no font rendering needed.
        titleTimer += dt;
        if (titleTimer > 0.25f) {
            titleTimer = 0.0f;
            const Vec3 pos = player.feetPosition();
            char title[256];
            std::snprintf(title, sizeof(title),
                          "ArchCraft | %.0f fps | xyz %.1f %.1f %.1f | chunks %zu (%d drawn, %d culled)"
                          " | tris %dk | %s%s",
                          clock.fps(), pos.x, pos.y, pos.z, world.chunkCount(), stats.chunksDrawn,
                          stats.chunksCulled, stats.triangles / 1000,
                          player.flying() ? "flying" : (player.onGround() ? "ground" : "falling"),
                          player.inWater() ? " | in water" : "");
            window.setTitle(title);
            (void)streaming;  // handy to print while tuning the budgets
        }

        if (options.exitAfterFrames > 0 &&
            clock.frameCount() >= static_cast<unsigned long long>(options.exitAfterFrames)) {
            const Vec3 finalPos = player.feetPosition();
            std::printf("[archcraft] %d frames, %.1f fps, %zu chunks, %d triangles, player at "
                        "%.1f %.1f %.1f (%s)\n",
                        options.exitAfterFrames, clock.fps(), world.chunkCount(), stats.triangles,
                        finalPos.x, finalPos.y, finalPos.z,
                        player.onGround() ? "on ground" : "airborne");
            running = false;
        }
    }

    return 0;
}
