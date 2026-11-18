#pragma once
// ---------------------------------------------------------------------------
// Turns the SDL event stream into something a game loop can ask questions of.
//
// Two flavours of query, and the difference matters:
//   keyDown()    - is it held *right now*? Use for movement (continuous).
//   keyPressed() - did it go down *this frame*? Use for toggles (discrete).
// Polling held state directly in the loop would make a toggle fire 60x/second.
// ---------------------------------------------------------------------------
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

#include <array>

#include "math/Vec.h"

union SDL_Event;

class Input {
public:
    // Call at the top of each frame, before pumping events: clears the
    // one-frame-only state (edges, mouse delta, scroll).
    void beginFrame();
    void handleEvent(const SDL_Event& event);

    bool keyDown(SDL_Scancode code) const { return down_[code]; }
    bool keyPressed(SDL_Scancode code) const { return pressed_[code]; }

    bool mouseDown(int button) const { return mouseDown_[button & 7]; }
    bool mousePressed(int button) const { return mousePressed_[button & 7]; }

    Vec2 mouseDelta() const { return mouseDelta_; }
    float scrollDelta() const { return scroll_; }
    bool quitRequested() const { return quit_; }
    bool windowResized() const { return resized_; }

private:
    std::array<bool, SDL_SCANCODE_COUNT> down_{};
    std::array<bool, SDL_SCANCODE_COUNT> pressed_{};
    std::array<bool, 8> mouseDown_{};
    std::array<bool, 8> mousePressed_{};
    Vec2 mouseDelta_{};
    float scroll_ = 0.0f;
    bool quit_ = false;
    bool resized_ = false;
};
