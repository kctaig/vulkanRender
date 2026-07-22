#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
} global;

layout(push_constant) uniform PushConst {
    mat4 model;
} pc;

void main() {
    gl_Position = global.proj * global.view * pc.model * vec4(inPosition, 1.0);
}
