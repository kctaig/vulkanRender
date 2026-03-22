/**
 * @file ImGuiContextBuilder.cpp
 * @brief Implementation for the ImGuiContextBuilder module.
 */
#include "renderer/ui/ImGuiContextBuilder.h"

namespace vr {

ImGuiBackend::InitContext ImGuiContextBuilder::build(Source&& source) const {
    const std::uint32_t imageCount = source.swapchainImages != nullptr
        ? static_cast<std::uint32_t>(source.swapchainImages->size())
        : 0;

    return ImGuiBackend::InitContext{
        .windowHandle = source.windowHandle,
        .instance = source.instance,
        .physicalDevice = source.physicalDevice,
        .device = source.device,
        .queueFamily = source.queueFamily,
        .queue = source.queue,
        .descriptorPool = source.descriptorPool,
        .minImageCount = source.minImageCount,
        .imageCount = imageCount,
        .uiRenderPass = source.uiRenderPass,
        .sceneColorSampler = source.sceneColorSampler,
        .sceneColorImageView = source.sceneColorImageView,
        .sceneTextureId = source.sceneTextureId,
        .appendOutput = std::move(source.appendOutput),
    };
}

} // namespace vr


