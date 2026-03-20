#version 450

#include "common.glsl"

vec3 cookTorranceBRDF(vec3 normal, vec3 viewDir, vec3 lightDir, vec3 albedo, float metallic, float roughness) {
    vec3 halfDir = safeNormalize(viewDir + lightDir);
    float ndotl = max(dot(normal, lightDir), 0.0);
    float ndotv = max(dot(normal, viewDir), 0.0);
    float ndoth = max(dot(normal, halfDir), 0.0);

    float distribution = ndoth * ndoth;
    float visibility = ndotl * ndotv;
    vec3 fresnel = mix(vec3(0.04), albedo, metallic);

    return (distribution * visibility) * fresnel;
}
