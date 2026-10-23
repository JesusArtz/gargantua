#pragma once
#include <string>
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
    int   maxSteps{280};
    float timeScale{0.5f};

    // Look Interstellar / DNGR & Volumetrics
    int   maxCrossings{4};        // imagenes secundaria y terciaria del disco
    float dopplerStrength{0.10f}; // 0 = look pelicula, 1 = beaming fisico
    bool  spectralDoppler{false}; // shift de color espectral
    bool  volumetricDisk{true};   // disco volumetrico 3D continuo
    float diskDensity{3.0f};      // densidad optica del gas
    float diskThickness{0.18f};   // grosor del perfil volumetrico
    float diskBrightness{2.4f};
    float diskTurbulence{0.75f};
    float photonRingBoost{1.8f};  // realce del anillo de fotones fino

    // Post-proceso HDR & Optica de Cine (IMAX)
    bool  bloomEnabled{true};
    float bloomThreshold{1.20f};
    float bloomSoftKnee{0.50f};
    float bloomIntensity{0.40f};
    int   bloomPasses{4};
    bool  anamorphicFlare{true};
    float anamorphicIntensity{0.55f};
    float chromaticAberration{0.0035f};
    float filmGrain{0.022f};
    float exposure{0.95f};
    float vignette{0.35f};
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
    // Color target + its framebuffer. All of them are RGBA16F so the disk can
    // carry values above 1.0 until the composite pass tone maps.
    struct RenderTarget {
        unsigned int fbo{0};
        unsigned int tex{0};
        int w{0};
        int h{0};
    };

    void createFullscreenQuad();
    void destroyFullscreenQuad() noexcept;
    void drawFullscreen() const;

    void loadShaderPair(Shader& target, const std::string& vert, const std::string& frag);

    void ensureTargets(int sceneW, int sceneH);
    void destroyTargets() noexcept;
    static void createTarget(RenderTarget& rt, int w, int h);
    static void destroyTarget(RenderTarget& rt) noexcept;

    int width_;
    int height_;
    unsigned int quadVAO_{0};
    unsigned int quadVBO_{0};
    Shader shader_;            // ray marching del agujero negro
    Shader brightShader_;      // extraccion de brillos
    Shader blurShader_;        // gaussiano separable
    Shader compositeShader_;   // bloom + ACES + vignette + gamma
    bool initialized_{false};

    RenderTarget sceneRT_;     // escena HDR (a renderScale)
    RenderTarget bloomRT_[2];  // ping-pong a media resolucion de sceneRT_
    RenderTarget flareRT_[2];  // anamorphic streak (horizontal)
};
