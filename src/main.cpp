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
              << "  F2 - Stars toggle  F3 - toggle UI  \n";
}

int main() {
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    int initialW = 1280, initialH = 720;
    GLFWwindow* window = glfwCreateWindow(initialW, initialH, "Gargantua Renderer - Black Hole", nullptr, nullptr);
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

    Camera camera(glm::vec3(0.0f, 4.5f, 13.0f), -90.0f, -18.0f, 45.0f, (float)initialW/initialH);
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
    // true = instala callbacks y encadena los previos de Input (previene que las barras no respondan)
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    BlackHoleParams bhParams;
    bhParams.position = glm::vec3(0,0,0);
    bhParams.mass = 1.0f;
    bhParams.spin = 0.7f;
    bhParams.eventHorizonRadius = 1.5f;
    bhParams.photonSphereRadius = 2.6f;
    bhParams.diskInnerRadius = 2.8f;
    bhParams.diskOuterRadius = 10.0f;
    bhParams.gravityStrength = 3.2f;
    bhParams.baseStepSize = 0.18f;
    bhParams.maxSteps = 180;
    BlackHole blackHole(bhParams);
    gBlackHolePtr = &blackHole;

    Renderer renderer(initialW, initialH);
    try { renderer.init(); } catch (const std::exception& e) {
        std::cerr << "Renderer init failed: " << e.what() << "\n"; return -1;
    }

    gSettings.maxSteps = bhParams.maxSteps;

    printControls();

    auto lastTime = std::chrono::high_resolution_clock::now();
    float totalTime = 0.0f;
    int frameCount = 0;
    float fpsAccum = 0.0f;
    double lastFpsUpdate = glfwGetTime();
    float currentFps = 0.0f;
    bool showUI = true;

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

            ImGui::Begin("Gargantua - Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("FPS: %.1f", currentFps);
            ImGui::Text("Camera: (%.2f, %.2f, %.2f)", camera.position().x, camera.position().y, camera.position().z);
            ImGui::Text("Yaw %.1f Pitch %.1f FOV %.1f", camera.yaw(), camera.pitch(), camera.fov());
            ImGui::Separator();
            ImGui::Text("Black Hole");
            float mass = blackHole.params().mass;
            if (ImGui::SliderFloat("Mass", &mass, 0.1f, 3.0f)) blackHole.params().mass = mass;
            float spin = blackHole.params().spin;
            if (ImGui::SliderFloat("Spin", &spin, 0.0f, 0.98f)) blackHole.params().spin = spin;
            float grav = blackHole.params().gravityStrength;
            if (ImGui::SliderFloat("Gravity", &grav, 0.0f, 8.0f)) blackHole.params().gravityStrength = grav;
            float inner = blackHole.params().diskInnerRadius;
            if (ImGui::SliderFloat("Inner Radius", &inner, 1.6f, 5.0f)) blackHole.params().diskInnerRadius = inner;
            float outer = blackHole.params().diskOuterRadius;
            if (ImGui::SliderFloat("Outer Radius", &outer, 6.0f, 16.0f)) blackHole.params().diskOuterRadius = outer;
            int steps = gSettings.maxSteps;
            if (ImGui::SliderInt("Ray Steps", &steps, 32, 300)) gSettings.maxSteps = steps;
            float stepSz = blackHole.params().baseStepSize;
            if (ImGui::SliderFloat("Step Size", &stepSz, 0.05f, 0.5f)) blackHole.params().baseStepSize = stepSz;
            ImGui::Separator();
            ImGui::Checkbox("Lensing", &gSettings.lensingEnabled);
            ImGui::Checkbox("Disk", &gSettings.diskEnabled);
            ImGui::Checkbox("Stars", &gSettings.starsEnabled);
            ImGui::Checkbox("Doppler", &gSettings.dopplerEnabled);
            ImGui::Checkbox("Redshift", &gSettings.redshiftEnabled);
            ImGui::Separator();
            ImGui::SliderFloat("Render Scale", &gSettings.renderScale, 0.25f, 1.0f);
            {
                int sw = int(fbW * gSettings.renderScale);
                int sh = int(fbH * gSettings.renderScale);
                ImGui::Text("Effective: %dx%d %s", sw, sh, gSettings.renderScale < 0.999f ? "(FBO downscale)" : "(native)");
            }
            ImGui::SliderFloat("Time Scale", &gSettings.timeScale, 0.0f, 2.0f);
            if (ImGui::Button("Reset Camera (R)")) camera.reset();
            ImGui::SameLine();
            if (ImGui::Button("Reset BH")) { blackHole = BlackHole(BlackHoleParams{}); gSettings.maxSteps = blackHole.params().maxSteps; }
            ImGui::Separator();
            ImGui::Text("Keys: 1/2 Lensing 3/4 Disk 5/6 Doppler 7/8 Redshift");
            ImGui::Text("R Reset  F11 Fullscreen  F1 Capture Mouse  F2 Stars  F3 UI");
            ImGui::Checkbox("VSync", &inputState.vsync);
            if (inputState.vsync) glfwSwapInterval(1); else glfwSwapInterval(0);
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
