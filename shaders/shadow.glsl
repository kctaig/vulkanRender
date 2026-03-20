#version 450

float sampleShadowPCF(sampler2D shadowMap, vec2 uv, float compareDepth) {
    float shadow = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(x, y) * 0.0015;
            float depth = texture(shadowMap, uv + offset).r;
            shadow += compareDepth > depth ? 0.0 : 1.0;
        }
    }
    return shadow / 9.0;
}
