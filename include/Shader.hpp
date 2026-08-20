#pragma once
#include <string>
#include <string_view>
#include <glm/glm.hpp>

class Shader {
public:
    Shader() = default;
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void load(const std::string& vertexPath, const std::string& fragmentPath);
    void use() const;
    void unuse() const;
    [[nodiscard]] bool isValid() const noexcept { return id_ != 0; }
    [[nodiscard]] unsigned int id() const noexcept { return id_; }

    void setBool(std::string_view name, bool value) const;
    void setInt(std::string_view name, int value) const;
    void setFloat(std::string_view name, float value) const;
    void setVec2(std::string_view name, const glm::vec2& v) const;
    void setVec2(std::string_view name, float x, float y) const;
    void setVec3(std::string_view name, const glm::vec3& v) const;
    void setVec3(std::string_view name, float x, float y, float z) const;
    void setVec4(std::string_view name, const glm::vec4& v) const;
    void setMat4(std::string_view name, const glm::mat4& m) const;

    static std::string readFile(const std::string& path);
    static void checkCompileErrors(unsigned int shader, std::string_view type);
    static void checkLinkErrors(unsigned int program);

private:
    unsigned int id_{0};
    void destroy() noexcept;
};
