#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inWorldNormal;
layout(location = 2) in vec3 inAlbedo;
layout(location = 3) in vec2 inUv;

// 输出到 renderpass 所绑定的 FrameBuffer 中
layout(location = 0) out vec4 outGBufferPosition;
layout(location = 1) out vec4 outGBufferNormal;
layout(location = 2) out vec4 outGBufferAlbedo;

vec3 sampleProceduralBaseColor(vec2 uv) {
    vec2 tiledUv = fract(uv * 8.0);
    float checker = abs(step(0.5, tiledUv.x) - step(0.5, tiledUv.y));
    vec3 colorA = vec3(0.85, 0.82, 0.78);
    vec3 colorB = vec3(0.28, 0.33, 0.42);
    return mix(colorA, colorB, checker);
}

vec3 sampleProceduralOrm(vec2 uv) {
    float metallic = smoothstep(0.15, 0.85, fract(uv.x * 2.0));
    float roughness = mix(0.08, 0.92, fract(uv.y * 2.5));
    float ao = mix(0.7, 1.0, smoothstep(0.0, 1.0, sin(uv.x * 6.2831) * 0.5 + 0.5));
    return vec3(metallic, roughness, ao);
}

void main() {
    vec3 baseColor = sampleProceduralBaseColor(inUv);
    vec3 orm = sampleProceduralOrm(inUv);

    outGBufferPosition = vec4(inWorldPos, 1.0);
    outGBufferPosition.a = orm.r;
    outGBufferNormal = vec4(normalize(inWorldNormal), orm.g);
    outGBufferAlbedo = vec4(baseColor * inAlbedo, orm.b);
}
