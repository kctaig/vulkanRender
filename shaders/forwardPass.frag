#version 450

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main() {
    vec3 baseColor = texture(texSampler, inUv).rgb;

    // Simple diffuse: light from camera direction
    vec3 N = normalize(inWorldNormal);
    vec3 L = normalize(vec3(1.0, 1.0, 2.0));
    float diff = max(dot(N, L), 0.0);

    vec3 color = baseColor * diff;
    outColor = vec4(color, 1.0);
}
