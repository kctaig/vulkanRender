#ifndef DDGI_COMMON_GLSL
#define DDGI_COMMON_GLSL

const float PI = 3.14159265359;

// --- Octahedral encoding / decoding ---

vec2 directionToOctahedralUV(vec3 d) {
    d /= abs(d.x) + abs(d.y) + abs(d.z);
    vec2 uv = (d.z >= 0.0) ? d.xy : (1.0 - abs(d.yx)) * sign(d.xy);
    return uv * 0.5 + 0.5;
}

vec3 octahedralToDirection(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    vec3 d = vec3(uv, 1.0 - abs(uv.x) - abs(uv.y));
    if (d.z < 0.0) {
        vec2 q = (1.0 - abs(d.yx)) * sign(d.xy);
        d.xy = q;
    }
    return normalize(d);
}

// --- Low-discrepancy sequence for ray jittering ---

float radicalInverseBase2(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;  // / 2^32
}

vec2 hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), radicalInverseBase2(i));
}

// --- Cosine-weighted hemisphere sampling ---

vec3 cosineSampleHemisphere(vec3 N, vec2 rnd) {
    float r = sqrt(rnd.x);
    float theta = 2.0 * PI * rnd.y;
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(1.0 - rnd.x);

    vec3 T = abs(N.y) < 0.999 ? normalize(cross(vec3(0.0, 1.0, 0.0), N))
                              : normalize(cross(vec3(1.0, 0.0, 0.0), N));
    vec3 B = cross(N, T);
    return normalize(T * x + B * y + N * z);
}

// --- Probe atlas coordinate helpers ---

// Convert a flat probe index + octahedral UV to atlas pixel coordinate
ivec2 probeAtlasTexel(uint probeIdx, vec2 octUV, ivec3 probeCounts, int res) {
    int probeX = int(probeIdx) % probeCounts.x;
    int probeYZ = int(probeIdx) / probeCounts.x;
    int probeY = probeYZ % probeCounts.y;
    int probeZ = probeYZ / probeCounts.y;

    int texelX = int(octUV.x * float(res)) + probeX * res;
    int texelY = int(octUV.y * float(res)) + (probeY + probeZ * probeCounts.y) * res;
    return ivec2(texelX, texelY);
}

vec3 getProbeWorldPos(uint probeIdx, vec3 gridOrigin, float spacing, ivec3 probeCounts) {
    int px = int(probeIdx) % probeCounts.x;
    int yz = int(probeIdx) / probeCounts.x;
    int py = yz % probeCounts.y;
    int pz = yz / probeCounts.y;
    return gridOrigin + vec3(float(px), float(py), float(pz)) * spacing;
}

#endif
