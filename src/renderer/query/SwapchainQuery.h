/**
 * @file SwapchainQuery.h
 * @brief Declarations for the SwapchainQuery module.
 */
#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

namespace vr {

class SwapchainQuery {
public:
    struct QueueFamilyIndices {
        std::uint32_t graphicsFamily = UINT32_MAX;
        std::uint32_t presentFamily = UINT32_MAX;

        bool isComplete() const {
            return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
        }
    };

    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const;
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) const;
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes) const;
    VkExtent2D chooseSwapExtent(
        const VkSurfaceCapabilitiesKHR& capabilities,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight
    ) const;
    bool checkDeviceExtensionSupport(
        VkPhysicalDevice device,
        const std::vector<const char*>& requiredExtensions
    ) const;
    bool isDeviceSuitable(
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        const std::vector<const char*>& requiredExtensions
    ) const;
};

} // namespace vr


