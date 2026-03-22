/**
 * @file SwapchainLifecycle.h
 * @brief Declarations for the SwapchainLifecycle module.
 */
#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vr {

class SwapchainLifecycle {
public:
    struct CleanupContext {
        VkDevice device = VK_NULL_HANDLE;

        void*& sceneTextureId;

        VkFramebuffer& sceneFramebuffer;
        std::vector<VkFramebuffer>& swapchainFramebuffers;

        VkImageView& depthImageView;
        VkImage& depthImage;
        VkDeviceMemory& depthImageMemory;

        VkImageView& gbufferPositionImageView;
        VkImage& gbufferPositionImage;
        VkDeviceMemory& gbufferPositionImageMemory;

        VkImageView& gbufferNormalImageView;
        VkImage& gbufferNormalImage;
        VkDeviceMemory& gbufferNormalImageMemory;

        VkImageView& gbufferAlbedoImageView;
        VkImage& gbufferAlbedoImage;
        VkDeviceMemory& gbufferAlbedoImageMemory;

        VkSampler& sceneColorSampler;
        VkImageView& sceneColorImageView;
        VkImage& sceneColorImage;
        VkDeviceMemory& sceneColorImageMemory;

        VkPipeline& graphicsPipeline;
        VkPipeline& lightingPipeline;
        VkPipelineLayout& pipelineLayout;
        VkPipelineLayout& lightingPipelineLayout;

        VkRenderPass& renderPass;
        VkRenderPass& uiRenderPass;

        std::vector<VkImageView>& swapchainImageViews;
        VkSwapchainKHR& swapchain;
    };

    struct SyncObjectsContext {
        VkDevice device = VK_NULL_HANDLE;
        std::vector<VkSemaphore>& renderFinishedSemaphores;
        std::size_t swapchainImageCount = 0;
    };

    void removeSceneTextureDescriptor(void*& sceneTextureId) const;
    void addSceneTextureDescriptor(void*& sceneTextureId, VkSampler sampler, VkImageView imageView) const;
    void cleanupResources(CleanupContext& context) const;
    void recreateRenderFinishedSemaphores(SyncObjectsContext& context) const;
};

} // namespace vr


