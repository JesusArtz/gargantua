# Gargantua Renderer — Simulación Interactiva de Agujero Negro

Renderizador en tiempo real de un agujero negro rotatorio inspirado en Gargantua (Interstellar). C++20 / OpenGL 4.3 / GLSL / GLFW / GLAD / GLM / Dear ImGui. Ray marching por píxel, lente gravitacional aproximada, disco de acreción procedural con estructura de nube, photon sphere, Doppler, redshift y post-proceso HDR con bloom y tone mapping ACES.

> Física aproximación visual estable, no GR exacta.

## Arquitectura
```
Gargantua/
├── CMakeLists.txt
├── external/ glad (GL 4.3) / glfw 3.4 / glm 1.0.1 / imgui 1.91.8
├── assets/shaders/
│   ├── fullscreen.vert   triángulo fullscreen, compartido por todos los pases
│   ├── blackhole.frag    ray marching -> radiancia HDR lineal
│   ├── bright.frag       extracción de brillos (soft knee)
│   ├── blur.frag         gaussiano separable
│   └── composite.frag    bloom + ACES + vignette + gamma
├── include/ Shader.hpp Camera.hpp BlackHole.hpp Renderer.hpp Input.hpp
├── src/ main.cpp Camera.cpp Shader.cpp BlackHole.cpp Renderer.cpp Input.cpp
└── tests/ test_math.cpp
```
RAII, sin estado global salvo callbacks Input.

## Pipeline (3 pases)
```
GLFW + GLAD (debug callback)
  -> Renderer::init(): VAO triangle + compila 4 programas
  -> Loop:
     [1] sceneRT (RGBA16F, a renderScale) : blackhole.frag, ray marching, HDR lineal
     [2] bloomRT[0..1] (media res)        : bright.frag -> N x (blur H, blur V) ping-pong
     [3] framebuffer por defecto          : composite.frag = escena + bloom, ACES, vignette, gamma
  -> ImGui -> swap
```
El pase [3] muestrea `sceneRT` con `GL_LINEAR`, así que también hace de upscale cuando `renderScale < 1`. Todos los targets son `RGBA16F`: el disco emite por encima de 1.0 y el bright pass tiene que poder verlo.

## Fullscreen & Rayos
`fullscreen.vert` genera triángulo `(-1,-1),(3,-1),(-1,3)`. Por píxel:
```glsl
ndc = uv*2-1; clip=vec4(ndc,-1,1); eye=uInvProjection*clip; eye=vec4(eye.xy,-1,0); world=uInvView*eye;
rayOrigin=uCameraPosition; rayDirection=normalize(world.xyz);
```

## Curvatura
```
acc = -normalize(offset)*gravity/(r*r) + tangential*spin*...   // drag suave, factor 0.12
direction += acc*stepSize; position += direction*stepSize;
stepSize = base*clamp(r/photonSphere, 0.15, 3.5)
```
La cota superior de 3.5 cruza el campo lejano en pocos pasos; ahí la curvatura es despreciable y el presupuesto de pasos se necesita cerca del agujero.

## Imágenes múltiples del disco (clave del look Gargantua)
El halo que arquea **por encima y por debajo** de la sombra no es "el disco de atrás": son la **segunda y tercera imagen del mismo disco**, luz que rodeó el agujero antes de llegar a la cámara. Un rayo que retorna en su primer impacto sólo puede mostrar la imagen primaria.

`traceRay` acumula emisión front-to-back con la transmitancia que queda tras cada cruce:
```glsl
accum += col * a * trans;
trans  *= (1.0 - a);
crossings++;
if(trans < 0.015) return accum;          // opaco: lo de atrás ya no importa
```
Y las tres salidas del bucle respetan lo ya acumulado:
```glsl
if(r < horizon)  return accum;                    // el disco DELANTE de la sombra sobrevive
if(r > escape)   return accum + trans * stars;
// presupuesto agotado: igual que escape
```
`uMaxCrossings` (slider "Disk Images") controla cuántas imágenes se acumulan. En 1 desaparece el halo; 3–4 es el look de la película.

## Disco: estructura de nube
```glsl
spiral = angle + time*omega(r) + 2.6*log(r/inner)   // espiral logarítmica
q      = vec2(spiral*0.8, rNorm*5.0)
warp   = vec2(fbm(q), fbm(q + vec2(5.2,1.3)))
clouds = fbm(q + 2.4*warp)                          // fbm con dominio deformado
cloudField = smoothstep(0.16, 0.84, 0.70*clouds + 0.24*wisps + 0.06*detail)
```
Tres decisiones que hacen que lea como gas y no como ruido:
- **Coordenada espiral.** El material interno orbita más rápido, así que cualquier textura pintada en coordenadas polares planas se cizalla en brazos. Muestrear el ruido en ese marco mete la cizalla desde el principio y convierte manchas redondas en filamentos alargados.
- **Domain warping.** Realimentar el fbm en sus propias coordenadas da estructura ondulante en vez de ruido plano obviamente procedural.
- **Emisión y densidad correlacionadas.** El mismo `cloudField` modula cuánto emite y cuán opaco es. Brillante y denso van juntos, que es lo que hace que las vetas parezcan material.

Profundidad óptica de una losa de grosor finito:
```glsl
pathLen = uDiskThickness * densityMod / max(|dot(dir,n)|, 0.06)
density = 1 - exp(-pathLen * 2.6)
```
El recorrido crece cuando el rayo roza el plano: por eso la toma casi de canto se ve sólida y la vista de frente se mantiene translúcida.

Emisividad ∝ `(inner/r)^1.15`, mucho más abrupta que el color: casi toda la luz sale de los primeros radios, así el núcleo se va a HDR (y hace bloom) mientras las afueras quedan lo bastante oscuras para que se lean los filamentos.

## Paleta
Ámbar profundo → naranja → blanco cálido. **Nunca vira a azul**: un disco a esa temperatura físicamente sí lo haría, pero DNGR lo mantuvo en el rango cálido para que se leyera como objeto físico y no como carta de cuerpo negro. La rampa está duplicada en `BlackHole::temperatureToColor` (C++) y en `blackhole.frag` (GLSL); si cambia una hay que cambiar la otra — `testDiskPaletteInvariants` verifica que el azul nunca domine al rojo y que la luminancia sea monótona.

## Doppler / Redshift
Doppler: `vel = tangent*0.45*r^-0.5, beta=|vel|, gamma=1/sqrt(1-beta²), dop=1/(gamma*(1-beta*cosTheta)), beamed=dop³`

El resultado se interpola con `uDopplerStrength`:
```glsl
return mix(1.0, beamed, uDopplerStrength);
```
**Thorne y DNGR renderizaron el disco de la película con el beaming casi apagado.** La asimetría real deja un lado cegador y el otro casi negro, y en pantalla se lee como error de render, no como física. Por eso el preset Cinematic usa `0.10` y el preset Physical `1.0`: la comparación entre ambos es exactamente la concesión estética que hizo la película.
Referencia: Thorne et al., *"Gravitational Lensing by Spinning Black Holes in Astrophysics, and in the Movie Interstellar"*, Class. Quantum Grav. 32 (2015) 065001.

Redshift: `g = sqrt(1-rs/r), dim = mix(0.35, 1, smoothstep(rs, 3rs, r))`

## Post-proceso HDR
- **Bright pass** con soft knee (`bright.frag`): el borde del disco se funde con el glow en vez de bandear en un corte duro.
- **Blur separable** (`blur.frag`): gaussiano de 9 taps colapsado a 5 muestras usando filtrado lineal del hardware. El kernel se ensancha en cada iteración (`spread = 1+i`), así unos pocos pases alcanzan la caída amplia del glow de cine sin un conteo enorme de taps. Corre a media resolución.
- **Composite** (`composite.frag`): curva filmic ACES (aproximación de Narkowicz 2015), vignette y codificación a sRGB. El ray marcher no hace tone mapping ni gamma: entrega radiancia lineal para que nada recorte el disco antes de que el bright pass lo vea.

## Limitaciones
Potencial newtoniano, disco delgado (losa de grosor uniforme), spin escalar con drag ad-hoc, sin geodésicas Kerr, sin órbitas múltiples ilimitadas (tope `uMaxCrossings`), estrellas procedurales sin HDRI.

## Compilación
Requisitos: C++20, OpenGL 4.3+, CMake 3.21+, Python3 (solo para regenerar glad). Dependencias vendorizadas.

Windows / MSVC (generador multi-config):
```bash
cmake -S . -B build
cmake --build build --config Release
./build/Release/gargantua.exe
./build/tests/Release/gargantua_tests.exe
```

Linux / macOS (generador single-config):
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/gargantua
./build/tests/gargantua_tests
```

`ctest --test-dir build` funciona en ambos.

### Flags de línea de comandos
| Flag | Efecto |
|---|---|
| `--no-ui` | arranca con el panel oculto (capturas limpias) |
| `--physical` | arranca con el preset Physical en vez del Cinematic |

## Controles
WASD mover, Q/E subir/bajar, mouse (F1 capturar), scroll FOV, ESC salir, R reset cámara, F11 fullscreen, 1/2 lensing, 3/4 disco, 5/6 doppler, 7/8 redshift, F2 estrellas, F3 UI, **F4 preset Cinematic**, **F5 preset Physical**.

Panel ImGui:
- **Black Hole & Kerr Physics**: Mass, Kerr Spin, Gravity Deflection, ISCO / Outer Radius, Photon Ring Boost, Ray Steps, Step Size
- **3D Volumetric Accretion Disk**: Volumetric 3D toggle, Gas Density, Disk Scale Height ($H_0$), HDR Brightness, Gas Turbulence, Max Crossings (fallback)
- **Relativity & Spectra**: Doppler effect, Gravitational Redshift, Doppler Strength, Spectral Color Shift (Blueshift/Redshift)
- **IMAX Camera Optics & HDR Post-Process**: Bloom toggle/intensity/threshold, Anamorphic Flare toggle/intensity, Chromatic Aberration, 70mm Film Grain, Exposure, Vignette
- **Environment & Celestial Body**: Milky Way & Stars toggle, Miller's Planet toggle, Radius, Orbit Radius, Orbit Speed
- **Render & Display**: renderScale, timeScale, VSync, Camera Reset

## Presets
| | Cinematic (F4) | Physical (F5) |
|---|---|---|
| Cámara | ~4° sobre el plano, r=30 | ~30° de inclinación, r=28 |
| Disco | Volumétrico 3D ($H_0=0.18$) | Volumétrico 3D ($H_0=0.18$) |
| Doppler Strength | 0.12 (film look) | 1.00 (beaming completo) |
| Shift Espectral | Off | On (Blueshift / Redshift de color) |
| Gas Turbulence | 0.72 | 0.85 |
| Anamorphic Flare | 0.55 | 0.25 |
| Bloom Intensity | 0.42 | 0.20 |
| Para qué | reproducir el frame exacto de Interstellar | mostrar la asimetría y corrimiento relativista real |

## Relación con CG
Coordenadas clip→NDC→view→world, vectores yaw/pitch sin gimbal lock, matrices lookAt/perspective/inverse, shaders GLSL 430, rasterización triangle, ray tracing por píxel, integración adaptiva con corrección relativista post-Newtoniana y arrastre de marco de Kerr, transferencia radiativa volumétrica 3D ($I = \int \text{emisión} \cdot e^{-\tau} ds$), noise/fbm y domain warping, framebuffers y render targets de punto flotante RGBA16F, pipeline de post-proceso multi-pase (Gaussian blur + horizontal anamorphic streak), tone mapping ACES filmic, aberración cromática, grano de película 70mm, codificación gamma/sRGB.

## Tests
`tests/test_math.cpp` (99 asserts): raySphere, rayPlane, temperature, temperatureColor, invariantes de la paleta del disco, validación BlackHole (con volumetría y cuerpos celestes), cámara vectores/matrices/aspect.
```bash
cmake --build build --config Release --target gargantua_tests
./build/tests/Release/gargantua_tests.exe
```
