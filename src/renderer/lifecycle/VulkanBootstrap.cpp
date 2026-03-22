/**
 * @file VulkanBootstrap.cpp
 * @brief Implementation for the VulkanBootstrap module.
 */
#include "renderer/lifecycle/VulkanBootstrap.h"

#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>

namespace vr {

namespace {

bool isInstanceExtensionSupported(const char* extensionName) {
    std::uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    for (const VkExtensionProperties& extension : availableExtensions) {
        if (std::strcmp(extension.extensionName, extensionName) == 0) {
            return true;
        }
    }

    return false;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                    VkDebugUtilsMessageTypeFlagsEXT,
                                    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                    void*) -> VkBool32 {
        const char* severity = "INFO";
        if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
            severity = "ERROR";
        } else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0U) {
            severity = "WARN";
        }

        if (callbackData != nullptr && callbackData->pMessage != nullptr) {
            std::cerr << "[Vulkan Validation][" << severity << "] " << callbackData->pMessage << "\n";
        }
        return VK_FALSE;
    };
}

bool areValidationLayersSupported(const std::vector<const char*>& validationLayers) {
    std::uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* requestedLayer : validationLayers) {
        bool found = false;
        for (const auto& layerProperties : availableLayers) {
            if (std::strcmp(requestedLayer, layerProperties.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

} // namespace

void VulkanBootstrap::createInstance(BootstrapContext& context) const {
    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Vulkan Renderer";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    applicationInfo.pEngineName = "VR";
    applicationInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_2;

    const bool supportsValidationLayers = areValidationLayersSupported(context.validationLayers);
    const bool shouldEnableValidation = context.enableValidationLayers && supportsValidationLayers;

    std::vector<const char*> enabledInstanceExtensions = context.instanceExtensions;
    const bool supportsDebugUtils = isInstanceExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (shouldEnableValidation && supportsDebugUtils) {
        enabledInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (shouldEnableValidation && supportsDebugUtils) {
        populateDebugMessengerCreateInfo(debugCreateInfo);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(enabledInstanceExtensions.size());
    createInfo.ppEnabledExtensionNames = enabledInstanceExtensions.data();

    if (shouldEnableValidation) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(context.validationLayers.size());
        createInfo.ppEnabledLayerNames = context.validationLayers.data();
        if (supportsDebugUtils) {
            createInfo.pNext = &debugCreateInfo;
        }
    }

    if (vkCreateInstance(&createInfo, nullptr, &context.instance) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateInstance failed");
    }

    if (shouldEnableValidation && !supportsDebugUtils) {
        std::cerr << "[Renderer] VK_EXT_debug_utils not available, validation callback disabled\n";
    }
}

void VulkanBootstrap::setupDebugMessenger(BootstrapContext& context) const {
    if (!context.enableValidationLayers || !areValidationLayersSupported(context.validationLayers)) {
        return;
    }

    if (!isInstanceExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    auto createDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(context.instance, "vkCreateDebugUtilsMessengerEXT")
    );
    if (createDebugUtilsMessenger == nullptr) {
        throw std::runtime_error("vkCreateDebugUtilsMessengerEXT not found");
    }

    if (createDebugUtilsMessenger(context.instance, &createInfo, nullptr, &context.debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDebugUtilsMessengerEXT failed");
    }

    std::cerr << "[Renderer] Vulkan validation enabled (debug messenger active)\n";
}

void VulkanBootstrap::destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger) const {
    if (debugMessenger == VK_NULL_HANDLE || instance == VK_NULL_HANDLE) {
        return;
    }

    auto destroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
    );
    if (destroyDebugUtilsMessenger != nullptr) {
        destroyDebugUtilsMessenger(instance, debugMessenger, nullptr);
    }
    debugMessenger = VK_NULL_HANDLE;
}

void VulkanBootstrap::createSurface(BootstrapContext& context) const {
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = context.instanceHandle;
    createInfo.hwnd = context.windowHandle;

    if (vkCreateWin32SurfaceKHR(context.instance, &createInfo, nullptr, &context.surface) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateWin32SurfaceKHR failed");
    }
}

void VulkanBootstrap::pickPhysicalDevice(BootstrapContext& context) const {
    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(context.instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("no Vulkan physical device found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(context.instance, &deviceCount, devices.data());

    for (VkPhysicalDevice device : devices) {
        if (context.isDeviceSuitable(device)) {
            context.physicalDevice = device;
            break;
        }
    }

    if (context.physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("no suitable Vulkan physical device found");
    }
}

void VulkanBootstrap::createLogicalDevice(BootstrapContext& context) const {
    QueueFamilyIndices indices = context.findQueueFamilies(context.physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<std::uint32_t> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

    constexpr float queuePriority = 1.0f;
    for (std::uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(context.deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = context.deviceExtensions.data();

    if (context.enableValidationLayers && areValidationLayersSupported(context.validationLayers)) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(context.validationLayers.size());
        createInfo.ppEnabledLayerNames = context.validationLayers.data();
    }

    if (vkCreateDevice(context.physicalDevice, &createInfo, nullptr, &context.device) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDevice failed");
    }

    vkGetDeviceQueue(context.device, indices.graphicsFamily, 0, &context.graphicsQueue);
    vkGetDeviceQueue(context.device, indices.presentFamily, 0, &context.presentQueue);
}

} // namespace vr


