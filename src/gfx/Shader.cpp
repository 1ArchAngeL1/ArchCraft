#include "gfx/Shader.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

namespace {

bool readFile(const std::string& path, std::string& out) {
    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "[shader] cannot open %s\n", path.c_str());
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

GLuint compileStage(GLenum type, const std::string& source, const std::string& label) {
    const GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<size_t>(logLength > 1 ? logLength : 1));
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        std::fprintf(stderr, "[shader] %s failed to compile:\n%s\n", label.c_str(), log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

}  // namespace

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertexSource;
    std::string fragmentSource;
    if (!readFile(vertexPath, vertexSource) || !readFile(fragmentPath, fragmentSource)) return false;

    const GLuint vs = compileStage(GL_VERTEX_SHADER, vertexSource, vertexPath);
    if (!vs) return false;
    const GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // Linking checks that the vertex shader's outputs match the fragment
    // shader's inputs, among other things.
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<size_t>(logLength > 1 ? logLength : 1));
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        std::fprintf(stderr, "[shader] link failed:\n%s\n", log.data());
        glDeleteProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    // The stages are baked into the program now; the objects can go.
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (program_) glDeleteProgram(program_);
    program_ = program;
    uniformCache_.clear();
    return true;
}

GLint Shader::uniformLocation(const std::string& name) {
    const auto it = uniformCache_.find(name);
    if (it != uniformCache_.end()) return it->second;
    // -1 means "no such uniform" (or it was optimised out because unused).
    const GLint location = glGetUniformLocation(program_, name.c_str());
    uniformCache_.emplace(name, location);
    return location;
}

void Shader::setInt(const std::string& name, int value) { glUniform1i(uniformLocation(name), value); }
void Shader::setFloat(const std::string& name, float value) { glUniform1f(uniformLocation(name), value); }
void Shader::setVec2(const std::string& name, float x, float y) { glUniform2f(uniformLocation(name), x, y); }
void Shader::setVec3(const std::string& name, Vec3 v) { glUniform3f(uniformLocation(name), v.x, v.y, v.z); }

void Shader::setVec4(const std::string& name, float x, float y, float z, float w) {
    glUniform4f(uniformLocation(name), x, y, z, w);
}

void Shader::setMat4(const std::string& name, const Mat4& m) {
    // GL_FALSE = "do not transpose": our Mat4 is already column-major.
    glUniformMatrix4fv(uniformLocation(name), 1, GL_FALSE, m.m);
}
