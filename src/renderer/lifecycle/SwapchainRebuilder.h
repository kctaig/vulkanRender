/**
 * @file SwapchainRebuilder.h
 * @brief Declarations for the SwapchainRebuilder module.
 */
#pragma once

#include <cstdint>
#include <functional>

#include <vulkan/vulkan.h>

namespace vr {

class SwapchainRebuilder {
public:
    struct RebuildContext {
        std::uint32_t& windowWidth;
        std::uint32_t& windowHeight;
        VkDevice device = VK_NULL_HANDLE;

        std::function<bool()> processWindowMessages;
        std::function<void()> cleanupSwapchain;
        std::function<void()> createSwapchain;
        std::function<void()> createImageViews;
        std::function<void()> createRenderPass;
        std::function<void()> createGraphicsPipeline;
        std::function<void()> createLightingPipeline;
        std::function<void()> createDepthResources;
        std::function<void()> createGBufferResources;
        std::function<void()> createFramebuffers;
        std::function<void()> updateLightingDescriptorSet;
        std::function<void()> refreshSceneTextureDescriptor;
        std::function<void()> updateImGuiMinImageCount;
        std::function<void()> appendSwapchainRecreatedLog;
        std::function<void()> recreateRenderFinishedSemaphores;
        std::function<void()> resetImagesInFlight;
    };

    bool rebuild(RebuildContext& context) const;
};

} // namespace vr


