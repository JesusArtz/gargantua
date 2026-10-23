#version 410 core
in vec2 vUV;
out vec4 FragColor;

// --- Uniforms ---
uniform vec2 uResolution;
uniform vec3 uCameraPosition;
uniform mat4 uInvView;
uniform mat4 uInvProjection;
uniform float uTime;

// Black hole
uniform vec3 uBlackHolePosition;
uniform vec3 uDiskNormal;
uniform float uEventHorizonRadius;
uniform float uPhotonSphereRadius;
uniform float uDiskInnerRadius;
uniform float uDiskOuterRadius;
uniform float uGravityStrength;
uniform float uSpin;
uniform float uBaseStepSize;
uniform int   uMaxSteps;
uniform float uEscapeRadius;
uniform float uTemperatureExponent;
uniform int   uLensingEnabled;
uniform int   uDiskEnabled;
uniform int   uStarsEnabled;
uniform int   uDopplerEnabled;
uniform int   uRedshiftEnabled;
uniform float uRenderScale;

// Look controls & Volumetrics (Interstellar / DNGR reference)
uniform int   uMaxCrossings;     // secondary + tertiary disk images
uniform float uDopplerStrength;  // 0 = film look, 1 = full relativistic beaming
uniform int   uSpectralDoppler;  // 0 = intensity only, 1 = color temperature shift
uniform int   uVolumetricDisk;   // 1 = 3D continuous volume raymarching, 0 = planar crossings
uniform float uDiskDensity;      // optical gas density scale
uniform float uDiskThickness;    // slab base height / scale height
uniform float uDiskBrightness;   // HDR scale
uniform float uDiskTurbulence;   // 0 = smooth, 1 = turbulent gas
uniform float uPhotonRingBoost;  // crisp photon ring intensity

// Celestial body (Miller's Planet)
uniform int   uPlanetEnabled;
uniform float uPlanetRadius;
uniform float uPlanetOrbitRadius;
uniform float uPlanetOrbitSpeed;

// ------------------------------------------------------------
// Hash / noise utilities
float shash21(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}
float shash31(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float snoise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = shash21(i);
    float b = shash21(i + vec2(1.0, 0.0));
    float c = shash21(i + vec2(0.0, 1.0));
    float d = shash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p) {
    float v = 0.0; float amp = 0.5;
    for (int i = 0; i < 5; i++) {
        v += amp * snoise2D(p);
        p *= 2.02;
        amp *= 0.5;
    }
    return v;
}

// ------------------------------------------------------------
// Milky Way & Cosmic Starfield
vec3 starBackground(vec3 dir) {
    float u = atan(dir.z, dir.x) / 6.2831853 + 0.5;
    float v = dir.y * 0.5 + 0.5;

    // Milky Way galactic band (tilted across space)
    vec3 mwDir = normalize(vec3(dir.x * 0.866 - dir.y * 0.5, dir.x * 0.5 + dir.y * 0.866, dir.z));
    float mwLat = abs(mwDir.y);
    float mwBand = exp(-mwLat * mwLat * 26.0);
    float mwCenter = max(0.0, dot(mwDir, vec3(0.0, 0.0, 1.0)));
    float mwCore = pow(mwCenter, 4.0) * exp(-mwLat * mwLat * 14.0) * 1.5;

    // Galactic dust lanes and nebular clouds
    float dustNoise = fbm(vec2(u * 14.0, v * 14.0));
    float dustLanes = smoothstep(0.38, 0.75, dustNoise) * mwBand;
    vec3 mwColor = mix(vec3(0.08, 0.06, 0.12), vec3(0.28, 0.18, 0.11), dustNoise);
    vec3 neb = mwColor * (mwBand * 0.08 + mwCore * 0.22) * (1.0 - dustLanes * 0.8);

    // Stars
    vec2 suv = vec2(u * 280.0, v * 140.0);
    vec2 id = floor(suv);
    vec2 gv = fract(suv) - 0.5;
    float h = shash21(id);
    vec2 off = vec2(shash21(id + vec2(7, 3)), shash21(id + vec2(11, 19))) - 0.5;
    float d = length(gv - off * 0.6);

    float starThreshold = mix(0.994, 0.982, mwBand);
    vec3 stars = vec3(0.0);
    if (h > starThreshold) {
        float intensity = (h - starThreshold) / (1.0 - starThreshold);
        float r = 0.08 + intensity * 0.14;
        float s = smoothstep(r, 0.0, d);
        vec3 col = mix(vec3(0.85, 0.90, 1.0), vec3(1.0, 0.92, 0.75), step(0.5, fract(h * 7.0)));
        stars = col * s * (0.6 + intensity * 1.8);
    }

    vec2 suv2 = vec2(u * 560.0, v * 280.0);
    vec2 id2 = floor(suv2);
    float h2 = shash21(id2 + vec2(13.0, 77.0));
    if (h2 > 0.995) {
        vec2 gv2 = fract(suv2) - 0.5;
        float d2 = length(gv2);
        float s2 = smoothstep(0.25, 0.0, d2);
        stars += vec3(0.8, 0.85, 1.0) * s2 * 0.5;
    }

    return neb + stars;
}

// ------------------------------------------------------------
// Interstellar Color Palette
// Deep amber -> fiery orange -> warm gold -> incandescent white
vec3 temperatureToColor(float t) {
    t = clamp(t, 0.0, 1.0);
    const vec3 c0 = vec3(0.28, 0.09, 0.03);
    const vec3 c1 = vec3(0.95, 0.38, 0.09);
    const vec3 c2 = vec3(1.00, 0.72, 0.34);
    const vec3 c3 = vec3(1.00, 0.95, 0.85);
    if (t < 0.33) return mix(c0, c1, t / 0.33);
    if (t < 0.70) return mix(c1, c2, (t - 0.33) / 0.37);
    return mix(c2, c3, (t - 0.70) / 0.30);
}

// Gravitational Redshift
vec3 applyRedshift(vec3 col, float radius) {
    if (uRedshiftEnabled == 0) return col;
    float rs = uEventHorizonRadius;
    float g = sqrt(max(0.0, 1.0 - rs / max(radius, rs + 0.01)));
    float dim = mix(0.20, 1.0, smoothstep(rs, rs * 2.6, radius));
    vec3 shifted = mix(col * vec3(1.20, 0.60, 0.35), col, g);
    return shifted * dim;
}

// Relativistic Doppler factor
float calculateDoppler(vec3 diskVel, vec3 viewDir, out float beta) {
    beta = clamp(length(diskVel), 0.0, 0.85);
    float gamma = 1.0 / sqrt(max(0.05, 1.0 - beta * beta));
    float cosTheta = dot(normalize(diskVel), normalize(-viewDir));
    float dop = 1.0 / (gamma * (1.0 - beta * cosTheta));
    return dop;
}

// ------------------------------------------------------------
// Accretion Disk Shading with Interstellar Micro-Filaments
vec3 shadeAccretionDisk(vec3 hitPos, float radius, vec3 bhPos, vec3 rayDir, float time, out float alpha) {
    float inner = uDiskInnerRadius;
    float outer = uDiskOuterRadius;

    // Thermodynamic base temperature
    float temp = pow(clamp(inner / radius, 0.0, 1.0), uTemperatureExponent);
    vec3 baseCol = temperatureToColor(temp);

    // Inner edge emissivity (steep gradient towards ISCO)
    float emissivity = pow(clamp(inner / radius, 0.0, 1.0), 1.25);

    // Polar coordinates in disk plane
    vec3 n = normalize(uDiskNormal);
    vec3 tangent = normalize(cross(n, vec3(0.0, 0.0, 1.0)));
    if (length(tangent) < 0.1) tangent = normalize(cross(n, vec3(1.0, 0.0, 0.0)));
    vec3 bitangent = cross(n, tangent);

    vec3 rel = hitPos - bhPos;
    float x = dot(rel, tangent);
    float z = dot(rel, bitangent);
    float angle = atan(z, x);

    // Keplerian orbital rotation
    float omega = 1.4 * pow(max(radius, 0.5), -1.5);
    float animatedAngle = angle + time * omega + uSpin * time * 0.15;
    float rNorm = (radius - inner) / max(0.001, outer - inner);

    // Logarithmic spiral shear
    float spiral = animatedAngle + 3.2 * log(max(radius / inner, 1.0001));

    // High-frequency fibrous striations (thousands of gas threads)
    float f1 = snoise2D(vec2(spiral * 1.5, rNorm * 45.0));
    float f2 = snoise2D(vec2(spiral * 3.5, rNorm * 110.0));
    float f3 = snoise2D(vec2(spiral * 8.0, rNorm * 260.0));
    float filaments = f1 * 0.50 + f2 * 0.32 + f3 * 0.18;

    // Billowing gas domain-warped clouds
    vec2 q = vec2(spiral * 0.75, rNorm * 6.0);
    vec2 warp = vec2(fbm(q), fbm(q + vec2(5.2, 1.3)));
    float clouds = fbm(q + 2.2 * warp);

    // Combined gas structure
    float gasField = smoothstep(0.12, 0.88, clouds * 0.38 + filaments * 0.62);

    float turb = clamp(uDiskTurbulence, 0.0, 1.0);
    float emissionMod = mix(1.0, 0.35 + 1.65 * gasField, turb);
    float densityMod  = mix(1.0, 0.25 + 1.50 * gasField, turb);

    // Optical depth through the thin disk slab
    float cosI = abs(dot(normalize(rayDir), n));
    float pathLen = uDiskThickness * densityMod / max(cosI, 0.04);
    // Optical density calibrated so disk is translucent when viewed at an angle
    float opticalDepth = pathLen * 2.2 * (uDiskDensity / 3.0);
    float density = 1.0 - exp(-opticalDepth);

    // Radial boundaries (sharp ISCO inner edge, soft outer boundary)
    float innerFade = smoothstep(inner, inner * 1.04, radius);
    float outerFade = 1.0 - smoothstep(outer * 0.68, outer, radius);
    alpha = clamp(density * innerFade * outerFade, 0.0, 1.0);

    // Doppler beaming and optional spectral shift
    vec3 diskVel = normalize(cross(n, rel)) * 0.48 * pow(max(radius, 0.5), -0.5);
    float beta;
    float dop = calculateDoppler(diskVel, rayDir, beta);

    vec3 col;
    if (uSpectralDoppler == 1 && uDopplerEnabled == 1) {
        float effectiveTemp = clamp(temp * pow(dop, 1.2), 0.0, 1.0);
        col = temperatureToColor(effectiveTemp);
        col *= mix(1.0, pow(dop, 2.2), uDopplerStrength);
    } else {
        col = baseCol;
        if (uDopplerEnabled == 1) {
            float beamed = pow(dop, 3.0);
            col *= mix(1.0, beamed, uDopplerStrength);
        }
    }

    col = applyRedshift(col, radius);

    // Blinding inner rim glow
    float innerGlow = pow(clamp(inner * 1.30 / radius, 0.0, 1.0), 3.5);
    return col * emissivity * emissionMod * uDiskBrightness * (1.0 + innerGlow * 1.2);
}

// ------------------------------------------------------------
// Orbiting Celestial Sphere (Miller's Planet)
bool testPlanet(vec3 p0, vec3 p1, vec3 bhPos, float time, out vec3 planetRadiance, out float tHit) {
    planetRadiance = vec3(0.0);
    tHit = 1.0;
    if (uPlanetEnabled == 0) return false;

    float orbitAngle = time * uPlanetOrbitSpeed + 2.2;
    vec3 pPos = bhPos + vec3(cos(orbitAngle) * uPlanetOrbitRadius, sin(orbitAngle * 0.4) * 0.9, sin(orbitAngle) * uPlanetOrbitRadius);
    float rPlanet = uPlanetRadius;

    vec3 segment = p1 - p0;
    float segLen = length(segment);
    if (segLen < 1e-5) return false;
    vec3 segDir = segment / segLen;

    vec3 oc = p0 - pPos;
    float b = dot(oc, segDir);
    float c = dot(oc, oc) - rPlanet * rPlanet;
    float disc = b * b - c;
    if (disc < 0.0) return false;

    float s = sqrt(disc);
    float t = -b - s;
    if (t < 0.0 || t > segLen) return false;

    tHit = t / segLen;
    vec3 hitPos = p0 + segDir * t;
    vec3 N = normalize(hitPos - pPos);

    // Planet lighting from the disk / black hole core
    vec3 lightDir = normalize(bhPos - pPos);
    float NdotL = dot(N, lightDir);
    float diffuse = max(0.0, NdotL * 0.90 + 0.10);

    float pTex = fbm(vec2(atan(N.z, N.x) * 3.0, N.y * 5.0));
    vec3 oceanCol = vec3(0.02, 0.05, 0.12);
    vec3 landCol  = vec3(0.08, 0.09, 0.10);
    vec3 cloudCol = vec3(0.70, 0.75, 0.85);
    float clouds = smoothstep(0.46, 0.72, pTex);
    vec3 surface = mix(mix(oceanCol, landCol, smoothstep(0.38, 0.44, pTex)), cloudCol, clouds * 0.55);

    // Atmospheric rim glow
    float fresnel = pow(1.0 - max(0.0, dot(N, -segDir)), 4.0);
    vec3 atmoColor = vec3(0.25, 0.50, 0.90) * fresnel * 1.8;

    planetRadiance = (surface * diffuse * 1.5 + atmoColor) * uDiskBrightness * 0.5;
    return true;
}

// ------------------------------------------------------------
// Main Raymarch Engine
vec3 traceRay(vec3 ro, vec3 rd) {
    vec3 pos = ro;
    vec3 dir = normalize(rd);
    vec3 bh = uBlackHolePosition;
    vec3 n = normalize(uDiskNormal);

    vec3 accum = vec3(0.0);
    float trans = 1.0;
    int crossings = 0;

    float minRadiusSeen = 1e6;
    vec3 prevPos = pos;
    float prevDiskDist = dot(prevPos - bh, n);

    for (int i = 0; i < 300; i++) {
        if (i >= uMaxSteps) break;
        vec3 offset = pos - bh;
        float r = length(offset);
        minRadiusSeen = min(minRadiusSeen, r);

        // --- Event Horizon Hit: Deep Black Shadow ---
        if (r < uEventHorizonRadius) {
            // Rays falling into the horizon show pure blackness!
            // Only light accumulated in FRONT of the shadow is returned.
            return accum;
        }

        // --- Escape to Deep Space ---
        if (r > uEscapeRadius) {
            vec3 bg = (uStarsEnabled == 1) ? starBackground(normalize(dir)) : vec3(0.004, 0.004, 0.008);
            return accum + trans * bg;
        }

        // Adaptive step size
        float stepSize = uBaseStepSize;
        if (uLensingEnabled == 1) {
            float adaptive = clamp(r / uPhotonSphereRadius, 0.14, 3.5);
            stepSize = uBaseStepSize * adaptive;
            if (r < uPhotonSphereRadius * 1.6) stepSize *= 0.5;
            if (r < uEventHorizonRadius * 1.35) stepSize *= 0.45;
        }

        // Relativistic Kerr Gravitational Deflection
        if (uLensingEnabled == 1 && uGravityStrength > 0.001) {
            vec3 gravDir = -normalize(offset);
            float invR2 = 1.0 / max(0.1, r * r);
            // Relativistic 1 + 3M/r general relativity light bending
            float grCorrection = 1.0 + 3.0 * (uEventHorizonRadius * 0.5) / max(r, 0.1);
            vec3 acc = gravDir * (uGravityStrength * invR2 * grCorrection);

            // Kerr Frame Dragging
            if (uSpin > 0.005) {
                vec3 tangential = normalize(cross(n, offset));
                float rH = uEventHorizonRadius;
                float spinEffect = uSpin * 1.5 * uGravityStrength * invR2 * (rH / max(r, 0.4));
                acc += tangential * spinEffect * 0.15;
            }
            dir = normalize(dir + acc * stepSize);
        }

        vec3 nextPos = pos + dir * stepSize;

        // Check Planet Hit
        vec3 pColor; float tHitP;
        if (testPlanet(pos, nextPos, bh, uTime, pColor, tHitP)) {
            accum += pColor * trans;
            return accum;
        }

        // Accretion Disk Plane Crossing (Primary, Secondary & Tertiary Images)
        if (uDiskEnabled == 1 && crossings < uMaxCrossings) {
            float currDiskDist = dot(nextPos - bh, n);
            if (prevDiskDist * currDiskDist < 0.0) {
                float t = prevDiskDist / (prevDiskDist - currDiskDist);
                vec3 hit = mix(pos, nextPos, t);
                float rad = length(hit - bh);
                if (rad >= uDiskInnerRadius && rad <= uDiskOuterRadius) {
                    float a;
                    vec3 col = shadeAccretionDisk(hit, rad, bh, dir, uTime, a);

                    // Front-to-back radiative transfer
                    accum += col * a * trans;
                    trans *= (1.0 - a);
                    crossings++;
                    if (trans < 0.012) return accum;
                }
            }
            prevDiskDist = currDiskDist;
        }

        pos = nextPos;
    }

    // Razor-Sharp Photon Ring (Caustic at the critical photon orbit)
    if (uPhotonRingBoost > 0.05 && minRadiusSeen < uPhotonSphereRadius * 1.05 && minRadiusSeen > uEventHorizonRadius * 1.005) {
        float ringDist = abs(minRadiusSeen - uPhotonSphereRadius * 0.98);
        float ring = exp(-ringDist * ringDist * 320.0) * uPhotonRingBoost * 1.8;
        accum += vec3(1.0, 0.92, 0.78) * ring * trans;
    }

    vec3 bg = (uStarsEnabled == 1) ? starBackground(normalize(dir)) : vec3(0.004, 0.004, 0.008);
    return accum + trans * bg;
}

// ------------------------------------------------------------
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / uResolution;
    vec2 ndc = uv * 2.0 - 1.0;

    // Reconstruct world ray
    vec4 clip = vec4(ndc, -1.0, 1.0);
    vec4 eye = uInvProjection * clip;
    eye = vec4(eye.xy, -1.0, 0.0);
    vec4 worldDir4 = uInvView * eye;
    vec3 rd = normalize(worldDir4.xyz);
    vec3 ro = uCameraPosition;

    vec3 color = traceRay(ro, rd);

    FragColor = vec4(max(color, vec3(0.0)), 1.0);
}
