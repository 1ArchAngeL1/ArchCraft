#pragma once
// ---------------------------------------------------------------------------
// A mesh is vertex data living in GPU memory, plus a description of its layout.
//
//   VBO (Vertex Buffer Object) - the bytes. Just a blob on the GPU.
//   VAO (Vertex Array Object)  - the *recipe*: "bytes 0..11 are a vec3 called
//                                position, bytes 12..19 are a vec2 called uv".
//
// Uploading is expensive; drawing an already-uploaded mesh is nearly free.
// That asymmetry is the single most important fact about voxel rendering, and
// it is why we build one mesh per chunk instead of per block. See docs/03.
// ---------------------------------------------------------------------------
#include <initializer_list>

#include "gfx/gl.h"

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    // `layout` lists the float-count of each attribute in order, e.g. {3,2,1}
    // means location 0 is a vec3, location 1 is a vec2, location 2 is a float.
    // `dynamic` hints that the data will be re-uploaded often.
    void upload(const void* data, size_t byteSize, int vertexCount,
                std::initializer_list<int> layout, bool dynamic = false);

    void draw(GLenum primitive = GL_TRIANGLES) const;
    void destroy();

    bool empty() const { return vertexCount_ == 0; }
    int vertexCount() const { return vertexCount_; }

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    int vertexCount_ = 0;
};
