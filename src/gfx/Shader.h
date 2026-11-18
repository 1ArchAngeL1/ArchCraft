#pragma once
// ---------------------------------------------------------------------------
// A shader program = a vertex shader + a fragment shader, compiled by the GPU
// driver and linked together.
//
//   vertex shader   : runs once per vertex. Decides WHERE things land on screen.
//   fragment shader : runs once per covered pixel. Decides WHAT COLOUR it is.
//
// Between them the GPU rasterises triangles and interpolates every output of
// the vertex shader across the triangle's surface. That interpolation is why
// we can compute a value at 3 corners and get a smooth gradient for free.
// ---------------------------------------------------------------------------
#include <string>
#include <unordered_map>

#include "gfx/gl.h"
#include "math/Mat4.h"

class Shader {
public:
    ~Shader();

    Shader() = default;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Reads, compiles and links two GLSL files. On failure returns false and
    // prints the driver's compile log (which is genuinely useful - read it).
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

    void bind() const { glUseProgram(program_); }

    // Uniforms: per-draw-call constants, the same for every vertex/fragment.
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec2(const std::string& name, float x, float y);
    void setVec3(const std::string& name, Vec3 v);
    void setVec4(const std::string& name, float x, float y, float z, float w);
    void setMat4(const std::string& name, const Mat4& m);

    bool valid() const { return program_ != 0; }

private:
    // Uniform locations never change for a linked program, and
    // glGetUniformLocation is a string lookup inside the driver, so we cache.
    GLint uniformLocation(const std::string& name);

    GLuint program_ = 0;
    std::unordered_map<std::string, GLint> uniformCache_;
};
