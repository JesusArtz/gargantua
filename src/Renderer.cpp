#include "Renderer.hpp"
#include <glad/glad.h>
#include <iostream>
#include <algorithm>

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
    destroyLowResFBO();
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

void Renderer::ensureLowResFBO(int scaledW, int scaledH) {
    scaledW = std::max(1, scaledW);
    scaledH = std::max(1, scaledH);
    if (lowResFBO_ != 0 && lowResW_ == scaledW && lowResH_ == scaledH) return;
    destroyLowResFBO();
    lowResW_ = scaledW;
    lowResH_ = scaledH;

    glGenFramebuffers(1, &lowResFBO_);
    glBindFramebuffer(GL_FRAMEBUFFER, lowResFBO_);

    glGenTextures(1, &lowResColorTex_);
    glBindTexture(GL_TEXTURE_2D, lowResColorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, lowResW_, lowResH_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lowResColorTex_, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Renderer: low-res FBO incomplete\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::destroyLowResFBO() noexcept {
    if (lowResColorTex_) { glDeleteTextures(1, &lowResColorTex_); lowResColorTex_ = 0; }
    if (lowResFBO_) { glDeleteFramebuffers(1, &lowResFBO_); lowResFBO_ = 0; }
    lowResW_ = lowResH_ = 0;
}

void Renderer::render(const Camera& camera, const BlackHole& blackHole, float timeSeconds, const RenderSettings& settings) {
    const auto& p = blackHole.params();

    float scale = std::clamp(settings.renderScale, 0.25f, 1.0f);
    bool useLowRes = scale < 0.999f;
    int rw = width_;
    int rh = height_;
    int scaledW = useLowRes ? std::max(1, int(width_ * scale)) : width_;
    int scaledH = useLowRes ? std::max(1, int(height_ * scale)) : height_;

    auto drawBlackHole = [&](int resW, int resH) {
        shader_.use();
        shader_.setVec2("uResolution", glm::vec2((float)resW, (float)resH));
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
        shader_.setFloat("uRenderScale", scale);

        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        shader_.unuse();
    };

    if (useLowRes) {
        ensureLowResFBO(scaledW, scaledH);
        glBindFramebuffer(GL_FRAMEBUFFER, lowResFBO_);
        glViewport(0, 0, scaledW, scaledH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        drawBlackHole(scaledW, scaledH);

        // Upscale to default framebuffer with linear filtering (GL_LINEAR on texture + blit)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, lowResFBO_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glViewport(0, 0, rw, rh);
        glBlitFramebuffer(0, 0, scaledW, scaledH, 0, 0, rw, rh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else {
        // Destroy low-res FBO if not needed to save VRAM
        if (lowResFBO_ != 0) destroyLowResFBO();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, rw, rh);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        drawBlackHole(rw, rh);
    }
}
