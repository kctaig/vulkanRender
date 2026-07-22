#version 450

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormalRoughness;
layout(location = 2) out vec4 outMaterial;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main() {
    vec3 albedo = texture(texSampler, inUv).rgb;
    vec3 N = normalize(inWorldNormal);

    // Default PBR parameters (can be extended with material textures later)
    float roughness = 0.5;
    float metallic  = 0.0;
    float ao        = 1.0;

    outAlbedo           = vec4(albedo, 1.0);
    outNormalRoughness  = vec4(N * 0.5 + 0.5, roughness);
    outMaterial          = vec4(metallic, ao, 0.0, 1.0);
}
