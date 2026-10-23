#include "Renderer.hpp"
#include <glad/glad.h>
#include <iostream>
#include <algorithm>

Renderer::Renderer(int width, int height) : width_(width), height_(height) {}
Renderer::~Renderer() { shutdown(); }

void Renderer::loadShaderPair(Shader& target, const std::string& vert, const std::string& frag) {
    try {
        target.load("assets/shaders/" + vert, "assets/shaders/" + frag);
    } catch (...) {
        target.load("../assets/shaders/" + vert, "../assets/shaders/" + frag);
    }
}

void Renderer::init() {
    if (initialized_) return;
    createFullscreenQuad();
    loadShaderPair(shader_,          "fullscreen.vert", "blackhole.frag");
    loadShaderPair(brightShader_,    "fullscreen.vert", "bright.frag");
    loadShaderPair(blurShader_,      "fullscreen.vert", "blur.frag");
    loadShaderPair(compositeShader_, "fullscreen.vert", "composite.frag");
    initialized_ = true;
}

void Renderer::shutdown() {
    destroyTargets();
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

void Renderer::drawFullscreen() const {
    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void Renderer::setSize(int width, int height) { width_ = width; height_ = height; }

void Renderer::createTarget(RenderTarget& rt, int w, int h) {
    rt.w = std::max(1, w);
    rt.h = std::max(1, h);

    glGenFramebuffers(1, &rt.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);

    glGenTextures(1, &rt.tex);
    glBindTexture(GL_TEXTURE_2D, rt.tex);
    // RGBA16F: the disk emits well above 1.0 and the bright pass needs to see it.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, rt.w, rt.h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Renderer: FBO incomplete (" << rt.w << "x" << rt.h << ")\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::destroyTarget(RenderTarget& rt) noexcept {
    if (rt.tex) { glDeleteTextures(1, &rt.tex); rt.tex = 0; }
    if (rt.fbo) { glDeleteFramebuffers(1, &rt.fbo); rt.fbo = 0; }
    rt.w = rt.h = 0;
}

void Renderer::ensureTargets(int sceneW, int sceneH) {
    sceneW = std::max(1, sceneW);
    sceneH = std::max(1, sceneH);
    if (sceneRT_.fbo != 0 && sceneRT_.w == sceneW && sceneRT_.h == sceneH) return;

    destroyTargets();
    createTarget(sceneRT_, sceneW, sceneH);

    // Bloom runs at half the scene resolution
    const int bw = std::max(1, sceneW / 2);
    const int bh = std::max(1, sceneH / 2);
    createTarget(bloomRT_[0], bw, bh);
    createTarget(bloomRT_[1], bw, bh);

    // Anamorphic flare runs at quarter height for long horizontal stretches
    const int fw = std::max(1, sceneW / 2);
    const int fh = std::max(1, sceneH / 4);
    createTarget(flareRT_[0], fw, fh);
    createTarget(flareRT_[1], fw, fh);
}

void Renderer::destroyTargets() noexcept {
    destroyTarget(sceneRT_);
    destroyTarget(bloomRT_[0]);
    destroyTarget(bloomRT_[1]);
    destroyTarget(flareRT_[0]);
    destroyTarget(flareRT_[1]);
}

void Renderer::render(const Camera& camera, const BlackHole& blackHole, float timeSeconds, const RenderSettings& settings) {
    const auto& p = blackHole.params();

    const float scale = std::clamp(settings.renderScale, 0.25f, 1.0f);
    const int sceneW = std::max(1, int(width_ * scale));
    const int sceneH = std::max(1, int(height_ * scale));

    ensureTargets(sceneW, sceneH);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    // --- Pass 1: ray marching into the linear HDR scene target ---
    glBindFramebuffer(GL_FRAMEBUFFER, sceneRT_.fbo);
    glViewport(0, 0, sceneRT_.w, sceneRT_.h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    shader_.use();
    shader_.setVec2("uResolution", glm::vec2((float)sceneRT_.w, (float)sceneRT_.h));
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

    shader_.setInt("uMaxCrossings", std::clamp(settings.maxCrossings, 1, 8));
    shader_.setFloat("uDopplerStrength", settings.dopplerStrength);
    shader_.setInt("uSpectralDoppler", settings.spectralDoppler ? 1 : 0);
    shader_.setInt("uVolumetricDisk", settings.volumetricDisk ? 1 : 0);
    shader_.setFloat("uDiskDensity", settings.diskDensity);
    shader_.setFloat("uDiskThickness", settings.diskThickness);
    shader_.setFloat("uDiskBrightness", settings.diskBrightness);
    shader_.setFloat("uDiskTurbulence", settings.diskTurbulence);
    shader_.setFloat("uPhotonRingBoost", settings.photonRingBoost);

    shader_.setInt("uPlanetEnabled", p.planetEnabled ? 1 : 0);
    shader_.setFloat("uPlanetRadius", p.planetRadius);
    shader_.setFloat("uPlanetOrbitRadius", p.planetOrbitRadius);
    shader_.setFloat("uPlanetOrbitSpeed", p.planetOrbitSpeed);

    drawFullscreen();
    shader_.unuse();

    // --- Pass 2: bright extraction + separable Gaussian ping-pong ---
    if (settings.bloomEnabled || settings.anamorphicFlare) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomRT_[0].fbo);
        glViewport(0, 0, bloomRT_[0].w, bloomRT_[0].h);
        glClear(GL_COLOR_BUFFER_BIT);

        brightShader_.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneRT_.tex);
        brightShader_.setInt("uScene", 0);
        brightShader_.setFloat("uThreshold", settings.bloomThreshold);
        brightShader_.setFloat("uSoftKnee", settings.bloomSoftKnee);
        drawFullscreen();
        brightShader_.unuse();

        if (settings.bloomEnabled) {
            const float texelX = 1.0f / float(bloomRT_[0].w);
            const float texelY = 1.0f / float(bloomRT_[0].h);
            const int passes = std::clamp(settings.bloomPasses, 1, 12);

            blurShader_.use();
            blurShader_.setInt("uSource", 0);
            for (int i = 0; i < passes; ++i) {
                const float spread = 1.0f + float(i);

                // Horizontal: [0] -> [1]
                glBindFramebuffer(GL_FRAMEBUFFER, bloomRT_[1].fbo);
                glViewport(0, 0, bloomRT_[1].w, bloomRT_[1].h);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, bloomRT_[0].tex);
                blurShader_.setVec2("uTexelDir", glm::vec2(texelX * spread, 0.0f));
                drawFullscreen();

                // Vertical: [1] -> [0]
                glBindFramebuffer(GL_FRAMEBUFFER, bloomRT_[0].fbo);
                glViewport(0, 0, bloomRT_[0].w, bloomRT_[0].h);
                glBindTexture(GL_TEXTURE_2D, bloomRT_[1].tex);
                blurShader_.setVec2("uTexelDir", glm::vec2(0.0f, texelY * spread));
                drawFullscreen();
            }
            blurShader_.unuse();
        }

        // --- Anamorphic Flare Horizontal Streak Pass ---
        if (settings.anamorphicFlare) {
            glBindFramebuffer(GL_FRAMEBUFFER, flareRT_[0].fbo);
            glViewport(0, 0, flareRT_[0].w, flareRT_[0].h);
            glClear(GL_COLOR_BUFFER_BIT);

            // Copy initial bright pass to flare RT
            blurShader_.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, bloomRT_[0].tex);
            blurShader_.setInt("uSource", 0);
            blurShader_.setVec2("uTexelDir", glm::vec2(0.0f, 0.0f));
            drawFullscreen();

            const float flareTexelX = 1.0f / float(flareRT_[0].w);
            const int flarePasses = 5;
            for (int i = 0; i < flarePasses; ++i) {
                float spread = std::pow(2.2f, float(i + 1));

                // Streak horizontally: [0] -> [1]
                glBindFramebuffer(GL_FRAMEBUFFER, flareRT_[1].fbo);
                glViewport(0, 0, flareRT_[1].w, flareRT_[1].h);
                glBindTexture(GL_TEXTURE_2D, flareRT_[0].tex);
                blurShader_.setVec2("uTexelDir", glm::vec2(flareTexelX * spread, 0.0f));
                drawFullscreen();

                // Small vertical dispersion & ping-pong back: [1] -> [0]
                glBindFramebuffer(GL_FRAMEBUFFER, flareRT_[0].fbo);
                glViewport(0, 0, flareRT_[0].w, flareRT_[0].h);
                glBindTexture(GL_TEXTURE_2D, flareRT_[1].tex);
                blurShader_.setVec2("uTexelDir", glm::vec2(flareTexelX * spread * 0.5f, 0.0f));
                drawFullscreen();
            }
            blurShader_.unuse();
        }
    }

    // --- Pass 3: composite to the default framebuffer (also the upscale) ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    compositeShader_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneRT_.tex);
    compositeShader_.setInt("uScene", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomRT_[0].tex);
    compositeShader_.setInt("uBloom", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, flareRT_[0].tex);
    compositeShader_.setInt("uFlare", 2);

    compositeShader_.setInt("uBloomEnabled", settings.bloomEnabled ? 1 : 0);
    compositeShader_.setFloat("uBloomIntensity", settings.bloomIntensity);
    compositeShader_.setInt("uFlareEnabled", settings.anamorphicFlare ? 1 : 0);
    compositeShader_.setFloat("uFlareIntensity", settings.anamorphicIntensity);
    compositeShader_.setFloat("uChromaticAberration", settings.chromaticAberration);
    compositeShader_.setFloat("uFilmGrain", settings.filmGrain);
    compositeShader_.setFloat("uExposure", settings.exposure);
    compositeShader_.setFloat("uVignette", settings.vignette);
    compositeShader_.setFloat("uTime", timeSeconds);

    drawFullscreen();
    compositeShader_.unuse();

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glEnable(GL_BLEND);
}
