#include <iostream>
#include <cassert>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "BlackHole.hpp"
#include "Camera.hpp"

int testsPassed = 0, testsFailed = 0;

#define EXPECT_TRUE(cond) do { if(!(cond)){ std::cerr<<"FAIL: " #cond " at " __FILE__ ":" << __LINE__ << "\n"; testsFailed++; } else testsPassed++; } while(0)
#define EXPECT_NEAR(a,b,eps) EXPECT_TRUE(std::abs((a)-(b)) < (eps))
#define EXPECT_VEC_NEAR(a,b,eps) EXPECT_TRUE(glm::length((a)-(b)) < (eps))

void testRaySphere() {
    std::cout << "== raySphere ==\n";
    glm::vec3 ro(0,0,10), rd(0,0,-1);
    float tNear, tFar;
    bool hit = BlackHole::rayIntersectsSphere(ro, rd, glm::vec3(0), 1.0f, tNear, tFar);
    EXPECT_TRUE(hit);
    EXPECT_NEAR(tNear, 9.0f, 1e-4);
    EXPECT_NEAR(tFar, 11.0f, 1e-4);

    glm::vec3 ro2(0,0,10), rd2(0,1,0);
    hit = BlackHole::rayIntersectsSphere(ro2, rd2, glm::vec3(0), 1.0f, tNear, tFar);
    EXPECT_TRUE(!hit);
}

void testRayPlane() {
    std::cout << "== rayPlane ==\n";
    glm::vec3 ro(0,5,0), rd(0,-1,0);
    float t;
    bool hit = BlackHole::rayIntersectsPlane(ro, rd, glm::vec3(0), glm::vec3(0,1,0), t);
    EXPECT_TRUE(hit);
    EXPECT_NEAR(t, 5.0f, 1e-4);

    glm::vec3 rd2(1,0,0);
    hit = BlackHole::rayIntersectsPlane(ro, rd2, glm::vec3(0), glm::vec3(0,1,0), t);
    EXPECT_TRUE(!hit);
}

void testTemperature() {
    std::cout << "== temperature ==\n";
    float t1 = BlackHole::temperatureAtRadius(2.8f, 2.8f, 0.8f);
    EXPECT_NEAR(t1, 1.0f, 1e-4);
    float t2 = BlackHole::temperatureAtRadius(10.0f, 2.8f, 0.8f);
    EXPECT_TRUE(t2 < 0.5f && t2 > 0.0f);
    float t3 = BlackHole::temperatureAtRadius(1.0f, 2.8f, 0.8f);
    EXPECT_NEAR(t3, 1.0f, 1e-4);
}

void testTemperatureColor() {
    std::cout << "== temperatureColor ==\n";
    glm::vec3 c0 = BlackHole::temperatureToColor(0.0f);
    glm::vec3 c1 = BlackHole::temperatureToColor(1.0f);
    EXPECT_TRUE(c0.r < 0.3f);
    EXPECT_TRUE(c1.r > 0.9f && c1.g > 0.9f);
    glm::vec3 cMid = BlackHole::temperatureToColor(0.5f);
    EXPECT_TRUE(cMid.r >= c0.r);
}

// La rampa de la pelicula tiene dos invariantes que la separan de un cuerpo negro
// real: nunca vira a azul (el azul jamas domina al rojo) y el brillo crece de forma
// monotona con la temperatura. Si alguien "corrige" la paleta hacia fisica real,
// esto lo detecta antes de que cambie el look.
void testDiskPaletteInvariants() {
    std::cout << "== diskPaletteInvariants ==\n";
    float prevLum = -1.0f;
    for (int i = 0; i <= 20; ++i) {
        float t = float(i) / 20.0f;
        glm::vec3 c = BlackHole::temperatureToColor(t);
        EXPECT_TRUE(c.b <= c.r + 1e-4f);
        float lum = 0.2126f*c.r + 0.7152f*c.g + 0.0722f*c.b;
        EXPECT_TRUE(lum >= prevLum - 1e-4f);
        prevLum = lum;
    }
}

void testBlackHoleValidation() {
    std::cout << "== blackHoleValidation ==\n";
    BlackHoleParams p;
    p.eventHorizonRadius = 1.5f;
    p.photonSphereRadius = 1.0f;
    p.diskInnerRadius = 1.0f;
    p.diskOuterRadius = 0.5f;
    p.diskHeight = 0.0f;
    p.planetRadius = 0.01f;
    p.planetOrbitRadius = 1.0f;
    BlackHole bh(p);
    EXPECT_TRUE(bh.params().photonSphereRadius >= bh.params().eventHorizonRadius * 1.3f - 1e-4);
    EXPECT_TRUE(bh.params().diskInnerRadius >= bh.params().eventHorizonRadius);
    EXPECT_TRUE(bh.params().diskOuterRadius > bh.params().diskInnerRadius);
    EXPECT_TRUE(bh.params().diskHeight >= 0.01f);
    EXPECT_TRUE(bh.params().planetRadius >= 0.05f);
    EXPECT_TRUE(bh.params().planetOrbitRadius >= bh.params().diskInnerRadius + 1.0f - 1e-4);
}

void testCameraVectors() {
    std::cout << "== cameraVectors ==\n";
    Camera cam(glm::vec3(0,0,5), -90, 0, 45, 16.0f/9.0f);
    EXPECT_VEC_NEAR(cam.front(), glm::vec3(0,0,-1), 1e-4);
    EXPECT_VEC_NEAR(glm::normalize(cam.right()), glm::vec3(1,0,0), 1e-4);
    cam.processMouseMovement(0, 1000, true);
    EXPECT_TRUE(cam.pitch() <= 89.0f + 1e-4);
    cam.processMouseMovement(0, -2000, true);
    EXPECT_TRUE(cam.pitch() >= -89.0f - 1e-4);
}

void testCameraMatrices() {
    std::cout << "== cameraMatrices ==\n";
    Camera cam(glm::vec3(0,0,5), -90, 0, 45, 1.0f);
    glm::mat4 view = cam.viewMatrix();
    glm::mat4 invView = cam.invViewMatrix();
    glm::mat4 shouldBeIdentity = view * invView;
    for(int i=0;i<4;i++) for(int j=0;j<4;j++) {
        float expected = (i==j)?1.0f:0.0f;
        EXPECT_NEAR(shouldBeIdentity[i][j], expected, 1e-3);
    }
    glm::mat4 proj = cam.projectionMatrix();
    glm::mat4 invProj = cam.invProjectionMatrix();
    shouldBeIdentity = proj * invProj;
    for(int i=0;i<4;i++) for(int j=0;j<4;j++) {
        float expected = (i==j)?1.0f:0.0f;
        EXPECT_NEAR(shouldBeIdentity[i][j], expected, 1e-3);
    }
}

void testAspect() {
    std::cout << "== aspect ==\n";
    Camera cam(glm::vec3(0), -90, 0, 45, 1.0f);
    cam.setAspect(2.0f);
    EXPECT_NEAR(cam.aspect(), 2.0f, 1e-5);
    cam.setFov(100.0f);
    EXPECT_TRUE(cam.fov() <= 90.0f);
}

int main() {
    std::cout << "Gargantua CPU Tests\n";
    testRaySphere();
    testRayPlane();
    testTemperature();
    testTemperatureColor();
    testDiskPaletteInvariants();
    testBlackHoleValidation();
    testCameraVectors();
    testCameraMatrices();
    testAspect();

    std::cout << "\nPassed: " << testsPassed << " Failed: " << testsFailed << "\n";
    return testsFailed == 0 ? 0 : 1;
}
