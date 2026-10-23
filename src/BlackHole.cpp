#include "BlackHole.hpp"
#include <glm/gtc/constants.hpp>
#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>

BlackHole::BlackHole(const BlackHoleParams& params) : params_(params) { validate(); }

void BlackHole::validate() {
    params_.spin = glm::clamp(params_.spin, 0.0f, 1.0f);
    if (params_.diskInnerRadius < params_.eventHorizonRadius + 0.1f)
        params_.diskInnerRadius = params_.eventHorizonRadius + 0.1f;
    if (params_.diskOuterRadius <= params_.diskInnerRadius)
        params_.diskOuterRadius = params_.diskInnerRadius + 2.0f;
    params_.photonSphereRadius = glm::max(params_.photonSphereRadius, params_.eventHorizonRadius * 1.3f);
    params_.diskHeight = glm::clamp(params_.diskHeight, 0.01f, 2.0f);
    params_.photonRingIntensity = glm::max(0.0f, params_.photonRingIntensity);
    params_.planetRadius = glm::clamp(params_.planetRadius, 0.05f, 5.0f);
    params_.planetOrbitRadius = glm::max(params_.diskInnerRadius + 1.0f, params_.planetOrbitRadius);
    if (glm::length(params_.diskNormal) < 0.001f) params_.diskNormal = glm::vec3(0,1,0);
    params_.diskNormal = glm::normalize(params_.diskNormal);
}

bool BlackHole::rayIntersectsSphere(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                    const glm::vec3& sphereCenter, float radius,
                                    float& tNear, float& tFar) {
    glm::vec3 oc = rayOrigin - sphereCenter;
    float b = glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - radius*radius;
    float disc = b*b - c;
    if (disc < 0) return false;
    float s = std::sqrt(disc);
    tNear = -b - s;
    tFar  = -b + s;
    return tFar >= 0;
}

bool BlackHole::rayIntersectsPlane(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                   const glm::vec3& planePoint, const glm::vec3& planeNormal,
                                   float& t) {
    float denom = glm::dot(planeNormal, rayDir);
    if (std::abs(denom) < 1e-6f) return false;
    t = glm::dot(planePoint - rayOrigin, planeNormal) / denom;
    return t >= 0;
}

float BlackHole::temperatureAtRadius(float radius, float innerRadius, float exponent) noexcept {
    if (radius <= 0.0f || innerRadius <= 0.0f) return 0.0f;
    float r = innerRadius / radius;
    return std::pow(glm::clamp(r, 0.0f, 1.0f), exponent);
}

// Espejo exacto de temperatureToColor() en assets/shaders/blackhole.frag: los
// tests de CPU solo valen si ambas rampas coinciden. Si cambia una, cambia la otra.
// Paleta Interstellar: ambar profundo -> naranja -> blanco calido; nunca azul.
glm::vec3 BlackHole::temperatureToColor(float temp) noexcept {
    temp = glm::clamp(temp, 0.0f, 1.0f);
    const glm::vec3 c0(0.28f, 0.09f, 0.03f);
    const glm::vec3 c1(0.95f, 0.38f, 0.09f);
    const glm::vec3 c2(1.00f, 0.72f, 0.34f);
    const glm::vec3 c3(1.00f, 0.95f, 0.85f);
    if (temp < 0.33f) return glm::mix(c0, c1, temp / 0.33f);
    if (temp < 0.70f) return glm::mix(c1, c2, (temp - 0.33f) / 0.37f);
    return glm::mix(c2, c3, (temp - 0.70f) / 0.30f);
}
