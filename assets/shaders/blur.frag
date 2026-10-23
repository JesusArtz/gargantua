#version 430 core
// Separable Gaussian blur. Run once with uTexelDir = (1/w, 0) and once with
// (0, 1/h) to get a 2D blur at 2N taps instead of N*N.
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uSource;
uniform vec2 uTexelDir;

void main(){
    // 9-tap Gaussian collapsed to 5 samples using hardware linear filtering.
    const float w0 = 0.2270270270;
    const float w1 = 0.3162162162;
    const float w2 = 0.0702702703;
    const float o1 = 1.3846153846;
    const float o2 = 3.2307692308;

    vec3 c  = texture(uSource, vUV).rgb * w0;
    c += texture(uSource, vUV + uTexelDir * o1).rgb * w1;
    c += texture(uSource, vUV - uTexelDir * o1).rgb * w1;
    c += texture(uSource, vUV + uTexelDir * o2).rgb * w2;
    c += texture(uSource, vUV - uTexelDir * o2).rgb * w2;

    FragColor = vec4(c, 1.0);
}
