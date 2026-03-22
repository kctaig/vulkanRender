/**
 * @file GuiLayer.h
 * @brief Declarations for the GuiLayer module.
 */
#pragma once

#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace vr {

class GuiLayer {
public:
    struct BuildContext {
        VkExtent2D swapchainExtent{};
        void* sceneTextureId = nullptr;

        bool* sceneViewHovered = nullptr;
        bool* sceneViewFocused = nullptr;
        int* sceneViewportX = nullptr;
        int* sceneViewportY = nullptr;
        int* sceneViewportWidth = nullptr;
        int* sceneViewportHeight = nullptr;

        glm::vec3* modelTranslation = nullptr;
        float* modelYawRadians = nullptr;
        float* modelPitchRadians = nullptr;
        float* cameraDistance = nullptr;
        float sceneRadius = 1.0f;

        int* lightingDebugMode = nullptr;
        int* deferredLightCount = nullptr;
        float* deferredLightIntensity = nullptr;

        bool* useProceduralMaterialMaps = nullptr;
        float* materialMetallic = nullptr;
        float* materialRoughness = nullptr;
        float* materialAo = nullptr;
        float* iblIntensity = nullptr;

        bool modelImportInProgress = false;
        std::vector<std::string>* outputLines = nullptr;
        bool* autoScrollOutput = nullptr;

        float* smoothedFrameTimeMs = nullptr;
        float* smoothedFps = nullptr;

        std::function<void(std::string)> onAppendOutput;
        std::function<bool(std::string&)> onOpenModelDialog;
        std::function<bool(const std::string&)> onReloadModelPath;
    };

    void build(BuildContext& context) const;
};

} // namespace vr


