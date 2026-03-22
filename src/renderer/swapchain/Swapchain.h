/**
 * @file Swapchain.h
 * @brief Declarations for the Swapchain module.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <vulkan/vulkan.h>

namespace vr {

class Swapchain {
public:
    /** @brief Queue family indices required for swapchain sharing mode decisions. */
    struct QueueFamilyIndices {
        std::uint32_t graphicsFamily = UINT32_MAX;
        std::uint32_t presentFamily = UINT32_MAX;
    };

    /** @brief Surface capability snapshot used during swapchain creation. */
    struct SupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    /** @brief Input/output bundle used when creating a Vulkan swapchain. */
    struct CreateContext {
        VkDevice device = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        SupportDetails support;
        QueueFamilyIndices queueFamilies;

        std::function<VkSurfaceFormatKHR(const std::vector<VkSurfaceFormatKHR>&)> chooseSurfaceFormat;
        std::function<VkPresentModeKHR(const std::vector<VkPresentModeKHR>&)> choosePresentMode;
        std::function<VkExtent2D(const VkSurfaceCapabilitiesKHR&)> chooseExtent;

        VkSwapchainKHR& swapchain;
        std::vector<VkImage>& swapchainImages;
        VkFormat& swapchainImageFormat;
        VkExtent2D& swapchainExtent;
        std::uint32_t& swapchainMinImageCount;
    };

    /** @brief Input/output bundle used when creating swapchain image views. */
    struct ImageViewsContext {
        std::vector<VkImage>& swapchainImages;
        VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
        std::vector<VkImageView>& swapchainImageViews;
        std::function<VkImageView(VkImage, VkFormat, VkImageAspectFlags)> createImageView;
    };

    /** @brief Creates the swapchain and updates all output references in context. */
    void create(CreateContext& context) const;
    /** @brief Creates image views for every swapchain image. */
    void createImageViews(ImageViewsContext& context) const;

private:
    /** @brief Resolves desired swapchain image count within surface limits. */
    static std::uint32_t resolveImageCount(const VkSurfaceCapabilitiesKHR& capabilities);
};

} // namespace vr


