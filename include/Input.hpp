#pragma once
#include <GLFW/glfw3.h>
#include "Camera.hpp"

struct InputState {
    bool wireframe{false};
    bool vsync{true};
    bool fullscreen{false};
    int windowedX{100}, windowedY{100}, windowedW{1280}, windowedH{720};
};

class Input {
public:
    static void init(GLFWwindow* window, Camera& camera, InputState& state);
    static void processContinuous(GLFWwindow* window, Camera& camera, float deltaTime);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

    static Camera* camera;
    static InputState* state;
    static GLFWwindow* windowPtr;
    static bool firstMouse;
    static double lastX, lastY;
    static bool mouseCaptured;
};
