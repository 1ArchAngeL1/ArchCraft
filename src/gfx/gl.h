#pragma once
// ---------------------------------------------------------------------------
// OpenGL entry points.
//
// Why this file exists: the OS only guarantees you OpenGL 1.1 symbols. Every
// modern function (VAOs, shaders, ...) must be looked up at runtime from the
// driver. Libraries like GLAD/GLEW generate thousands of lines to do this; we
// need ~45 functions, so a tiny X-macro loader is enough and you can actually
// read it.
//
// macOS is the exception: its OpenGL framework exports the full 3.3 core
// profile directly, so there is nothing to load.
// ---------------------------------------------------------------------------

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>

inline bool glLoadFunctions() { return true; }  // nothing to do

#else
// SDL_opengl.h gives us the GL types and *all* the enum constants without
// declaring prototypes for the modern functions, which is exactly what we
// want: we declare those ourselves as function pointers below.
#include <SDL3/SDL_opengl.h>

// The set of modern GL functions ArchCraft uses: (return, name, params...)
#define ARCHCRAFT_GL_FUNCTIONS(X)                                                                   \
    X(void,   glGenVertexArrays,        (GLsizei, GLuint*))                                         \
    X(void,   glBindVertexArray,        (GLuint))                                                   \
    X(void,   glDeleteVertexArrays,     (GLsizei, const GLuint*))                                   \
    X(void,   glGenBuffers,             (GLsizei, GLuint*))                                         \
    X(void,   glBindBuffer,             (GLenum, GLuint))                                           \
    X(void,   glBufferData,             (GLenum, GLsizeiptr, const void*, GLenum))                  \
    X(void,   glDeleteBuffers,          (GLsizei, const GLuint*))                                   \
    X(void,   glVertexAttribPointer,    (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))   \
    X(void,   glEnableVertexAttribArray,(GLuint))                                                   \
    X(GLuint, glCreateShader,           (GLenum))                                                   \
    X(void,   glShaderSource,           (GLuint, GLsizei, const GLchar* const*, const GLint*))      \
    X(void,   glCompileShader,          (GLuint))                                                   \
    X(void,   glGetShaderiv,            (GLuint, GLenum, GLint*))                                   \
    X(void,   glGetShaderInfoLog,       (GLuint, GLsizei, GLsizei*, GLchar*))                       \
    X(void,   glDeleteShader,           (GLuint))                                                   \
    X(GLuint, glCreateProgram,          (void))                                                     \
    X(void,   glAttachShader,           (GLuint, GLuint))                                           \
    X(void,   glLinkProgram,            (GLuint))                                                   \
    X(void,   glGetProgramiv,           (GLuint, GLenum, GLint*))                                   \
    X(void,   glGetProgramInfoLog,      (GLuint, GLsizei, GLsizei*, GLchar*))                       \
    X(void,   glDeleteProgram,          (GLuint))                                                   \
    X(void,   glUseProgram,             (GLuint))                                                   \
    X(GLint,  glGetUniformLocation,     (GLuint, const GLchar*))                                    \
    X(void,   glUniform1i,              (GLint, GLint))                                             \
    X(void,   glUniform1f,              (GLint, GLfloat))                                           \
    X(void,   glUniform2f,              (GLint, GLfloat, GLfloat))                                  \
    X(void,   glUniform3f,              (GLint, GLfloat, GLfloat, GLfloat))                         \
    X(void,   glUniform4f,              (GLint, GLfloat, GLfloat, GLfloat, GLfloat))                \
    X(void,   glUniformMatrix4fv,       (GLint, GLsizei, GLboolean, const GLfloat*))                \
    X(void,   glActiveTexture,          (GLenum))                                                   \
    X(void,   glGenerateMipmap,         (GLenum))

// Declare `ac_glFoo` pointers, then alias `glFoo` to them so the rest of the
// engine can call plain `glFoo(...)`. The names inside the X-macro list are
// only ever used with # or ##, so these aliases never clobber the list itself.
#define ARCHCRAFT_GL_DECLARE(ret, name, params) extern ret(APIENTRY* ac_##name) params;
ARCHCRAFT_GL_FUNCTIONS(ARCHCRAFT_GL_DECLARE)
#undef ARCHCRAFT_GL_DECLARE

#define glGenVertexArrays ac_glGenVertexArrays
#define glBindVertexArray ac_glBindVertexArray
#define glDeleteVertexArrays ac_glDeleteVertexArrays
#define glGenBuffers ac_glGenBuffers
#define glBindBuffer ac_glBindBuffer
#define glBufferData ac_glBufferData
#define glDeleteBuffers ac_glDeleteBuffers
#define glVertexAttribPointer ac_glVertexAttribPointer
#define glEnableVertexAttribArray ac_glEnableVertexAttribArray
#define glCreateShader ac_glCreateShader
#define glShaderSource ac_glShaderSource
#define glCompileShader ac_glCompileShader
#define glGetShaderiv ac_glGetShaderiv
#define glGetShaderInfoLog ac_glGetShaderInfoLog
#define glDeleteShader ac_glDeleteShader
#define glCreateProgram ac_glCreateProgram
#define glAttachShader ac_glAttachShader
#define glLinkProgram ac_glLinkProgram
#define glGetProgramiv ac_glGetProgramiv
#define glGetProgramInfoLog ac_glGetProgramInfoLog
#define glDeleteProgram ac_glDeleteProgram
#define glUseProgram ac_glUseProgram
#define glGetUniformLocation ac_glGetUniformLocation
#define glUniform1i ac_glUniform1i
#define glUniform1f ac_glUniform1f
#define glUniform2f ac_glUniform2f
#define glUniform3f ac_glUniform3f
#define glUniform4f ac_glUniform4f
#define glUniformMatrix4fv ac_glUniformMatrix4fv
#define glActiveTexture ac_glActiveTexture
#define glGenerateMipmap ac_glGenerateMipmap

// Call once, after a GL context is current. Returns false if the driver is
// missing something we need.
bool glLoadFunctions();
#endif
