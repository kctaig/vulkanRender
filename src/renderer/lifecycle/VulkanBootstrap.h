/**
 * @file VulkanBootstrap.h
 * @brief Declarations for the VulkanBootstrap module.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>
#include <vulkan/vulkan.h>

namespace vr {

class VulkanBootstrap {
public:
    struct QueueFamilyIndices {
        std::uint32_t graphicsFamily = UINT32_MAX;
        std::uint32_t presentFamily = UINT32_MAX;

        bool isComplete() const {
            return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
        }
    };

    struct BootstrapContext {
        HINSTANCE instanceHandle = nullptr;
        HWND windowHandle = nullptr;

        bool enableValidationLayers = false;
        std::vector<const char*> instanceExtensions;
        std::vector<const char*> deviceExtensions;
        std::vector<const char*> validationLayers;

        std::function<bool(VkPhysicalDevice)> isDeviceSuitable;
        std::function<QueueFamilyIndices(VkPhysicalDevice)> findQueueFamilies;

        VkInstance& instance;
        VkDebugUtilsMessengerEXT& debugMessenger;
        VkSurfaceKHR& surface;
        VkPhysicalDevice& physicalDevice;
        VkDevice& device;
        VkQueue& graphicsQueue;
        VkQueue& presentQueue;
    };

    void createInstance(BootstrapContext& context) const;
    void setupDebugMessenger(BootstrapContext& context) const;
    void destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger) const;
    void createSurface(BootstrapContext& context) const;
    void pickPhysicalDevice(BootstrapContext& context) const;
    void createLogicalDevice(BootstrapContext& context) const;
};

} // namespace vr


