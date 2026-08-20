#include "Renderer.hpp"
#include <glad/glad.h>
#include <iostream>

Renderer::Renderer(int width, int height) : width_(width), height_(height) {}
Renderer::~Renderer() { shutdown(); }

void Renderer::init() {
    if (initialized_) return;
    createFullscreenQuad();
    try {
        shader_.load("assets/shaders/fullscreen.vert", "assets/shaders/blackhole.frag");
    } catch (...) {
        shader_.load("../assets/shaders/fullscreen.vert", "../assets/shaders/blackhole.frag");
    }
    initialized_ = true;
}

void Renderer::shutdown() {
    destroyFullscreenQuad();
    initialized_ = false;
}

void Renderer::createFullscreenQuad() {
    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    float dummy[3] = {0,0,0};
    glBufferData(GL_ARRAY_BUFFER, sizeof(dummy), dummy, GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void Renderer::destroyFullscreenQuad() noexcept {
    if (quadVBO_) { glDeleteBuffers(1, &quadVBO_); quadVBO_ = 0; }
    if (quadVAO_) { glDeleteVertexArrays(1, &quadVAO_); quadVAO_ = 0; }
}

void Renderer::setSize(int width, int height) { width_ = width; height_ = height; }

void Renderer::render(const Camera& camera, const BlackHole& blackHole, float timeSeconds, const RenderSettings& settings) {
    const auto& p = blackHole.params();
    shader_.use();
    shader_.setVec2("uResolution", glm::vec2((float)width_, (float)height_));
    shader_.setVec3("uCameraPosition", camera.position());
    shader_.setMat4("uInvView", camera.invViewMatrix());
    shader_.setMat4("uInvProjection", camera.invProjectionMatrix());
    shader_.setFloat("uTime", timeSeconds);

    shader_.setVec3("uBlackHolePosition", p.position);
    shader_.setVec3("uDiskNormal", p.diskNormal);
    shader_.setFloat("uEventHorizonRadius", p.eventHorizonRadius);
    shader_.setFloat("uPhotonSphereRadius", p.photonSphereRadius);
    shader_.setFloat("uDiskInnerRadius", p.diskInnerRadius);
    shader_.setFloat("uDiskOuterRadius", p.diskOuterRadius);
    shader_.setFloat("uGravityStrength", settings.lensingEnabled ? p.gravityStrength : 0.0f);
    shader_.setFloat("uSpin", p.spin);
    shader_.setFloat("uBaseStepSize", p.baseStepSize);
    shader_.setInt("uMaxSteps", settings.maxSteps);
    shader_.setFloat("uEscapeRadius", p.escapeRadius);
    shader_.setFloat("uTemperatureExponent", p.temperatureExponent);
    shader_.setInt("uLensingEnabled", settings.lensingEnabled ? 1 : 0);
    shader_.setInt("uDiskEnabled", settings.diskEnabled ? 1 : 0);
    shader_.setInt("uStarsEnabled", settings.starsEnabled ? 1 : 0);
    shader_.setInt("uDopplerEnabled", settings.dopplerEnabled ? 1 : 0);
    shader_.setInt("uRedshiftEnabled", settings.redshiftEnabled ? 1 : 0);
    shader_.setFloat("uRenderScale", settings.renderScale);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    shader_.unuse();
}
