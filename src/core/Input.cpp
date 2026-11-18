#include "core/Input.h"

#include <SDL3/SDL.h>

void Input::beginFrame() {
    pressed_.fill(false);
    mousePressed_.fill(false);
    mouseDelta_ = {0.0f, 0.0f};
    scroll_ = 0.0f;
    resized_ = false;
}

void Input::handleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
            quit_ = true;
            break;

        case SDL_EVENT_KEY_DOWN:
            // `repeat` is the OS auto-repeat while a key is held; ignoring it
            // keeps keyPressed() to one true edge per physical press.
            if (!event.key.repeat) {
                down_[event.key.scancode] = true;
                pressed_[event.key.scancode] = true;
            }
            break;

        case SDL_EVENT_KEY_UP:
            down_[event.key.scancode] = false;
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            mouseDown_[event.button.button & 7] = true;
            mousePressed_[event.button.button & 7] = true;
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            mouseDown_[event.button.button & 7] = false;
            break;

        case SDL_EVENT_MOUSE_MOTION:
            // In relative mouse mode xrel/yrel are raw deltas, which is what a
            // first-person camera wants (absolute position is meaningless once
            // the cursor is locked).
            mouseDelta_.x += event.motion.xrel;
            mouseDelta_.y += event.motion.yrel;
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            scroll_ += event.wheel.y;
            break;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED:
            resized_ = true;
            break;

        default:
            break;
    }
}
