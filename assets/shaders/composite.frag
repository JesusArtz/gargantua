#version 430 core
// Final pass: chromatic aberration, bloom, anamorphic flare, ACES tone mapping,
// film grain, vignette, and sRGB encoding.
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform sampler2D uFlare;

uniform int   uBloomEnabled;
uniform float uBloomIntensity;
uniform int   uFlareEnabled;
uniform float uFlareIntensity;
uniform float uChromaticAberration;
uniform float uFilmGrain;
uniform float uExposure;
uniform float uVignette;
uniform float uTime;

// ACES filmic curve (Narkowicz 2015 approximation)
vec3 acesFilm(vec3 x){
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Pseudo-random noise for film grain
float hashGrain(vec2 p) {
    p = fract(p * vec2(443.897, 441.423));
    p += dot(p, p.yx + 19.19);
    return fract((p.x + p.y) * p.x);
}

void main(){
    vec2 ndc = vUV * 2.0 - 1.0;
    vec2 caOffset = ndc * uChromaticAberration * 0.025;

    // Sample scene with subtle radial chromatic aberration
    vec3 col;
    if (uChromaticAberration > 0.0001) {
        col.r = texture(uScene, vUV + caOffset).r;
        col.g = texture(uScene, vUV).g;
        col.b = texture(uScene, vUV - caOffset).b;
    } else {
        col = texture(uScene, vUV).rgb;
    }

    // Isotropic Gaussian Bloom
    if (uBloomEnabled == 1) {
        col += texture(uBloom, vUV).rgb * uBloomIntensity;
    }

    // Anamorphic Lens Flare (Horizontal Streak) with IMAX lens tint
    if (uFlareEnabled == 1) {
        vec3 flare = texture(uFlare, vUV).rgb;
        // Warm/subtle blue IMAX anamorphic tint
        vec3 flareTint = mix(vec3(1.0, 0.92, 0.82), vec3(0.75, 0.85, 1.05), 0.35);
        col += flare * flareTint * uFlareIntensity;
    }

    // Exposure & ACES Filmic Tone Mapping
    col *= uExposure;
    col = acesFilm(col);

    // Vignette
    col *= 1.0 - uVignette * dot(ndc, ndc) * 0.35;

    // 70mm Film Grain
    if (uFilmGrain > 0.001) {
        float grain = hashGrain(vUV * 1200.0 + fract(uTime * 43.123));
        float grainFactor = (grain - 0.5) * uFilmGrain;
        // Grain is more visible in midtones than in pure black/white
        float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
        float midtoneMask = smoothstep(0.02, 0.35, lum) * (1.0 - smoothstep(0.70, 0.98, lum));
        col += grainFactor * midtoneMask;
    }

    // sRGB gamma encoding
    col = pow(max(col, vec3(0.0)), vec3(1.0 / 2.2));
    FragColor = vec4(col, 1.0);
}
