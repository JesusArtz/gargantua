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

glm::vec3 BlackHole::temperatureToColor(float temp) noexcept {
    temp = glm::clamp(temp, 0.0f, 1.0f);
    if (temp < 0.25f) {
        float t = temp / 0.25f;
        return glm::mix(glm::vec3(0.12f, 0.02f, 0.02f), glm::vec3(0.8f, 0.12f, 0.02f), t);
    } else if (temp < 0.5f) {
        float t = (temp - 0.25f)/0.25f;
        return glm::mix(glm::vec3(0.8f, 0.12f, 0.02f), glm::vec3(1.0f, 0.45f, 0.1f), t);
    } else if (temp < 0.75f) {
        float t = (temp - 0.5f)/0.25f;
        return glm::mix(glm::vec3(1.0f, 0.45f, 0.1f), glm::vec3(1.0f, 0.85f, 0.4f), t);
    } else {
        float t = (temp - 0.75f)/0.25f;
        return glm::mix(glm::vec3(1.0f, 0.85f, 0.4f), glm::vec3(1.0f, 1.0f, 0.95f), t);
    }
}
