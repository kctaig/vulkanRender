#include "core/VulkanContext.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace vr {

namespace {

constexpr std::array<const char*, 1> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

constexpr std::array<const char*, 2> kInstanceExtensions = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
};

#ifdef VR_ENABLE_VALIDATION
constexpr bool kEnableValidationLayers = true;
#else
constexpr bool kEnableValidationLayers = false;
#endif

constexpr std::array<const char*, 1> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

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
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback =
        [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT,
           const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*) -> VkBool32 {
        const char* severity = "INFO";
        if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
            severity = "ERROR";
        } else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0U) {
            severity = "WARN";
        }

        if (callbackData != nullptr && callbackData->pMessage != nullptr) {
            std::cerr << "[Vulkan Validation][" << severity << "] " << callbackData->pMessage
                      << "\n";
        }
        return VK_FALSE;
    };
}

bool areValidationLayersSupported() {
    std::uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* requestedLayer : kValidationLayers) {
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

}  // namespace

bool VulkanContext::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool VulkanContext::initialize(const CreateInfo& info) {
    windowHandle_ = info.windowHandle;
    instanceHandle_ = info.instanceHandle;
    validationEnabled_ = info.enableValidation && kEnableValidationLayers;
    initialWidth_ = info.initialWidth;
    initialHeight_ = info.initialHeight;
    framebufferWidth_ = info.initialWidth;
    framebufferHeight_ = info.initialHeight;

    try {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
        createSwapchain();
        createImageViews();
    } catch (const std::exception& exception) {
        std::cerr << "[VulkanContext] Initialization failed: " << exception.what() << "\n";
        return false;
    }

    return true;
}

void VulkanContext::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    cleanupSwapchain();

    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }

    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    destroyDebugMessenger();

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::setFramebufferSize(std::uint32_t width, std::uint32_t height) {
    framebufferWidth_ = width;
    framebufferHeight_ = height;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

VkInstance VulkanContext::instance() const {
    return instance_;
}
VkPhysicalDevice VulkanContext::physicalDevice() const {
    return physicalDevice_;
}
VkDevice VulkanContext::device() const {
    return device_;
}
VkQueue VulkanContext::graphicsQueue() const {
    return graphicsQueue_;
}
VkQueue VulkanContext::presentQueue() const {
    return presentQueue_;
}
VkSurfaceKHR VulkanContext::surface() const {
    return surface_;
}
VkCommandPool VulkanContext::commandPool() const {
    return commandPool_;
}
VkSwapchainKHR VulkanContext::swapchain() const {
    return swapchain_;
}
VkFormat VulkanContext::swapchainFormat() const {
    return swapchainImageFormat_;
}
VkExtent2D VulkanContext::swapchainExtent() const {
    return swapchainExtent_;
}
std::uint32_t VulkanContext::swapchainMinImageCount() const {
    return swapchainMinImageCount_;
}
std::uint32_t VulkanContext::swapchainImageCount() const {
    return static_cast<std::uint32_t>(swapchainImages_.size());
}
const std::vector<VkImage>& VulkanContext::swapchainImages() const {
    return swapchainImages_;
}
const std::vector<VkImageView>& VulkanContext::swapchainImageViews() const {
    return swapchainImageViews_;
}
bool VulkanContext::validationEnabled() const {
    return validationEnabled_;
}

// ---------------------------------------------------------------------------
// Swapchain operations
// ---------------------------------------------------------------------------

bool VulkanContext::acquireNextImage(VkSemaphore signalSemaphore, std::uint32_t& outImageIndex) {
    VkResult result = vkAcquireNextImageKHR(
        device_, swapchain_, UINT64_MAX, signalSemaphore, VK_NULL_HANDLE, &outImageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return false;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("vkAcquireNextImageKHR failed");
    }

    return true;
}

void VulkanContext::recreateSwapchain() {
    vkDeviceWaitIdle(device_);
    cleanupSwapchain();
    createSwapchain();
    createImageViews();
}

void VulkanContext::cleanupSwapchain() {
    for (VkImageView imageView : swapchainImageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    swapchainImageViews_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

// ---------------------------------------------------------------------------
// Resource creation helpers
// ---------------------------------------------------------------------------

std::uint32_t VulkanContext::findMemoryType(
    std::uint32_t typeFilter, VkMemoryPropertyFlags properties
) const {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);

    for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if (((typeFilter & (1U << i)) != 0U) &&
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type");
}

VkFormat VulkanContext::findSupportedFormat(
    const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features
) const {
    for (VkFormat format : candidates) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &properties);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (properties.linearTilingFeatures & features) == features) {
            return format;
        }

        if (tiling == VK_IMAGE_TILING_OPTIMAL &&
            (properties.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("no supported format found");
}

VkFormat VulkanContext::findDepthFormat() const {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

void VulkanContext::createBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer,
    VkDeviceMemory& bufferMemory
) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateBuffer failed");
    }

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device_, buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &allocateInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateMemory failed");
    }

    vkBindBufferMemory(device_, buffer, bufferMemory, 0);
}

void VulkanContext::createImage(
    std::uint32_t width, std::uint32_t height, VkFormat format, VkImageTiling tiling,
    VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image,
    VkDeviceMemory& imageMemory
) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device_, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImage failed");
    }

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(device_, image, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &allocateInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateMemory for image failed");
    }

    vkBindImageMemory(device_, image, imageMemory, 0);
}

VkImageView VulkanContext::createImageView(
    VkImage image, VkFormat format, VkImageAspectFlags aspectFlags
) const {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    if (vkCreateImageView(device_, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImageView failed");
    }

    return imageView;
}

VkShaderModule VulkanContext::createShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateShaderModule failed");
    }

    return shaderModule;
}

// ---------------------------------------------------------------------------
// Command buffer allocation
// ---------------------------------------------------------------------------

std::vector<VkCommandBuffer> VulkanContext::allocateCommandBuffers(std::uint32_t count) const {
    std::vector<VkCommandBuffer> buffers(count);

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = count;

    if (vkAllocateCommandBuffers(device_, &allocateInfo, buffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateCommandBuffers failed");
    }

    return buffers;
}

// ---------------------------------------------------------------------------
// Query helpers
// ---------------------------------------------------------------------------

VulkanContext::QueueFamilyIndices VulkanContext::findQueueFamilies() const {
    return findQueueFamilies(physicalDevice_);
}

VulkanContext::QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (std::uint32_t i = 0; i < queueFamilyCount; ++i) {
        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
        if (presentSupport == VK_TRUE) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

VulkanContext::SwapchainSupportDetails VulkanContext::querySwapchainSupport() const {
    return querySwapchainSupport(physicalDevice_);
}

VulkanContext::SwapchainSupportDetails VulkanContext::querySwapchainSupport(
    VkPhysicalDevice device
) const {
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
    if (formatCount > 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, surface_, &formatCount, details.formats.data()
        );
    }

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
    if (presentModeCount > 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface_, &presentModeCount, details.presentModes.data()
        );
    }

    return details;
}

// ---------------------------------------------------------------------------
// Static utilities
// ---------------------------------------------------------------------------

std::vector<char> VulkanContext::readBinaryFile(const char* filePath) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("failed to open file: ") + filePath);
    }

    std::streamsize fileSize = file.tellg();
    std::vector<char> buffer(static_cast<std::size_t>(fileSize));
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    return buffer;
}

// ---------------------------------------------------------------------------
// Private: Vulkan setup
// ---------------------------------------------------------------------------

void VulkanContext::createInstance() {
    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Vulkan Renderer";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    applicationInfo.pEngineName = "VR";
    applicationInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_2;

    std::vector<const char*> enabledInstanceExtensions(
        kInstanceExtensions.begin(), kInstanceExtensions.end()
    );
    const bool shouldEnableValidation = validationEnabled_ && areValidationLayersSupported();
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
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
        if (supportsDebugUtils) {
            createInfo.pNext = &debugCreateInfo;
        }
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateInstance failed");
    }

    if (shouldEnableValidation && !supportsDebugUtils) {
        std::cerr
            << "[VulkanContext] VK_EXT_debug_utils not available, validation callback disabled\n";
    }
}

void VulkanContext::setupDebugMessenger() {
    if (!validationEnabled_ || !areValidationLayersSupported()) {
        return;
    }

    if (!isInstanceExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    auto createDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT")
    );
    if (createDebugUtilsMessenger == nullptr) {
        throw std::runtime_error("vkCreateDebugUtilsMessengerEXT not found");
    }

    if (createDebugUtilsMessenger(instance_, &createInfo, nullptr, &debugMessenger_) !=
        VK_SUCCESS) {
        throw std::runtime_error("vkCreateDebugUtilsMessengerEXT failed");
    }

    std::cerr << "[VulkanContext] Vulkan validation enabled (debug messenger active)\n";
}

void VulkanContext::destroyDebugMessenger() {
    if (debugMessenger_ == VK_NULL_HANDLE || instance_ == VK_NULL_HANDLE) {
        return;
    }

    auto destroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT")
    );
    if (destroyDebugUtilsMessenger != nullptr) {
        destroyDebugUtilsMessenger(instance_, debugMessenger_, nullptr);
    }
    debugMessenger_ = VK_NULL_HANDLE;
}

void VulkanContext::createSurface() {
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = instanceHandle_;
    createInfo.hwnd = windowHandle_;

    if (vkCreateWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateWin32SurfaceKHR failed");
    }
}

void VulkanContext::pickPhysicalDevice() {
    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("no Vulkan physical device found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (VkPhysicalDevice device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice_ = device;
            break;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("no suitable Vulkan physical device found");
    }
}

void VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies();

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
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(kDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    if (validationEnabled_ && areValidationLayersSupported()) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDevice failed");
    }

    vkGetDeviceQueue(device_, indices.graphicsFamily, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices.presentFamily, 0, &presentQueue_);
}

void VulkanContext::createSwapchain() {
    SwapchainSupportDetails support = querySwapchainSupport();

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);
    VkExtent2D extent = chooseSwapExtent(support.capabilities);

    std::uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }
    swapchainMinImageCount_ = support.capabilities.minImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies();
    std::uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateSwapchainKHR failed");
    }

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());

    swapchainImageFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
}

void VulkanContext::createImageViews() {
    swapchainImageViews_.resize(swapchainImages_.size());

    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
        swapchainImageViews_[i] =
            createImageView(swapchainImages_[i], swapchainImageFormat_, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanContext::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateCommandPool failed");
    }
}

// ---------------------------------------------------------------------------
// Private: device suitability checks
// ---------------------------------------------------------------------------

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) const {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapchainAdequate = false;
    if (extensionsSupported) {
        SwapchainSupportDetails supportDetails = querySwapchainSupport(device);
        swapchainAdequate = !supportDetails.formats.empty() && !supportDetails.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapchainAdequate;
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    std::uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(
        device, nullptr, &extensionCount, availableExtensions.data()
    );

    std::set<std::string> requiredExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

VkSurfaceFormatKHR VulkanContext::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats
) const {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return formats.front();
}

VkPresentModeKHR VulkanContext::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& presentModes
) const {
    for (const auto& presentMode : presentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanContext::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent = {framebufferWidth_, framebufferHeight_};

    actualExtent.width = std::clamp(
        actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width
    );
    actualExtent.height = std::clamp(
        actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height
    );

    return actualExtent;
}

}  // namespace vr
