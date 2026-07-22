#ifndef DDGI_SAMPLE_GLSL
#define DDGI_SAMPLE_GLSL

#include "ddgiCommon.glsl"

// Sample DDGI probe volume at a world position with trilinear interpolation.
// Returns indirect diffuse irradiance.
vec3 sampleDDGI(vec3 worldPos, vec3 normal,
                sampler2D irradianceAtlas, sampler2D depthAtlas,
                vec3 gridOrigin, ivec3 probeCounts, float probeSpacing,
                float depthSharpness) {
    vec3 localPos = (worldPos - gridOrigin) / probeSpacing;
    ivec3 baseCell = ivec3(floor(localPos));
    vec3  frac     = localPos - vec3(baseCell);

    // Clamp to valid range (need 2 probes per axis for interpolation)
    baseCell = clamp(baseCell, ivec3(0), probeCounts - ivec3(2));

    vec3 result = vec3(0.0);
    float totalWeight = 0.0;

    for (int z = 0; z <= 1; z++) {
        for (int y = 0; y <= 1; y++) {
            for (int x = 0; x <= 1; x++) {
                ivec3 cell = baseCell + ivec3(x, y, z);
                uint probeIdx = uint(cell.z * probeCounts.x * probeCounts.y
                                   + cell.y * probeCounts.x + cell.x);

                vec3 probePos = gridOrigin + vec3(cell) * probeSpacing;
                vec3 dirToFrag = worldPos - probePos;
                float fragDist = length(dirToFrag);
                vec3 dir = normalize(dirToFrag);

                // Octahedral UV for the direction from probe to fragment
                vec2 octUV = directionToOctahedralUV(dir);

                // Read irradiance + depth from atlas
                ivec2 tc = probeAtlasTexel(probeIdx, octUV, probeCounts, 8); // 8=irradianceRes
                vec3 irradiance = texelFetch(irradianceAtlas, tc, 0).rgb;
                float probeDist = texelFetch(depthAtlas, tc, 0).r;

                // Visibility weight: compare probe's stored distance vs actual distance
                float visibility = exp(-depthSharpness * max(probeDist - fragDist, 0.0));

                // Trilinear blend weight
                vec3 w = mix(vec3(1.0) - frac, frac, vec3(x, y, z));
                float weight = w.x * w.y * w.z * visibility;

                result += irradiance * weight;
                totalWeight += weight;
            }
        }
    }

    return (totalWeight > 0.0) ? result / totalWeight : vec3(0.03);
}

#endif
