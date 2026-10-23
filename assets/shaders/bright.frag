#version 430 core
// Bloom bright pass: keep only what is above the threshold, with a soft knee so
// the disk edge fades into the glow instead of banding at a hard cutoff.
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform float uThreshold;
uniform float uSoftKnee;

void main(){
    vec3 c = texture(uScene, vUV).rgb;
    float br = max(c.r, max(c.g, c.b));

    float knee = max(uSoftKnee, 1e-4);
    float soft = clamp(br - uThreshold + knee, 0.0, 2.0 * knee);
    soft = (soft * soft) / (4.0 * knee);

    float contrib = max(soft, br - uThreshold) / max(br, 1e-4);
    FragColor = vec4(c * contrib, 1.0);
}
