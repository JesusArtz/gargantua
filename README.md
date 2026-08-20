# Gargantua Renderer — Simulación Interactiva de Agujero Negro

Renderizador en tiempo real de un agujero negro rotatorio inspirado en Gargantua (Interstellar). C++20 / OpenGL 4.3 / GLSL / GLFW / GLAD / GLM / Dear ImGui. Ray marching por píxel, lente gravitacional aproximada, disco de acreción procedural, photon sphere, Doppler y redshift.

> Física aproximación visual estable, no GR exacta.

## Arquitectura
```
Gargantua/
├── CMakeLists.txt
├── external/ glad (GL 4.3) / glfw 3.4 / glm 1.0.1 / imgui 1.91.8
├── assets/shaders/fullscreen.vert + blackhole.frag
├── include/ Shader.hpp Camera.hpp BlackHole.hpp Renderer.hpp Input.hpp
├── src/ main.cpp Camera.cpp Shader.cpp BlackHole.cpp Renderer.cpp Input.cpp
└── tests/ test_math.cpp
```
RAII, sin estado global salvo callbacks Input.

## Pipeline
GLFW + GLAD (debug callback) -> Renderer::init() VAO triangle + compila shaders -> Loop: Camera invView/invProjection -> uniforms -> glDrawArrays(TRIANGLES,0,3) fullscreen triangle -> fragment shader ray marching -> ImGui -> swap.

## Fullscreen & Rayos
`fullscreen.vert` genera triángulo `(-1,-1),(3,-1),(-1,3)`. Por píxel:
```glsl
ndc = uv*2-1; clip=vec4(ndc,-1,1); eye=uInvProjection*clip; eye=vec4(eye.xy,-1,0); world=uInvView*eye;
rayOrigin=uCameraPosition; rayDirection=normalize(world.xyz);
```

## Curvatura
```
acc = -normalize(offset)*gravity/(r*r) + tangential*spin*...
direction += acc*stepSize; position+=direction*stepSize;
stepSize = base*clamp(r/photonSphere,0.15,2.0)
```
Early exit `r<horizon` negro, `r>escapeRadius` estrellas. Separado de shading para futuro Kerr.

## Disco
Intersección plano entre pasos, `temp=pow(inner/r,exp)`, color darkRed->orange->yellow->white, fbm + noise, `angle+=time*omega(r)` omega~r^-1.2.

## Lensing
No warp post-proceso: cada píxel curva su rayo, disco trasero aparece arriba/abajo, estrellas deformadas vía `starBackground(normalize(dir))`.

## Doppler/Redshift
Doppler: `vel=tangent*0.45*r^-0.5, beta=|vel|, gamma=1/sqrt(1-beta²), dop=1/(gamma*(1-beta*cosTheta)), factor=dop³`
Redshift: `g=sqrt(1-rs/r), dim=mix(0.35,1,smoothstep(rs,3rs,r))`

## Limitaciones
Potencial newtoniano, disco delgado infinito, spin escalar, sin geodésicas Kerr, sin múltiples órbitas infinitas.

## Compilación
Requisitos: C++20, OpenGL 4.3+, CMake 3.21+, Python3 (solo para regenerar glad). Dependencias vendorizadas.
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/gargantua
./build/tests/gargantua_tests
```

## Controles
WASD mover, Q/E subir/bajar, mouse (F1 capturar), scroll FOV, ESC salir, R reset, F11 fullscreen, 1/2 lensing, 3/4 disco, 5/6 doppler, 7/8 redshift, F2 estrellas, F3 UI. UI: FPS, sliders mass/spin/gravity/radii/steps/stepSize, toggles, renderScale/timeScale, VSync.

## Relación con CG
Coordenadas clip->NDC->view->world, vectores yaw/pitch sin gimbal lock, matrices lookAt/perspective/inverse, shaders GLSL 430, rasterización triangle, ray tracing por píxel, iluminación procedural, noise/fbm, mix/smoothstep, integración Euler adaptiva.

## Tests
`tests/test_math.cpp`: raySphere, rayPlane, temperature, color, validación BlackHole, cámara vectores/matrices/aspect (54 asserts).
```bash
cmake --build build --target gargantua_tests && ./build/tests/gargantua_tests
ctest --test-dir build
```

## Futuro
Kerr completo, FBO renderScale + TAA, bloom ACES, HDRI, screenshot.
