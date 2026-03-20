#version 450

vec3 safeNormalize(vec3 value) {
    float lengthSq = max(dot(value, value), 1e-6);
    return value * inversesqrt(lengthSq);
}
