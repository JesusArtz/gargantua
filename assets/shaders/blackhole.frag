#version 430 core
in vec2 vUV;
out vec4 FragColor;

// --- Uniforms required by spec ---
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

// ------------------------------------------------------------
// Hash / noise utilities
float hash(vec2 p) {
    // deterministic pseudo-random
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}
float hash3(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f*f*(3.0-2.0*f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a,b,f.x), mix(c,d,f.x), f.y);
}
float fbm(vec2 p) {
    float v = 0.0; float amp = 0.5;
    for(int i=0;i<4;i++){ v += amp * noise(p); p *= 2.0; amp *= 0.5; }
    return v;
}
float fbm3(vec3 p){
    // cheap 3D-ish via slicing
    return fbm(p.xy + p.z*0.7) * 0.5 + fbm(p.xz*1.3)*0.5;
}

// ------------------------------------------------------------
// Stellar background procedural
vec3 starBackground(vec3 dir) {
    // dir must be normalized world direction
    // Project to spherical coordinates for hashing
    // Use octahedral-ish hashing via direction components
    float starThreshold = 0.9975;
    // Multi-scale stars: hash based on direction quantized
    // Primary stars
    vec3 ad = abs(dir);
    // Use 2D hash on spherical mapping
    float u = atan(dir.z, dir.x) / 6.2831853 + 0.5;
    float v = dir.y * 0.5 + 0.5;
    // Increase density by scaling UV
    vec2 suv = vec2(u* 400.0, v* 200.0);
    vec2 id = floor(suv);
    vec2 gv = fract(suv) - 0.5;
    float h = hash(id);
    // Random offset within cell for star position jitter
    vec2 off = vec2(hash(id+vec2(7,3)), hash(id+vec2(11,19))) - 0.5;
    float d = length(gv - off*0.6);
    float star = 0.0;
    // Small stars: bright if hash exceeds threshold
    if (h > starThreshold) {
        float intensity = (h - starThreshold) / (1.0 - starThreshold);
        // size variation
        float r = 0.05 + intensity*0.08;
        float s = smoothstep(r, 0.0, d);
        // color variation: slight blue/yellow
        float colorJitter = hash(id + vec2(3.14, 9.1));
        vec3 col = mix(vec3(0.9,0.9,1.0), vec3(1.0,0.95,0.7), step(0.5, fract(h*7.0)));
        // Add subtle color shift based on jitter
        col += vec3(colorJitter*0.1-0.05);
        star = s * (0.7 + intensity*1.0) * 1.5;
        // add twinkle with hash-based flicker
        // twinkle slow using dir hash (no time, keep deterministic; slight time if desired)
        return col * star;
    }
    // Second layer fainter stars
    vec2 suv2 = vec2(u*800.0, v*400.0);
    vec2 id2 = floor(suv2);
    float h2 = hash(id2 + vec2(13.0, 77.0));
    if (h2 > 0.9985) {
        vec2 gv2 = fract(suv2)-0.5;
        float d2 = length(gv2);
        float s2 = smoothstep(0.2, 0.0, d2);
        return vec3(1.0) * s2 * 0.4;
    }
    // Faint nebula gradient based on direction
    float neb = fbm(vec2(u*3.0, v*3.0)) * 0.03;
    vec3 nebCol = mix(vec3(0.02,0.02,0.04), vec3(0.05,0.03,0.07), fbm(vec2(u*1.5 + 0.3, v*1.5)));
    return nebCol * neb;
}

// ------------------------------------------------------------
// Disk helpers
vec3 temperatureToColor(float t){
    t = clamp(t, 0.0, 1.0);
    if(t < 0.25) {
        float k = t/0.25;
        return mix(vec3(0.12,0.02,0.02), vec3(0.8,0.12,0.02), k);
    } else if(t < 0.5){
        float k = (t-0.25)/0.25;
        return mix(vec3(0.8,0.12,0.02), vec3(1.0,0.45,0.1), k);
    } else if(t < 0.75){
        float k = (t-0.5)/0.25;
        return mix(vec3(1.0,0.45,0.1), vec3(1.0,0.85,0.4), k);
    } else {
        float k = (t-0.75)/0.25;
        return mix(vec3(1.0,0.85,0.4), vec3(1.0,1.0,0.95), k);
    }
}

bool intersectPlane(vec3 ro, vec3 rd, vec3 planePoint, vec3 planeNormal, out float t){
    float denom = dot(planeNormal, rd);
    if(abs(denom) < 1e-6) return false;
    t = dot(planePoint - ro, planeNormal) / denom;
    return t > 0.0;
}

// Procedural disk color with turbulence and orbit animation
vec3 shadeDisk(vec3 hitPos, float radius, vec3 bhPos, float time, out float alpha){
    float inner = uDiskInnerRadius;
    float outer = uDiskOuterRadius;
    // Normalize radius for temperature
    float temp = pow(clamp(inner / radius, 0.0, 1.0), uTemperatureExponent);
    vec3 base = temperatureToColor(temp);

    // Polar coordinates in disk plane
    // Project hitPos onto disk plane basis
    // Assume diskNormal is Y-up for simplicity but respect uniform
    // Build tangent basis: pick arbitrary perpendicular
    vec3 n = normalize(uDiskNormal);
    vec3 tangent = normalize(cross(n, vec3(0,0,1)));
    if(length(tangent) < 0.1) tangent = normalize(cross(n, vec3(1,0,0)));
    vec3 bitangent = cross(n, tangent);
    vec3 rel = hitPos - bhPos;
    float x = dot(rel, tangent);
    float z = dot(rel, bitangent);
    float angle = atan(z, x);
    // Orbital motion: Kepler-like angular velocity ~ r^-1.5
    float omega = 1.2 * pow(radius, -1.2); // tunable
    float animatedAngle = angle + time * omega + uSpin * time * 0.2; // spin drags
    // Spiral turbulence
    float rNorm = (radius - inner) / max(0.001, outer - inner);
    vec2 polarUV = vec2(animatedAngle * 2.0 + rNorm*6.0, rNorm*10.0);
    float turb = fbm(polarUV * 1.5);
    // Additional high-frequency noise for gas clumps
    float clumps = noise(vec2(animatedAngle*8.0, radius*2.0 + time*0.7));
    float mod = 0.7 + 0.5 * turb + 0.2 * clumps;
    // Hotspots
    float hotspot = pow(clumps, 3.0) * 0.6;
    vec3 col = base * mod + vec3(hotspot);
    // Doppler approximation (computed outside, applied later via factor)
    // Fade at edges
    float edgeFade = smoothstep(outer, outer*0.92, radius) * (1.0 - smoothstep(inner, inner*1.08, radius) * 0.0);
    // Actually inner fade: make inner edge sharp but slightly transparent due to ISCO
    float innerFade = smoothstep(inner, inner*1.15, radius);
    float outerFade = 1.0 - smoothstep(outer*0.85, outer, radius);
    alpha = innerFade * outerFade;
    // Thin disk thickness falloff: if hitPos close to plane it's opaque, already intersected
    return col;
}

// Gravitational redshift approximation (visual)
vec3 applyRedshift(vec3 col, float radius){
    if(uRedshiftEnabled == 0) return col;
    float rs = uEventHorizonRadius;
    // Redshift factor increases near horizon
    float g = sqrt(max(0.0, 1.0 - rs / max(radius, rs+0.01)));
    // g ~1 far, ~0 near; map to slight reddening and dimming
    float dim = mix(0.35, 1.0, smoothstep(rs, rs*3.0, radius));
    vec3 shifted = mix(col * vec3(1.2, 0.7, 0.5), col, g);
    return shifted * dim;
}

// Doppler factor (approx relativistic beaming)
float dopplerFactor(vec3 diskVel, vec3 viewDir){
    if(uDopplerEnabled == 0) return 1.0;
    float c = 1.0; // normalized light speed
    float beta = length(diskVel) / c;
    beta = clamp(beta, 0.0, 0.6);
    float gamma = 1.0 / sqrt(max(0.1, 1.0 - beta*beta));
    float cosTheta = dot(normalize(diskVel), normalize(-viewDir)); // vel vs line-of-sight
    float dop = 1.0 / (gamma * (1.0 - beta * cosTheta));
    // Beaming ~ dop^3 for intensity (approx)
    return pow(dop, 3.0);
}

// ------------------------------------------------------------
// Main ray march
vec3 traceRay(vec3 ro, vec3 rd, out bool hitHorizon, out bool hitDisk, out vec3 diskColor, out float diskAlpha) {
    hitHorizon = false;
    hitDisk = false;
    diskColor = vec3(0.0);
    diskAlpha = 0.0;

    vec3 pos = ro;
    vec3 dir = normalize(rd);

    float stepSize = uBaseStepSize;
    vec3 bh = uBlackHolePosition;
    vec3 n = normalize(uDiskNormal);

    // For disk intersection detection we track previous position
    vec3 prevPos = pos;
    float prevDiskDist = dot(prevPos - bh, n);

    for(int i=0; i< 300; i++){
        if(i >= uMaxSteps) break;
        vec3 offset = pos - bh;
        float r = length(offset);
        if(r < uEventHorizonRadius){
            hitHorizon = true;
            return vec3(0.0);
        }
        if(r > uEscapeRadius){
            // escaped: return star background in current direction (lensed direction)
            if(uStarsEnabled == 1) return starBackground(normalize(dir));
            else return vec3(0.02, 0.02, 0.04);
        }

        // Adaptive step: smaller near photon sphere / horizon
        if(uLensingEnabled == 1){
            float adaptive = clamp(r / uPhotonSphereRadius, 0.15, 2.0);
            stepSize = uBaseStepSize * adaptive;
            // Extra refinement very close to photon sphere
            if(r < uPhotonSphereRadius * 1.5) stepSize *= 0.5;
            if(r < uEventHorizonRadius * 1.3) stepSize *= 0.5;
        } else {
            stepSize = uBaseStepSize;
        }

        // Gravitational bending (Newtonian approx with spin drag)
        if(uLensingEnabled == 1 && uGravityStrength > 0.001){
            vec3 gravDir = -normalize(offset);
            float invR2 = 1.0 / max(0.1, r*r);
            // Basic Newtonian acceleration
            vec3 acc = gravDir * uGravityStrength * invR2;
            // Spin frame-dragging: small azimuthal kick around disk normal
            if(uSpin > 0.01){
                vec3 spinAxis = n;
                vec3 tangential = normalize(cross(spinAxis, offset));
                float spinEffect = uSpin * 0.8 * uGravityStrength * invR2 * (uPhotonSphereRadius / max(r, 0.5));
                acc += tangential * spinEffect * 0.3;
            }
            dir += acc * stepSize;
            dir = normalize(dir);
        }

        // Advance
        prevPos = pos;
        prevDiskDist = dot(prevPos - bh, n);
        pos += dir * stepSize;
        float currDiskDist = dot(pos - bh, n);

        // Disk intersection: crossed the plane between steps
        if(uDiskEnabled == 1 && prevDiskDist * currDiskDist < 0.0){
            // Interpolate intersection point
            float t = prevDiskDist / (prevDiskDist - currDiskDist); // 0..1
            vec3 hit = mix(prevPos, pos, t);
            float rad = length(hit - bh);
            // Also need to consider thickness vs plane: approximate infinitely thin
            // Check radial bounds
            if(rad >= uDiskInnerRadius && rad <= uDiskOuterRadius){
                // Shade disk at hit point
                float alpha;
                vec3 col = shadeDisk(hit, rad, bh, uTime, alpha);
                // Apply redshift
                col = applyRedshift(col, rad);
                // Doppler: estimate orbital velocity tangential
                vec3 tangent = normalize(cross(n, hit - bh));
                // Direction depends on which side: use cross to get orbital direction (prograde)
                // Kepler velocity magnitude ~ sqrt(M/r) approx
                float vMag = 0.45 * pow(max(rad,0.5), -0.5);
                vec3 vel = tangent * vMag;
                // View direction from hit to camera? Approximate with -dir (ray direction reversed)
                float dop = dopplerFactor(vel, dir);
                // Apply
                col *= clamp(dop, 0.4, 2.2);
                // Fade with alpha
                hitDisk = true;
                diskColor = col;
                diskAlpha = alpha;
                // For opaque inner region, return disk color attenuated by distance
                // Blend with background based on alpha; but we currently treat disk as emissive hit
                // If we hit disk, we still continue for secondary lensed hits? Simple: return disk with background mixed
                // To allow seeing lensed second image, we don't early exit fully opaque - we mix
                // But for first strong hit, return
                // Check if this is the primary image (not yet bounced) -> return directly
                // For simplicity return disk color modulated
                // Blend slight with stars behind for transparency at edges
                vec3 bg = vec3(0.0);
                if(alpha < 0.95){
                    // continue tracing a bit for background blend? approximate
                    bg = starBackground(normalize(dir)) * (1.0 - alpha) * 0.3;
                }
                return col * alpha + bg;
            }
        }

        // Also check for near-miss disk crossing within stepSize thickness (for grazing angles)
        // Already handled via plane crossing
    }
    // Max steps reached -> treat as background
    if(uStarsEnabled == 1) return starBackground(normalize(dir));
    return vec3(0.02,0.02,0.04);
}

// ------------------------------------------------------------
void main(){
    vec2 fragCoord = gl_FragCoord.xy;
    // Handle renderScale: we could just use resolution uniform already scaled
    vec2 uv = fragCoord / uResolution; // 0..1
    vec2 ndc = uv * 2.0 - 1.0; // -1..1

    // Reconstruct ray via inverse view/projection (spec)
    // For fullscreen rendering we compute ray in view space then to world
    vec4 clip = vec4(ndc, -1.0, 1.0);
    vec4 eye = uInvProjection * clip;
    eye = vec4(eye.xy, -1.0, 0.0);
    vec4 worldDir4 = uInvView * eye;
    vec3 rd = normalize(worldDir4.xyz);
    vec3 ro = uCameraPosition;

    bool hitHorizon, hitDisk;
    vec3 diskCol; float diskAlpha;
    vec3 color = traceRay(ro, rd, hitHorizon, hitDisk, diskCol, diskAlpha);

    // Post: slight exposure / tone mapping
    color = pow(color, vec3(0.85)); // gamma-ish
    // Vignette subtle
    float vign = 1.0 - 0.15 * length(ndc);
    color *= vign;

    // If horizon hit, ensure black (already 0)
    if(hitHorizon) color = vec3(0.0);

    FragColor = vec4(color, 1.0);
}
