#include "core/Window.h"

#include <SDL3/SDL.h>

#include "gfx/gl.h"

Window::~Window() {
    if (glContext_) SDL_GL_DestroyContext(static_cast<SDL_GLContext>(glContext_));
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool Window::create(const std::string& title, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        error_ = std::string("SDL_Init failed: ") + SDL_GetError();
        return false;
    }

    // Request the context *before* creating the window: these attributes are
    // part of the pixel format the window is created with.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);   // the depth buffer: see docs/02
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);

    window_ = SDL_CreateWindow(title.c_str(), width, height,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window_) {
        error_ = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        return false;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(window_);
    if (!ctx) {
        error_ = std::string("SDL_GL_CreateContext failed: ") + SDL_GetError();
        return false;
    }
    glContext_ = ctx;
    SDL_GL_MakeCurrent(window_, ctx);

    if (!glLoadFunctions()) {
        error_ = "failed to load required OpenGL 3.3 functions";
        return false;
    }

    // Swap interval 1 = vsync: present on the monitor's refresh, which caps us
    // at the display rate and avoids tearing. Not fatal if unsupported.
    SDL_GL_SetSwapInterval(1);

    refreshSize();
    return true;
}

void Window::refreshSize() {
    SDL_GetWindowSizeInPixels(window_, &pixelWidth_, &pixelHeight_);
    if (pixelWidth_ <= 0) pixelWidth_ = 1;
    if (pixelHeight_ <= 0) pixelHeight_ = 1;
}

float Window::aspect() const {
    return static_cast<float>(pixelWidth_) / static_cast<float>(pixelHeight_);
}

void Window::swapBuffers() { SDL_GL_SwapWindow(window_); }

void Window::setTitle(const std::string& title) { SDL_SetWindowTitle(window_, title.c_str()); }

void Window::setMouseCaptured(bool captured) {
    mouseCaptured_ = captured;
    SDL_SetWindowRelativeMouseMode(window_, captured);
}
