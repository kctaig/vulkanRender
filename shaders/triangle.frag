#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inWorldNormal;
layout(location = 2) in vec3 inAlbedo;

// 输出到 renderpass 所绑定的 FrameBuffer 中
layout(location = 0) out vec4 outGBufferPosition;
layout(location = 1) out vec4 outGBufferNormal;
layout(location = 2) out vec4 outGBufferAlbedo;

void main() {
    outGBufferPosition = vec4(inWorldPos, 1.0);
    outGBufferNormal = vec4(normalize(inWorldNormal), 1.0);
    outGBufferAlbedo = vec4(inAlbedo, 1.0);
}
