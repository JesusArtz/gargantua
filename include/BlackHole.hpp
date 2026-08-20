#pragma once
#include <glm/glm.hpp>

struct BlackHoleParams {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 diskNormal{0.0f, 1.0f, 0.0f};
    float mass{1.0f};
    float spin{0.7f};
    float eventHorizonRadius{1.5f};
    float photonSphereRadius{2.6f};
    float diskInnerRadius{2.8f};
    float diskOuterRadius{10.0f};
    float diskTemperature{1.0f};
    float gravityStrength{3.5f};
    float temperatureExponent{0.8f};
    float escapeRadius{30.0f};
    int   maxSteps{180};
    float baseStepSize{0.15f};
};

class BlackHole {
public:
    explicit BlackHole(const BlackHoleParams& params = BlackHoleParams{});
    [[nodiscard]] const BlackHoleParams& params() const noexcept { return params_; }
    [[nodiscard]] BlackHoleParams& params() noexcept { return params_; }

    void setParams(const BlackHoleParams& p) { params_ = p; validate(); }
    void validate();

    [[nodiscard]] static bool rayIntersectsSphere(const glm::vec3& rayOrigin,
                                                  const glm::vec3& rayDir,
                                                  const glm::vec3& sphereCenter,
                                                  float radius,
                                                  float& tNear,
                                                  float& tFar);

    [[nodiscard]] static bool rayIntersectsPlane(const glm::vec3& rayOrigin,
                                                 const glm::vec3& rayDir,
                                                 const glm::vec3& planePoint,
                                                 const glm::vec3& planeNormal,
                                                 float& t);

    [[nodiscard]] static float temperatureAtRadius(float radius,
                                                   float innerRadius,
                                                   float exponent) noexcept;

    [[nodiscard]] static glm::vec3 temperatureToColor(float temp) noexcept;

private:
    BlackHoleParams params_;
};
