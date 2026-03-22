/**
 * @file GuiContextBuilder.cpp
 * @brief Implementation for the GuiContextBuilder module.
 */
#include "renderer/ui/GuiContextBuilder.h"

namespace vr {

GuiLayer::BuildContext GuiContextBuilder::build(Source&& source) const {
    return GuiLayer::BuildContext{
        .swapchainExtent = source.swapchainExtent,
        .sceneTextureId = source.sceneTextureId,
        .sceneViewHovered = source.sceneViewHovered,
        .sceneViewFocused = source.sceneViewFocused,
        .sceneViewportX = source.sceneViewportX,
        .sceneViewportY = source.sceneViewportY,
        .sceneViewportWidth = source.sceneViewportWidth,
        .sceneViewportHeight = source.sceneViewportHeight,
        .modelTranslation = source.modelTranslation,
        .modelYawRadians = source.modelYawRadians,
        .modelPitchRadians = source.modelPitchRadians,
        .cameraDistance = source.cameraDistance,
        .sceneRadius = source.sceneRadius,
        .lightingDebugMode = source.lightingDebugMode,
        .deferredLightCount = source.deferredLightCount,
        .deferredLightIntensity = source.deferredLightIntensity,
        .useProceduralMaterialMaps = source.useProceduralMaterialMaps,
        .materialMetallic = source.materialMetallic,
        .materialRoughness = source.materialRoughness,
        .materialAo = source.materialAo,
        .iblIntensity = source.iblIntensity,
        .modelImportInProgress = source.modelImportInProgress,
        .outputLines = source.outputLines,
        .autoScrollOutput = source.autoScrollOutput,
        .smoothedFrameTimeMs = source.smoothedFrameTimeMs,
        .smoothedFps = source.smoothedFps,
        .onAppendOutput = std::move(source.onAppendOutput),
        .onOpenModelDialog = std::move(source.onOpenModelDialog),
        .onReloadModelPath = std::move(source.onReloadModelPath),
    };
}

} // namespace vr


