#include "gfx/gl.h"

#if !defined(__APPLE__)
#include <SDL3/SDL.h>
#include <cstdio>

#define ARCHCRAFT_GL_DEFINE(ret, name, params) ret(APIENTRY* ac_##name) params = nullptr;
ARCHCRAFT_GL_FUNCTIONS(ARCHCRAFT_GL_DEFINE)
#undef ARCHCRAFT_GL_DEFINE

bool glLoadFunctions() {
    bool ok = true;
#define ARCHCRAFT_GL_LOAD(ret, name, params)                                   \
    ac_##name = reinterpret_cast<ret(APIENTRY*) params>(SDL_GL_GetProcAddress(#name)); \
    if (!ac_##name) {                                                          \
        std::fprintf(stderr, "[gl] missing entry point: %s\n", #name);          \
        ok = false;                                                            \
    }
    ARCHCRAFT_GL_FUNCTIONS(ARCHCRAFT_GL_LOAD)
#undef ARCHCRAFT_GL_LOAD
    return ok;
}
#endif
