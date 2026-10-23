#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <iostream>
#include <chrono>
#include <string>

#include "Shader.hpp"
#include "Camera.hpp"
#include "BlackHole.hpp"
#include "Renderer.hpp"
#include "Input.hpp"

RenderSettings gSettings;
BlackHole* gBlackHolePtr = nullptr;
Camera* gCameraPtr = nullptr;

void APIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                              GLsizei length, const GLchar* message, const void* userParam) {
    (void)source; (void)type; (void)id; (void)severity; (void)length; (void)userParam;
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    std::cerr << "[GL DEBUG] " << message << "\n";
}

void printControls() {
    std::cout << "Controls:\n"
              << "  W/A/S/D - move  Q/E - up/down  Mouse - look (hold F1 to capture)\n"
              << "  Scroll - FOV zoom   R - reset camera   ESC - exit   F11 - fullscreen\n"
              << "  1/2 - Lensing OFF/ON  3/4 - Disk OFF/ON  5/6 - Doppler OFF/ON  7/8 - Redshift OFF/ON\n"
              << "  F2 - Stars toggle  F3 - toggle UI  F4 - Cinematic preset  F5 - Physical preset\n";
}

// Encuadre y parametros que reproducen fielmente el Gargantua de Interstellar:
// camara casi en el plano del disco, disco volumetrico con gas 3D y filamentos,
// anillo de fotones ultra-fino, flare anamorfico horizontal y tono filmico ACES.
void applyCinematicPreset(Camera& cam, BlackHole& bh, RenderSettings& s) {
    cam.setPosition(glm::vec3(0.0f, 2.2f, 30.0f));
    cam.lookAt(glm::vec3(0.0f));
    cam.setFov(50.0f);

    auto& p = bh.params();
    p.spin = 0.65f;
    p.eventHorizonRadius = 1.35f;
    p.photonSphereRadius = 2.4f;
    p.diskInnerRadius = 2.6f;
    p.diskOuterRadius = 14.5f;
    p.diskHeight = 0.18f;
    p.gravityStrength = 2.8f;
    p.baseStepSize = 0.15f;
    p.temperatureExponent = 0.8f;
    p.escapeRadius = 55.0f;
    p.maxSteps = 280;
    p.photonRingIntensity = 2.4f;
    p.planetEnabled = true;
    p.planetRadius = 0.42f;
    p.planetOrbitRadius = 12.0f;
    p.planetOrbitSpeed = 0.15f;

    s.maxSteps = 280;
    s.maxCrossings = 4;
    s.volumetricDisk = true;
    s.diskDensity = 3.2f;
    s.diskThickness = 0.18f;
    s.diskBrightness = 2.6f;
    s.diskTurbulence = 0.72f;
    s.photonRingBoost = 1.8f;
    s.dopplerStrength = 0.12f;
    s.spectralDoppler = false;

    s.lensingEnabled = true;
    s.diskEnabled = true;
    s.starsEnabled = true;
    s.dopplerEnabled = true;
    s.redshiftEnabled = true;

    s.bloomEnabled = true;
    s.bloomThreshold = 1.20f;
    s.bloomSoftKnee = 0.50f;
    s.bloomIntensity = 0.42f;
    s.bloomPasses = 4;

    s.anamorphicFlare = true;
    s.anamorphicIntensity = 0.55f;
    s.chromaticAberration = 0.0035f;
    s.filmGrain = 0.022f;
    s.exposure = 0.95f;
    s.vignette = 0.35f;
}

// Lo que se veria sin las concesiones esteticas: beaming completo (un lado
// cegador, el otro casi negro), shift espectral azul/rojo, gas turbulento.
void applyPhysicalPreset(Camera& cam, BlackHole& bh, RenderSettings& s) {
    applyCinematicPreset(cam, bh, s);
    cam.setPosition(glm::vec3(0.0f, 15.0f, 28.0f));
    cam.lookAt(glm::vec3(0.0f));
    bh.params().diskOuterRadius = 14.5f;
    bh.params().escapeRadius = 60.0f;
    s.dopplerStrength = 1.0f;
    s.spectralDoppler = true;
    s.diskTurbulence = 0.85f;
    s.diskBrightness = 1.4f;
    s.bloomIntensity = 0.20f;
    s.anamorphicIntensity = 0.25f;
    s.vignette = 0.15f;
}

int main(int argc, char** argv) {
    bool startWithUI = true;
    bool startPhysical = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--no-ui") startWithUI = false;
        else if (arg == "--physical") startPhysical = true;
    }

    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    int initialW = 1280, initialH = 720;
    GLFWwindow* window = glfwCreateWindow(initialW, initialH, "Gargantua Renderer - Interstellar Black Hole", nullptr, nullptr);
    if (!window) { std::cerr << "Window creation failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n"; return -1;
    }
    std::cout << "OpenGL " << glGetString(GL_VERSION) << " GLSL " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";

    int flags; glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(glDebugCallback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
        std::cout << "OpenGL debug callback enabled\n";
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Camera camera(glm::vec3(0.0f, 2.2f, 30.0f), -90.0f, -4.2f, 50.0f, (float)initialW / initialH);
    camera.lookAt(glm::vec3(0,0,0));
    gCameraPtr = &camera;
    InputState inputState;
    Input::init(window, camera, inputState);
    Input::mouseCaptured = false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    BlackHoleParams bhParams;
    bhParams.position = glm::vec3(0,0,0);
    bhParams.mass = 1.0f;
    bhParams.spin = 0.65f;
    bhParams.eventHorizonRadius = 1.35f;
    bhParams.photonSphereRadius = 2.4f;
    bhParams.diskInnerRadius = 2.6f;
    bhParams.diskOuterRadius = 14.5f;
    bhParams.diskHeight = 0.18f;
    bhParams.gravityStrength = 2.8f;
    bhParams.baseStepSize = 0.15f;
    bhParams.maxSteps = 280;
    bhParams.photonRingIntensity = 2.4f;
    bhParams.planetEnabled = true;
    bhParams.planetRadius = 0.42f;
    bhParams.planetOrbitRadius = 12.0f;
    bhParams.planetOrbitSpeed = 0.15f;

    BlackHole blackHole(bhParams);
    gBlackHolePtr = &blackHole;

    Renderer renderer(initialW, initialH);
    try { renderer.init(); } catch (const std::exception& e) {
        std::cerr << "Renderer init failed: " << e.what() << "\n"; return -1;
    }

    if (startPhysical) applyPhysicalPreset(camera, blackHole, gSettings);
    else               applyCinematicPreset(camera, blackHole, gSettings);

    printControls();

    auto lastTime = std::chrono::high_resolution_clock::now();
    float totalTime = 0.0f;
    int frameCount = 0;
    float fpsAccum = 0.0f;
    double lastFpsUpdate = glfwGetTime();
    float currentFps = 0.0f;
    bool showUI = startWithUI;

    while (!glfwWindowShouldClose(window)) {
        auto now = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        totalTime += deltaTime * gSettings.timeScale;
        frameCount++;

        double cur = glfwGetTime();
        fpsAccum += 1.0f;
        if (cur - lastFpsUpdate >= 0.5) {
            currentFps = fpsAccum / float(cur - lastFpsUpdate);
            fpsAccum = 0; lastFpsUpdate = cur;
        }

        glfwPollEvents();
        Input::processContinuous(window, camera, deltaTime);

        if (!ImGui::GetIO().WantCaptureKeyboard) {
            if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) gSettings.lensingEnabled = false;
            if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) gSettings.lensingEnabled = true;
            if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) gSettings.diskEnabled = false;
            if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) gSettings.diskEnabled = true;
            if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) gSettings.dopplerEnabled = false;
            if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) gSettings.dopplerEnabled = true;
            if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) gSettings.redshiftEnabled = false;
            if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) gSettings.redshiftEnabled = true;
        }
        static bool f2Prev=false, f3Prev=false;
        bool f2 = glfwGetKey(window, GLFW_KEY_F2)==GLFW_PRESS;
        if (f2 && !f2Prev) gSettings.starsEnabled = !gSettings.starsEnabled;
        f2Prev = f2;
        bool f3 = glfwGetKey(window, GLFW_KEY_F3)==GLFW_PRESS;
        if (f3 && !f3Prev) showUI = !showUI;
        f3Prev = f3;
        static bool f4Prev=false, f5Prev=false;
        bool f4 = glfwGetKey(window, GLFW_KEY_F4)==GLFW_PRESS;
        if (f4 && !f4Prev) applyCinematicPreset(camera, blackHole, gSettings);
        f4Prev = f4;
        bool f5 = glfwGetKey(window, GLFW_KEY_F5)==GLFW_PRESS;
        if (f5 && !f5Prev) applyPhysicalPreset(camera, blackHole, gSettings);
        f5Prev = f5;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(window,true);

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);

        blackHole.params().maxSteps = gSettings.maxSteps;

        renderer.setSize(fbW, fbH);
        renderer.render(camera, blackHole, totalTime, gSettings);

        if (showUI) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowSize(ImVec2(400, 680), ImGuiCond_FirstUseEver);
            ImGui::Begin("Gargantua - Interstellar Controls", nullptr);

            if (ImGui::Button("Cinematic / Interstellar (F4)", ImVec2(185, 28))) applyCinematicPreset(camera, blackHole, gSettings);
            ImGui::SameLine();
            if (ImGui::Button("Physical Beaming (F5)", ImVec2(185, 28))) applyPhysicalPreset(camera, blackHole, gSettings);

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "FPS: %.1f", currentFps);
            ImGui::SameLine(120);
            ImGui::Text("Cam: (%.1f, %.1f, %.1f)", camera.position().x, camera.position().y, camera.position().z);

            if (ImGui::CollapsingHeader("Black Hole & Kerr Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
                float spin = blackHole.params().spin;
                if (ImGui::SliderFloat("Kerr Spin", &spin, 0.0f, 0.99f)) blackHole.params().spin = spin;
                float grav = blackHole.params().gravityStrength;
                if (ImGui::SliderFloat("Gravity Deflection", &grav, 0.0f, 8.0f)) blackHole.params().gravityStrength = grav;
                float inner = blackHole.params().diskInnerRadius;
                if (ImGui::SliderFloat("Inner Radius (ISCO)", &inner, 1.5f, 5.0f)) blackHole.params().diskInnerRadius = inner;
                float outer = blackHole.params().diskOuterRadius;
                if (ImGui::SliderFloat("Outer Radius", &outer, 6.0f, 25.0f)) blackHole.params().diskOuterRadius = outer;
                ImGui::SliderFloat("Photon Ring Boost", &gSettings.photonRingBoost, 0.0f, 5.0f);
                int steps = gSettings.maxSteps;
                if (ImGui::SliderInt("Ray Steps", &steps, 32, 320)) gSettings.maxSteps = steps;
                float stepSz = blackHole.params().baseStepSize;
                if (ImGui::SliderFloat("Base Step Size", &stepSz, 0.05f, 0.4f)) blackHole.params().baseStepSize = stepSz;
                ImGui::Checkbox("Lensing Enabled", &gSettings.lensingEnabled);
            }

            if (ImGui::CollapsingHeader("3D Volumetric Accretion Disk", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Disk Enabled", &gSettings.diskEnabled);
                ImGui::SameLine();
                ImGui::Checkbox("Volumetric 3D", &gSettings.volumetricDisk);
                ImGui::SliderFloat("Gas Density", &gSettings.diskDensity, 0.5f, 8.0f);
                ImGui::SliderFloat("Disk Scale Height (H0)", &gSettings.diskThickness, 0.04f, 0.60f);
                ImGui::SliderFloat("HDR Brightness", &gSettings.diskBrightness, 0.5f, 6.0f);
                ImGui::SliderFloat("Gas Turbulence", &gSettings.diskTurbulence, 0.0f, 1.0f);
                if (!gSettings.volumetricDisk) {
                    ImGui::SliderInt("Disk Images (Halo)", &gSettings.maxCrossings, 1, 6);
                }
            }

            if (ImGui::CollapsingHeader("Relativistic Doppler & Redshift")) {
                ImGui::Checkbox("Doppler Effect", &gSettings.dopplerEnabled);
                ImGui::SameLine();
                ImGui::Checkbox("Gravitational Redshift", &gSettings.redshiftEnabled);
                ImGui::SliderFloat("Doppler Strength", &gSettings.dopplerStrength, 0.0f, 1.0f);
                ImGui::Checkbox("Spectral Color Shift (Blue/Red)", &gSettings.spectralDoppler);
            }

            if (ImGui::CollapsingHeader("IMAX Camera Optics & HDR Post-Process", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Bloom", &gSettings.bloomEnabled);
                ImGui::SameLine();
                ImGui::Checkbox("Anamorphic Flare", &gSettings.anamorphicFlare);
                ImGui::SliderFloat("Bloom Intensity", &gSettings.bloomIntensity, 0.0f, 2.0f);
                ImGui::SliderFloat("Flare Intensity", &gSettings.anamorphicIntensity, 0.0f, 2.0f);
                ImGui::SliderFloat("Bloom Threshold", &gSettings.bloomThreshold, 0.2f, 3.0f);
                ImGui::SliderFloat("Chromatic Aberration", &gSettings.chromaticAberration, 0.0f, 0.015f);
                ImGui::SliderFloat("70mm Film Grain", &gSettings.filmGrain, 0.0f, 0.08f);
                ImGui::SliderFloat("Exposure", &gSettings.exposure, 0.2f, 3.0f);
                ImGui::SliderFloat("Vignette", &gSettings.vignette, 0.0f, 1.5f);
            }

            if (ImGui::CollapsingHeader("Environment & Celestial Body")) {
                ImGui::Checkbox("Milky Way & Stars", &gSettings.starsEnabled);
                ImGui::Checkbox("Miller's Planet", &blackHole.params().planetEnabled);
                if (blackHole.params().planetEnabled) {
                    ImGui::SliderFloat("Planet Radius", &blackHole.params().planetRadius, 0.1f, 1.5f);
                    ImGui::SliderFloat("Orbit Radius", &blackHole.params().planetOrbitRadius, 5.0f, 22.0f);
                    ImGui::SliderFloat("Orbit Speed", &blackHole.params().planetOrbitSpeed, 0.0f, 0.6f);
                }
            }

            if (ImGui::CollapsingHeader("Render & Display Settings")) {
                ImGui::SliderFloat("Render Scale", &gSettings.renderScale, 0.25f, 1.0f);
                int sw = int(fbW * gSettings.renderScale);
                int sh = int(fbH * gSettings.renderScale);
                ImGui::Text("Ray march: %dx%d | Bloom: %dx%d", sw, sh, sw/2, sh/2);
                ImGui::SliderFloat("Time Scale", &gSettings.timeScale, 0.0f, 2.0f);
                ImGui::Checkbox("VSync", &inputState.vsync);
                if (inputState.vsync) glfwSwapInterval(1); else glfwSwapInterval(0);
                if (ImGui::Button("Reset Camera (R)")) camera.reset();
            }

            ImGui::End();
        } else {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(10,10));
            ImGui::Begin("FPS", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);
            ImGui::Text("FPS %.0f", currentFps);
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
