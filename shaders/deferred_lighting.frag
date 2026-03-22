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

void main() {
    vec3 worldPos = subpassLoad(gPosition).xyz;
    vec3 rawNormal = subpassLoad(gNormal).xyz;
    float rawNormalLength = length(rawNormal);
    vec3 normal = rawNormalLength > 1e-6 ? rawNormal / rawNormalLength : vec3(0.0, 1.0, 0.0);
    vec3 albedo = subpassLoad(gAlbedo).xyz;

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

    float metallic = clamp(debugPush.metallic, 0.0, 1.0);
    float roughness = clamp(debugPush.roughness, 0.04, 1.0);
    float ao = clamp(debugPush.ao, 0.0, 1.0);

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

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
