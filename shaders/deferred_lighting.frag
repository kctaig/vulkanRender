#version 450

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput gPosition;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput gNormal;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput gAlbedo;

layout(push_constant) uniform DebugPushConstants {
    int mode;
    int lightCount;
    float lightIntensity;
    float padding;
} debugPush;

layout(location = 0) out vec4 outColor;

const int kMaxLights = 32;

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
    vec3 normal = normalize(subpassLoad(gNormal).xyz);
    vec3 albedo = subpassLoad(gAlbedo).xyz;

    if (debugPush.mode == 1) {
        outColor = vec4(worldPos, 1.0);
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

    vec3 ambient = 0.08 * albedo;
    vec3 lighting = ambient;

    int clampedLightCount = clamp(debugPush.lightCount, 1, kMaxLights);
    for (int i = 0; i < clampedLightCount; ++i) {
        vec3 lightPos = getLightPosition(i);
        vec3 lightColor = getLightColor(i) * debugPush.lightIntensity;

        vec3 toLight = lightPos - worldPos;
        float distanceSq = max(dot(toLight, toLight), 0.001);
        vec3 lightDir = normalize(toLight);
        float nDotL = max(dot(normal, lightDir), 0.0);

        float attenuation = 1.0 / (1.0 + 0.09 * sqrt(distanceSq) + 0.032 * distanceSq);
        lighting += albedo * lightColor * nDotL * attenuation;
    }

    outColor = vec4(lighting, 1.0);
}
