#version 450

#include "ddgiSample.glsl"

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

// GBuffer inputs (bound as combined image samplers)
layout(set = 0, binding = 0) uniform LightingUBO {
    mat4 invViewProj;
    vec4 camPos;
    vec4 lightDir;      // .xyz = direction, .w = intensity
    vec4 lightColor;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D gbufAlbedo;
layout(set = 0, binding = 2) uniform sampler2D gbufNormalRoughness;
layout(set = 0, binding = 3) uniform sampler2D gbufMaterial;
layout(set = 0, binding = 4) uniform sampler2D prepassDepth;

// DDGI inputs
layout(set = 0, binding = 5) uniform sampler2D ddgiIrradiance;
layout(set = 0, binding = 6) uniform sampler2D ddgiDepth;
layout(set = 0, binding = 7) uniform DDGIConfigUBO {
    ivec4 probeCounts;
    vec4  gridOrigin;
    float probeSpacing;
    float hysteresis;
    float depthSharpness;
    float infDistance;
    int   raysPerProbe;
    int   irradianceRes;
    int   depthRes;
    int   pad;
} ddgiConfig;

// Reconstruct world position from depth + inverse VP
vec3 reconstructWorldPos(vec2 uv, float depth, mat4 invVP) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    clip.y = -clip.y;  // Vulkan Y-flip
    vec4 world = invVP * clip;
    return world.xyz / world.w;
}

// Simple GGX distribution
float ggx(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (3.14159265 * denom * denom);
}

float smithG1(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float smithG(float NdotV, float NdotL, float roughness) {
    return smithG1(NdotV, roughness) * smithG1(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // 1. Read GBuffer
    vec3 albedo   = texture(gbufAlbedo, inTexCoord).rgb;
    vec4 nr       = texture(gbufNormalRoughness, inTexCoord);
    vec3 N        = normalize(nr.rgb * 2.0 - 1.0);
    float roughness = max(nr.a, 0.04);
    vec2 mat      = texture(gbufMaterial, inTexCoord).rg;
    float metallic = mat.r;
    float ao      = mat.g;

    float depth   = texture(prepassDepth, inTexCoord).r;

    // 2. Reconstruct world position
    vec3 worldPos = reconstructWorldPos(inTexCoord, depth, ubo.invViewProj);
    vec3 V = normalize(ubo.camPos.xyz - worldPos);

    // 3. Direct lighting (PBR Cook-Torrance with one directional light)
    vec3 L = normalize(ubo.lightDir.xyz);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float D = ggx(NdotH, roughness);
    float G = smithG(NdotV, NdotL, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / 3.14159265;

    vec3 directLight = (diffuse + specular) * ubo.lightColor.rgb *
                       ubo.lightDir.w * NdotL * ao;

    // 4. DDGI indirect diffuse
    vec3 indirectDiffuse = sampleDDGI(worldPos, N,
                                       ddgiIrradiance, ddgiDepth,
                                       ddgiConfig.gridOrigin.xyz,
                                       ddgiConfig.probeCounts.xyz,
                                       ddgiConfig.probeSpacing,
                                       ddgiConfig.depthSharpness);

    // Blend direct + indirect
    vec3 indirect = kD * indirectDiffuse * albedo * ao;
    float ddgiBlend = 0.7;  // DDGI contribution strength
    vec3 hdrColor = directLight + indirect * ddgiBlend;

    outColor = vec4(hdrColor, 1.0);
}
