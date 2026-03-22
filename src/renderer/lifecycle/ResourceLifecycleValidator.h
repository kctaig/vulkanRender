/**
 * @file ResourceLifecycleValidator.h
 * @brief Declarations for the ResourceLifecycleValidator module.
 */
#pragma once

#include <vector>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>
#include <vulkan/vulkan.h>

namespace vr {

class ResourceLifecycleValidator {
public:
    struct ShutdownContext {
        VkInstance instance = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        HWND windowHandle = nullptr;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool lightingDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout lightingDescriptorSetLayout = VK_NULL_HANDLE;

        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

        VkCommandPool commandPool = VK_NULL_HANDLE;

        std::vector<VkSemaphore>* renderFinishedSemaphores = nullptr;
    };

    void validateSyncCollections(
        std::size_t swapchainImageCount,
        const std::vector<VkSemaphore>& renderFinishedSemaphores,
        const std::vector<VkFence>& imagesInFlight
    ) const;

    void validateShutdownCleared(const ShutdownContext& context) const;
};

} // namespace vr


