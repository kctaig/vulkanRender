/**
 * @file FormatSelector.h
 * @brief Declarations for the FormatSelector module.
 */
#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vr {

class FormatSelector {
public:
    VkFormat findSupportedFormat(
        VkPhysicalDevice physicalDevice,
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    ) const;

    VkFormat findDepthFormat(VkPhysicalDevice physicalDevice) const;
};

} // namespace vr


