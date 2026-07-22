#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 outWorldNormal;
layout(location = 1) out vec2 outUv;
layout(location = 2) out vec3 outWorldPos;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
} global;

layout(push_constant) uniform PushConst {
    mat4 model;
} pc;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = global.proj * global.view * worldPos;
    outWorldNormal = normalize(mat3(pc.model) * inNormal);
    outUv = inUv;
    outWorldPos = worldPos.xyz;
}
