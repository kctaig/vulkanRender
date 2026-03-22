/**
 * @file BufferImageAllocator.h
 * @brief Declarations for the BufferImageAllocator module.
 */
#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace vr {

class BufferImageAllocator {
public:
    std::uint32_t findMemoryType(
        VkPhysicalDevice physicalDevice,
        std::uint32_t typeFilter,
        VkMemoryPropertyFlags properties
    ) const;

    void createBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    ) const;

    void createImage(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        std::uint32_t width,
        std::uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory
    ) const;

    VkImageView createImageView(
        VkDevice device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectFlags
    ) const;
};

} // namespace vr


