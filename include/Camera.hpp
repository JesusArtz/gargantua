#pragma once
#include <glm/glm.hpp>

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 3.0f, 10.0f),
           float yaw = -90.0f,
           float pitch = -15.0f,
           float fovDeg = 45.0f,
           float aspect = 16.0f/9.0f,
           float nearPlane = 0.1f,
           float farPlane = 100.0f);

    [[nodiscard]] const glm::vec3& position() const noexcept { return position_; }
    [[nodiscard]] const glm::vec3& front() const noexcept { return front_; }
    [[nodiscard]] const glm::vec3& up() const noexcept { return up_; }
    [[nodiscard]] const glm::vec3& right() const noexcept { return right_; }
    [[nodiscard]] float yaw() const noexcept { return yaw_; }
    [[nodiscard]] float pitch() const noexcept { return pitch_; }
    [[nodiscard]] float fov() const noexcept { return fov_; }
    [[nodiscard]] float aspect() const noexcept { return aspect_; }

    void setPosition(const glm::vec3& p) noexcept { position_ = p; }
    void setYaw(float y) noexcept { yaw_ = y; updateVectors(); }
    void setPitch(float p) noexcept { pitch_ = p; updateVectors(); }
    void setFov(float f) noexcept { fov_ = glm::clamp(f, 1.0f, 90.0f); }
    void setAspect(float a) noexcept { aspect_ = a; }

    void processKeyboard(int key, float deltaTime);
    void processMouseMovement(float xOffset, float yOffset, bool constrainPitch = true);
    void processScroll(float yOffset);
    void lookAt(const glm::vec3& target);

    [[nodiscard]] glm::mat4 viewMatrix() const;
    [[nodiscard]] glm::mat4 projectionMatrix() const;
    [[nodiscard]] glm::mat4 invViewMatrix() const;
    [[nodiscard]] glm::mat4 invProjectionMatrix() const;

    void reset();

private:
    void updateVectors();

    glm::vec3 position_;
    glm::vec3 front_;
    glm::vec3 up_;
    glm::vec3 right_;
    glm::vec3 worldUp_{0.0f, 1.0f, 0.0f};

    float yaw_;
    float pitch_;
    float fov_;
    float aspect_;
    float nearPlane_;
    float farPlane_;

    glm::vec3 defaultPosition_;
    float defaultYaw_;
    float defaultPitch_;
    float defaultFov_;

    float movementSpeed_{5.0f};
    float mouseSensitivity_{0.1f};
};
