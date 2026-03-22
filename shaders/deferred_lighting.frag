#version 450

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput gPosition;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput gNormal;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput gAlbedo;

layout(push_constant) uniform DebugPushConstants {
    int mode;
    int lightCount;
    float lightIntensity;
    float positionDebugScale;
    float metallic;
    float roughness;
    float ao;
    float cameraDistance;
    float materialTextureWeight;
    float iblIntensity;
    float padding0;
    float padding1;
} debugPush;

layout(location = 0) out vec4 outColor;

const int kMaxLights = 32;
const float PI = 3.14159265359;

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float numerator = a2;
    float denominator = (NdotH2 * (a2 - 1.0) + 1.0);
    denominator = PI * denominator * denominator;
    return numerator / max(denominator, 1e-5);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float numerator = NdotV;
    float denominator = NdotV * (1.0 - k) + k;
    return numerator / max(denominator, 1e-5);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 getLightPosition(int index) {
    float angle = 6.28318530718 * (float(index) / float(max(debugPush.lightCount, 1)));
    float radius = 8.0 + float(index % 4) * 2.0;
    float height = 2.0 + float(index % 3) * 1.8;
    return vec3(cos(angle) * radius, height, sin(angle) * radius);
}

vec3 getLightColor(int index) {
    float hue = fract(float(index) * 0.61803398875);
    vec3 palette = vec3(
        0.5 + 0.5 * cos(6.28318530718 * (hue + 0.00)),
        0.5 + 0.5 * cos(6.28318530718 * (hue + 0.33)),
        0.5 + 0.5 * cos(6.28318530718 * (hue + 0.67))
    );
    return mix(vec3(0.35), palette, 0.85);
}

vec3 sampleIrradiance(vec3 normal) {
    vec3 skyColor = vec3(0.25, 0.34, 0.45);
    vec3 groundColor = vec3(0.08, 0.07, 0.06);
    float factor = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
    return mix(groundColor, skyColor, factor);
}

vec3 samplePrefilter(vec3 reflectionDir, float roughness) {
    vec3 horizon = vec3(0.24, 0.3, 0.38);
    vec3 zenith = vec3(0.45, 0.52, 0.62);
    float sky = clamp(reflectionDir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 prefiltered = mix(horizon, zenith, sky);
    return mix(prefiltered, vec3(dot(prefiltered, vec3(0.3333))), roughness * roughness);
}

vec2 sampleBrdfLut(float nDotV, float roughness) {
    float a = roughness * roughness;
    float k = a * 0.5;
    float denom = max(nDotV * (1.0 - k) + k, 1e-5);
    float visibility = nDotV / denom;
    float bias = clamp((1.0 - roughness) * 0.08, 0.0, 0.08);
    return vec2(visibility, bias);
}

void main() {
    vec3 worldPos = subpassLoad(gPosition).xyz;
    float metallicFromMap = clamp(subpassLoad(gPosition).w, 0.0, 1.0);
    vec3 rawNormal = subpassLoad(gNormal).xyz;
    float roughnessFromMap = clamp(subpassLoad(gNormal).w, 0.04, 1.0);
    float rawNormalLength = length(rawNormal);
    vec3 normal = rawNormalLength > 1e-6 ? rawNormal / rawNormalLength : vec3(0.0, 1.0, 0.0);
    vec3 albedo = subpassLoad(gAlbedo).xyz;
    float aoFromMap = clamp(subpassLoad(gAlbedo).w, 0.0, 1.0);

    if (debugPush.mode == 1) {
        if (rawNormalLength <= 1e-6) {
            outColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }

        float scale = max(debugPush.positionDebugScale, 1e-3);
        vec3 positionColor = clamp(worldPos / (2.0 * scale) + vec3(0.5), vec3(0.0), vec3(1.0));
        outColor = vec4(positionColor, 1.0);
        return;
    }

    if (debugPush.mode == 2) {
        outColor = vec4(normal * 0.5 + 0.5, 1.0);
        return;
    }

    if (debugPush.mode == 3) {
        outColor = vec4(albedo, 1.0);
        return;
    }

    float mapWeight = clamp(debugPush.materialTextureWeight, 0.0, 1.0);
    float metallic = mix(clamp(debugPush.metallic, 0.0, 1.0), metallicFromMap, mapWeight);
    float roughness = mix(clamp(debugPush.roughness, 0.04, 1.0), roughnessFromMap, mapWeight);
    float ao = mix(clamp(debugPush.ao, 0.0, 1.0), aoFromMap, mapWeight);

    vec3 viewPos = vec3(0.0, 0.0, debugPush.cameraDistance);
    vec3 V = normalize(viewPos - worldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    int clampedLightCount = clamp(debugPush.lightCount, 1, kMaxLights);
    for (int i = 0; i < clampedLightCount; ++i) {
        vec3 lightPos = getLightPosition(i);
        vec3 lightColor = getLightColor(i) * debugPush.lightIntensity;

        vec3 toLight = lightPos - worldPos;
        float distanceSq = max(dot(toLight, toLight), 0.001);
        vec3 lightDir = normalize(toLight);
        vec3 H = normalize(V + lightDir);
        float NdotL = max(dot(normal, lightDir), 0.0);
        float NdotV = max(dot(normal, V), 0.0);
        float HdotV = max(dot(H, V), 0.0);
        float attenuation = 1.0 / (1.0 + 0.09 * sqrt(distanceSq) + 0.032 * distanceSq);
        vec3 radiance = lightColor * attenuation;

        float NDF = distributionGGX(normal, H, roughness);
        float G = geometrySmith(normal, V, lightDir, roughness);
        vec3 F = fresnelSchlick(HdotV, F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(NdotV, 1e-5) * max(NdotL, 1e-5);
        vec3 specular = numerator / max(denominator, 1e-5);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 N = normal;
    vec3 R = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.0);

    vec3 F = fresnelSchlick(NdotV, F0);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 irradiance = sampleIrradiance(N);
    vec3 diffuseIbl = irradiance * albedo;

    vec3 prefiltered = samplePrefilter(R, roughness);
    vec2 brdf = sampleBrdfLut(NdotV, roughness);
    vec3 specularIbl = prefiltered * (F * brdf.x + brdf.y);

    vec3 ambient = (kD * diffuseIbl + specularIbl) * ao * max(debugPush.iblIntensity, 0.0);
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
