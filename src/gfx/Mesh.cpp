#include "gfx/Mesh.h"

#include <utility>

Mesh::~Mesh() { destroy(); }

Mesh::Mesh(Mesh&& other) noexcept
    : vao_(other.vao_), vbo_(other.vbo_), vertexCount_(other.vertexCount_) {
    other.vao_ = 0;
    other.vbo_ = 0;
    other.vertexCount_ = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        destroy();
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        vertexCount_ = other.vertexCount_;
        other.vao_ = 0;
        other.vbo_ = 0;
        other.vertexCount_ = 0;
    }
    return *this;
}

void Mesh::destroy() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
    vbo_ = 0;
    vertexCount_ = 0;
}

void Mesh::upload(const void* data, size_t byteSize, int vertexCount,
                  std::initializer_list<int> layout, bool dynamic) {
    vertexCount_ = vertexCount;
    if (vertexCount <= 0 || byteSize == 0) return;

    if (!vao_) glGenVertexArrays(1, &vao_);
    if (!vbo_) glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(byteSize), data,
                 dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    // Total floats per vertex = the stride the GPU steps by.
    int strideFloats = 0;
    for (int components : layout) strideFloats += components;
    const GLsizei stride = static_cast<GLsizei>(strideFloats * sizeof(float));

    GLuint location = 0;
    size_t offsetFloats = 0;
    for (int components : layout) {
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, components, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(offsetFloats * sizeof(float)));
        offsetFloats += static_cast<size_t>(components);
        ++location;
    }

    glBindVertexArray(0);
}

void Mesh::draw(GLenum primitive) const {
    if (vertexCount_ <= 0) return;
    // Binding the VAO restores the whole layout + buffer binding in one call.
    glBindVertexArray(vao_);
    glDrawArrays(primitive, 0, vertexCount_);
}
