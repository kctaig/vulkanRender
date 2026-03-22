/**
 * @file ImGuiContextBuilder.h
 * @brief Declarations for the ImGuiContextBuilder module.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <Windows.h>
#include <vulkan/vulkan.h>

#include "renderer/ui/ImGuiBackend.h"

namespace vr {

class ImGuiContextBuilder {
public:
    struct Source {
        HWND windowHandle = nullptr;

        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        std::uint32_t queueFamily = 0;
        VkQueue queue = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

        std::uint32_t minImageCount = 2;
        const std::vector<VkImage>* swapchainImages = nullptr;

        VkRenderPass uiRenderPass = VK_NULL_HANDLE;
        VkSampler sceneColorSampler = VK_NULL_HANDLE;
        VkImageView sceneColorImageView = VK_NULL_HANDLE;

        void*& sceneTextureId;

        std::function<void(std::string)> appendOutput;
    };

    ImGuiBackend::InitContext build(Source&& source) const;
};

} // namespace vr


