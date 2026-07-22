#version 450

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdrInput;

layout(set = 0, binding = 1) uniform PostUBO {
    float exposure;
    float gamma;
} ubo;

vec3 acesTonemap(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(hdrInput, inTexCoord).rgb;
    vec3 mapped = acesTonemap(hdr * ubo.exposure);
    mapped = pow(mapped, vec3(1.0 / ubo.gamma));
    outColor = vec4(mapped, 1.0);
}
