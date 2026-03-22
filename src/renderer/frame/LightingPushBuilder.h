/**
 * @file LightingPushBuilder.h
 * @brief Declarations for the LightingPushBuilder module.
 */
#pragma once

#include <algorithm>

namespace vr {

class LightingPushBuilder {
public:
    struct Source {
        int lightingDebugMode = 0;
        int deferredLightCount = 8;
        float deferredLightIntensity = 2.0f;
        float sceneRadius = 1.0f;
        float materialMetallic = 0.1f;
        float materialRoughness = 0.6f;
        float materialAo = 1.0f;
        float cameraDistance = 3.5f;
        bool useProceduralMaterialMaps = true;
        float iblIntensity = 1.0f;
    };

    template <typename PushConstantsT>
    PushConstantsT build(const Source& source) const {
        PushConstantsT pushConstants{};
        pushConstants.debugMode = source.lightingDebugMode;
        pushConstants.lightCount = std::clamp(source.deferredLightCount, 1, 32);
        pushConstants.lightIntensity = std::max(0.1f, source.deferredLightIntensity);
        pushConstants.positionDebugScale = std::max(1.0f, source.sceneRadius * 2.0f);
        pushConstants.metallic = std::clamp(source.materialMetallic, 0.0f, 1.0f);
        pushConstants.roughness = std::clamp(source.materialRoughness, 0.04f, 1.0f);
        pushConstants.ao = std::clamp(source.materialAo, 0.0f, 1.0f);
        pushConstants.cameraDistance = source.cameraDistance;
        pushConstants.materialTextureWeight = source.useProceduralMaterialMaps ? 1.0f : 0.0f;
        pushConstants.iblIntensity = std::max(0.0f, source.iblIntensity);
        return pushConstants;
    }
};

} // namespace vr


