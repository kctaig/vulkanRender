#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outWorldNormal;
layout(location = 2) out vec3 outAlbedo;

layout(set = 0, binding = 0) uniform Ubo {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    vec3 worldNormal = normalize(mat3(ubo.model) * inNormal);
    vec3 albedo = normalize(abs(inNormal));

    outWorldPos = worldPos.xyz;
    outWorldNormal = worldNormal;
    outAlbedo = albedo;
    gl_Position = ubo.projection * ubo.view * worldPos;
}
