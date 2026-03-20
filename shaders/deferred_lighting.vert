#version 450

vec2 kPositions[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

void main() {
    vec2 position = kPositions[gl_VertexIndex];
    gl_Position = vec4(position, 0.0, 1.0);
}
