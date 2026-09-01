#version 410 core
// Fullscreen triangle technique - no vertex attributes needed
// Generates triangle covering entire NDC: (-1,-1), (3,-1), (-1,3)
out vec2 vUV;

void main() {
    // Vertex IDs 0,1,2 -> positions
    vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vec2 pos = positions[gl_VertexID];
    gl_Position = vec4(pos, 0.0, 1.0);
    // UV in [0,1] for convenience
    vUV = pos * 0.5 + 0.5;
}
