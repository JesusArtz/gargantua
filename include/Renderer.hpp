#pragma once
#include <glm/glm.hpp>
#include "Shader.hpp"
#include "Camera.hpp"
#include "BlackHole.hpp"

struct RenderSettings {
    bool lensingEnabled{true};
    bool diskEnabled{true};
    bool starsEnabled{true};
    bool dopplerEnabled{true};
    bool redshiftEnabled{true};
    float renderScale{1.0f};
    int   maxSteps{180};
    float timeScale{0.5f};
};

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void init();
    void shutdown();

    void setSize(int width, int height);
    void render(const Camera& camera, const BlackHole& blackHole, float timeSeconds,
                const RenderSettings& settings);

    [[nodiscard]] unsigned int quadVAO() const noexcept { return quadVAO_; }
    [[nodiscard]] Shader& shader() noexcept { return shader_; }
    [[nodiscard]] const Shader& shader() const noexcept { return shader_; }

private:
    void createFullscreenQuad();
    void destroyFullscreenQuad() noexcept;
    void ensureLowResFBO(int scaledW, int scaledH);
    void destroyLowResFBO() noexcept;

    int width_;
    int height_;
    unsigned int quadVAO_{0};
    unsigned int quadVBO_{0};
    Shader shader_;
    bool initialized_{false};

    // Fase 16: FBO para renderScale < 1.0 (downscale + upscale)
    unsigned int lowResFBO_{0};
    unsigned int lowResColorTex_{0};
    int lowResW_{0};
    int lowResH_{0};
};
