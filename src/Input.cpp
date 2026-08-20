#include "Input.hpp"
#include <iostream>

Camera* Input::camera = nullptr;
InputState* Input::state = nullptr;
GLFWwindow* Input::windowPtr = nullptr;
bool Input::firstMouse = true;
double Input::lastX = 640;
double Input::lastY = 360;
bool Input::mouseCaptured = false;

void Input::init(GLFWwindow* window, Camera& cam, InputState& s) {
    windowPtr = window;
    camera = &cam;
    state = &s;
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
}

void Input::processContinuous(GLFWwindow* window, Camera& cam, float deltaTime) {
    float vel = 5.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cam.setPosition(cam.position() + cam.front() * vel);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cam.setPosition(cam.position() - cam.front() * vel);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cam.setPosition(cam.position() - cam.right() * vel);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cam.setPosition(cam.position() + cam.right() * vel);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) cam.setPosition(cam.position() - cam.up() * vel);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) cam.setPosition(cam.position() + cam.up() * vel);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        float sprint = 8.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cam.setPosition(cam.position() + cam.front() * sprint);
    }
}

void Input::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode; (void)mods;
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
    if (key == GLFW_KEY_F11) {
        if (!state) return;
        if (!state->fullscreen) {
            state->windowedX = 100; state->windowedY = 100;
            glfwGetWindowSize(window, &state->windowedW, &state->windowedH);
            GLFWmonitor* mon = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(mon);
            glfwSetWindowMonitor(window, mon, 0, 0, mode->width, mode->height, mode->refreshRate);
            state->fullscreen = true;
        } else {
            glfwSetWindowMonitor(window, nullptr, state->windowedX, state->windowedY, state->windowedW, state->windowedH, 0);
            state->fullscreen = false;
        }
    }
    if (key == GLFW_KEY_R && camera) camera->reset();
    if (key == GLFW_KEY_F1) mouseCaptured = !mouseCaptured;
    if (mouseCaptured) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void Input::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    if (!camera) return;
    if (!mouseCaptured) return;
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    double xoff = xpos - lastX;
    double yoff = lastY - ypos;
    lastX = xpos; lastY = ypos;
    camera->processMouseMovement((float)xoff, (float)yoff, true);
}

void Input::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window; (void)xoffset;
    if (camera) camera->processScroll((float)yoffset);
}

void Input::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
    if (camera) camera->setAspect((float)width / (float)height);
}
