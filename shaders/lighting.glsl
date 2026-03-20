#version 450

#include "pbr.glsl"
#include "shadow.glsl"

vec3 deferredLighting(
    vec3 normal,
    vec3 albedo,
    float metallic,
    float roughness,
    vec3 viewDir,
    vec3 lightDir,
    sampler2D shadowMap,
    vec2 shadowUv,
    float shadowDepth
) {
    vec3 brdf = cookTorranceBRDF(normal, viewDir, lightDir, albedo, metallic, roughness);
    float visibility = sampleShadowPCF(shadowMap, shadowUv, shadowDepth);
    return brdf * visibility;
}
