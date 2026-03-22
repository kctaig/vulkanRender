/**
 * @file RenderTargetsBuilder.h
 * @brief Declarations for the RenderTargetsBuilder module.
 */
#pragma once

#include <functional>
#include <vector>

#include <vulkan/vulkan.h>

namespace vr {

class RenderTargetsBuilder {
public:
    /** @brief Inputs/outputs required to allocate depth image resources. */
    struct DepthContext {
        VkExtent2D extent{};
        VkFormat& depthFormat;
        VkImage& depthImage;
        VkDeviceMemory& depthImageMemory;
        VkImageView& depthImageView;

        std::function<VkFormat()> findDepthFormat;
        std::function<bool(VkFormat)> hasStencilComponent;
        std::function<void(
            std::uint32_t,
            std::uint32_t,
            VkFormat,
            VkImageTiling,
            VkImageUsageFlags,
            VkMemoryPropertyFlags,
            VkImage&,
            VkDeviceMemory&
        )>
            createImage;
        std::function<VkImageView(VkImage, VkFormat, VkImageAspectFlags)> createImageView;
    };

    /** @brief Inputs/outputs required to allocate scene color and GBuffer resources. */
    struct GBufferContext {
        VkDevice device = VK_NULL_HANDLE;
        VkExtent2D extent{};

        VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
        VkFormat gbufferPositionFormat = VK_FORMAT_UNDEFINED;
        VkFormat gbufferNormalFormat = VK_FORMAT_UNDEFINED;
        VkFormat gbufferAlbedoFormat = VK_FORMAT_UNDEFINED;

        VkImage& sceneColorImage;
        VkDeviceMemory& sceneColorImageMemory;
        VkImageView& sceneColorImageView;
        VkSampler& sceneColorSampler;

        VkImage& gbufferPositionImage;
        VkDeviceMemory& gbufferPositionImageMemory;
        VkImageView& gbufferPositionImageView;

        VkImage& gbufferNormalImage;
        VkDeviceMemory& gbufferNormalImageMemory;
        VkImageView& gbufferNormalImageView;

        VkImage& gbufferAlbedoImage;
        VkDeviceMemory& gbufferAlbedoImageMemory;
        VkImageView& gbufferAlbedoImageView;

        std::function<void(
            std::uint32_t,
            std::uint32_t,
            VkFormat,
            VkImageTiling,
            VkImageUsageFlags,
            VkMemoryPropertyFlags,
            VkImage&,
            VkDeviceMemory&
        )>
            createImage;
        std::function<VkImageView(VkImage, VkFormat, VkImageAspectFlags)> createImageView;
    };

    /** @brief Inputs/outputs required to create scene and present framebuffers. */
    struct FramebufferContext {
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkRenderPass uiRenderPass = VK_NULL_HANDLE;
        VkExtent2D extent{};

        VkImageView sceneColorImageView = VK_NULL_HANDLE;
        VkImageView depthImageView = VK_NULL_HANDLE;
        VkImageView gbufferPositionImageView = VK_NULL_HANDLE;
        VkImageView gbufferNormalImageView = VK_NULL_HANDLE;
        VkImageView gbufferAlbedoImageView = VK_NULL_HANDLE;

        std::vector<VkImageView>& swapchainImageViews;
        VkFramebuffer& sceneFramebuffer;
        std::vector<VkFramebuffer>& swapchainFramebuffers;
    };

    /** @brief Creates depth image, memory, and view. */
    void createDepthResources(DepthContext& context) const;
    /** @brief Creates scene color target and all GBuffer attachments. */
    void createGBufferResources(GBufferContext& context) const;
    /** @brief Creates framebuffers for scene rendering and swapchain presentation. */
    void createFramebuffers(FramebufferContext& context) const;
};

} // namespace vr


