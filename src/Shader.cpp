#include "Shader.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
    load(vertexPath, fragmentPath);
}

Shader::~Shader() { destroy(); }

Shader::Shader(Shader&& other) noexcept : id_(other.id_) { other.id_ = 0; }
Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) { destroy(); id_ = other.id_; other.id_ = 0; }
    return *this;
}

void Shader::destroy() noexcept {
    if (id_ != 0) { glDeleteProgram(id_); id_ = 0; }
}

std::string Shader::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("Shader: cannot open file: " + path);
    std::stringstream ss; ss << file.rdbuf();
    return ss.str();
}

void Shader::checkCompileErrors(unsigned int shader, std::string_view type) {
    int success; char infoLog[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << "Shader compilation error (" << type << "):\n" << infoLog << "\n";
        throw std::runtime_error(std::string("Shader compile error: ") + std::string(type));
    }
}
void Shader::checkLinkErrors(unsigned int program) {
    int success; char infoLog[1024];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        std::cerr << "Shader linking error:\n" << infoLog << "\n";
        throw std::runtime_error("Shader linking error");
    }
}

void Shader::load(const std::string& vertexPath, const std::string& fragmentPath) {
    destroy();
    std::string vCode = readFile(vertexPath);
    std::string fCode = readFile(fragmentPath);
    const char* vSrc = vCode.c_str();
    const char* fSrc = fCode.c_str();

    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vSrc, nullptr);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");

    unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fSrc, nullptr);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");

    id_ = glCreateProgram();
    glAttachShader(id_, vertex);
    glAttachShader(id_, fragment);
    glLinkProgram(id_);
    checkLinkErrors(id_);

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void Shader::use() const { glUseProgram(id_); }
void Shader::unuse() const { glUseProgram(0); }

void Shader::setBool(std::string_view name, bool value) const {
    glUniform1i(glGetUniformLocation(id_, name.data()), static_cast<int>(value));
}
void Shader::setInt(std::string_view name, int value) const {
    glUniform1i(glGetUniformLocation(id_, name.data()), value);
}
void Shader::setFloat(std::string_view name, float value) const {
    glUniform1f(glGetUniformLocation(id_, name.data()), value);
}
void Shader::setVec2(std::string_view name, const glm::vec2& v) const {
    glUniform2fv(glGetUniformLocation(id_, name.data()), 1, glm::value_ptr(v));
}
void Shader::setVec2(std::string_view name, float x, float y) const {
    glUniform2f(glGetUniformLocation(id_, name.data()), x, y);
}
void Shader::setVec3(std::string_view name, const glm::vec3& v) const {
    glUniform3fv(glGetUniformLocation(id_, name.data()), 1, glm::value_ptr(v));
}
void Shader::setVec3(std::string_view name, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(id_, name.data()), x, y, z);
}
void Shader::setVec4(std::string_view name, const glm::vec4& v) const {
    glUniform4fv(glGetUniformLocation(id_, name.data()), 1, glm::value_ptr(v));
}
void Shader::setMat4(std::string_view name, const glm::mat4& m) const {
    glUniformMatrix4fv(glGetUniformLocation(id_, name.data()), 1, GL_FALSE, glm::value_ptr(m));
}
