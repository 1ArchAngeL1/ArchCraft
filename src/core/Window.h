#pragma once
// ---------------------------------------------------------------------------
// Owns the SDL window + the OpenGL context: the bridge between "an app the OS
// knows about" and "a thing that can talk to the GPU".
// ---------------------------------------------------------------------------
#include <string>

struct SDL_Window;
using SDL_GLContextHandle = void*;

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Creates the window and a 3.3 core-profile GL context. Returns false and
    // fills `error()` on failure.
    bool create(const std::string& title, int width, int height);

    void swapBuffers();
    void setTitle(const std::string& title);

    // Capture the mouse: hides the cursor and feeds us raw deltas so the
    // camera can turn forever without the pointer hitting a screen edge.
    void setMouseCaptured(bool captured);
    bool mouseCaptured() const { return mouseCaptured_; }

    // Size in *pixels* (not points) — differs on HiDPI/Retina displays, and
    // the GL viewport wants pixels.
    int pixelWidth() const { return pixelWidth_; }
    int pixelHeight() const { return pixelHeight_; }
    float aspect() const;
    void refreshSize();

    SDL_Window* handle() const { return window_; }
    const std::string& error() const { return error_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContextHandle glContext_ = nullptr;
    int pixelWidth_ = 0;
    int pixelHeight_ = 0;
    bool mouseCaptured_ = false;
    std::string error_;
};
