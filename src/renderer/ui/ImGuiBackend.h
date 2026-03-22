/**
 * @file ImGuiBackend.h
 * @brief Declarations for the ImGuiBackend module.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <Windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

namespace vr {

class ImGuiBackend {
public:
    struct InitContext {
        HWND windowHandle = nullptr;

        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        std::uint32_t queueFamily = 0;
        VkQueue queue = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

        std::uint32_t minImageCount = 2;
        std::uint32_t imageCount = 2;

        VkRenderPass uiRenderPass = VK_NULL_HANDLE;
        VkSampler sceneColorSampler = VK_NULL_HANDLE;
        VkImageView sceneColorImageView = VK_NULL_HANDLE;

        void*& sceneTextureId;

        std::function<void(std::string)> appendOutput;
    };

    void createDescriptorPool(VkDevice device, VkDescriptorPool& descriptorPool) const;
    void init(InitContext& context) const;
    void beginFrame() const;
    void endFrame() const;
    void shutdown(VkDevice device, VkDescriptorPool& descriptorPool, void*& sceneTextureId) const;
};

} // namespace vr


