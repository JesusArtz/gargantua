#include "Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>

Camera::Camera(glm::vec3 position, float yaw, float pitch, float fovDeg, float aspect, float nearPlane, float farPlane)
    : position_(position), yaw_(yaw), pitch_(pitch), fov_(fovDeg), aspect_(aspect), nearPlane_(nearPlane), farPlane_(farPlane),
      defaultPosition_(position), defaultYaw_(yaw), defaultPitch_(pitch), defaultFov_(fovDeg)
{
    updateVectors();
}

void Camera::updateVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front.y = sin(glm::radians(pitch_));
    front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_ = glm::normalize(front);
    right_ = glm::normalize(glm::cross(front_, worldUp_));
    up_    = glm::normalize(glm::cross(right_, front_));
}

void Camera::processKeyboard(int key, float deltaTime) {
    float velocity = movementSpeed_ * deltaTime;
    if (key == 0) return;
    (void)velocity;
}

void Camera::processMouseMovement(float xOffset, float yOffset, bool constrainPitch) {
    xOffset *= mouseSensitivity_;
    yOffset *= mouseSensitivity_;
    yaw_   += xOffset;
    pitch_ += yOffset;
    if (constrainPitch) pitch_ = glm::clamp(pitch_, -89.0f, 89.0f);
    updateVectors();
}

void Camera::processScroll(float yOffset) {
    fov_ -= yOffset;
    fov_ = glm::clamp(fov_, 1.0f, 90.0f);
}

void Camera::lookAt(const glm::vec3& target) {
    glm::vec3 dir = glm::normalize(target - position_);
    pitch_ = glm::degrees(asin(dir.y));
    yaw_ = glm::degrees(atan2(dir.z, dir.x));
    updateVectors();
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position_, position_ + front_, up_);
}
glm::mat4 Camera::projectionMatrix() const {
    return glm::perspective(glm::radians(fov_), aspect_, nearPlane_, farPlane_);
}
glm::mat4 Camera::invViewMatrix() const {
    return glm::inverse(viewMatrix());
}
glm::mat4 Camera::invProjectionMatrix() const {
    return glm::inverse(projectionMatrix());
}

void Camera::reset() {
    position_ = defaultPosition_;
    yaw_ = defaultYaw_;
    pitch_ = defaultPitch_;
    fov_ = defaultFov_;
    updateVectors();
}
