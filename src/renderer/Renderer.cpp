#include "renderer/Renderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_win32.h>

#include <commdlg.h>
#include <Windowsx.h>

#include <glm/gtc/matrix_transform.hpp>

#include "scene/MeshIO.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace vr {

namespace {

constexpr std::array<const char*, 1> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

constexpr std::array<const char*, 2> kInstanceExtensions = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
};

constexpr std::array<Renderer::Vertex, 3> kFallbackVertices = {
    Renderer::Vertex{{0.0f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}},
    Renderer::Vertex{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    Renderer::Vertex{{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
};

constexpr std::array<std::uint32_t, 3> kFallbackIndices = {0U, 1U, 2U};

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

bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

int imguiWin32CreateVkSurface(ImGuiViewport* viewport, ImU64 vkInstance, const void* vkAllocators, ImU64* outVkSurface) {
    if (viewport == nullptr || outVkSurface == nullptr || viewport->PlatformHandleRaw == nullptr) {
        return static_cast<int>(VK_ERROR_INITIALIZATION_FAILED);
    }

    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandle(nullptr);
    createInfo.hwnd = static_cast<HWND>(viewport->PlatformHandleRaw);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const VkResult result = vkCreateWin32SurfaceKHR(
        reinterpret_cast<VkInstance>(static_cast<uintptr_t>(vkInstance)),
        &createInfo,
        static_cast<const VkAllocationCallbacks*>(vkAllocators),
        &surface
    );

    *outVkSurface = static_cast<ImU64>(reinterpret_cast<uintptr_t>(surface));
    return static_cast<int>(result);
}

} // namespace

VkVertexInputBindingDescription Renderer::Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Renderer::Vertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributes{};

    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(Vertex, position);

    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(Vertex, normal);

    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[2].offset = offsetof(Vertex, uv);

    return attributes;
}

void Renderer::setMeshInputPath(std::string path) {
    meshInputPath_ = std::move(path);
}

bool Renderer::initialize(unsigned int width, unsigned int height) {
    windowWidth_ = width;
    windowHeight_ = height;

    if (!initWindow(width, height)) {
        return false;
    }

    if (!initVulkan()) {
        return false;
    }

    ready_ = true;
    return true;
}

void Renderer::mainLoop() {
    if (!ready_) {
        return;
    }

    while (windowRunning_) {
        if (!processWindowMessages()) {
            break;
        }
        drawFrame();
    }

    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
}

void Renderer::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    shutdownImGui();
    cleanupSwapchain();

    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }

    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }

    if (lightingDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, lightingDescriptorPool_, nullptr);
        lightingDescriptorPool_ = VK_NULL_HANDLE;
    }

    if (lightingDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, lightingDescriptorSetLayout_, nullptr);
        lightingDescriptorSetLayout_ = VK_NULL_HANDLE;
    }

    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (uniformBuffers_[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, uniformBuffers_[i], nullptr);
        }
        if (uniformBuffersMemory_[i] != VK_NULL_HANDLE) {
            vkFreeMemory(device_, uniformBuffersMemory_[i], nullptr);
        }
    }

    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, vertexBuffer_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
    }

    if (vertexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, vertexBufferMemory_, nullptr);
        vertexBufferMemory_ = VK_NULL_HANDLE;
    }

    if (indexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, indexBuffer_, nullptr);
        indexBuffer_ = VK_NULL_HANDLE;
    }

    if (indexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, indexBufferMemory_, nullptr);
        indexBufferMemory_ = VK_NULL_HANDLE;
    }

    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (imageAvailableSemaphores_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
        }
        if (inFlightFences_[i] != VK_NULL_HANDLE) {
            vkDestroyFence(device_, inFlightFences_[i], nullptr);
        }
    }

    for (VkSemaphore semaphore : renderFinishedSemaphores_) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, semaphore, nullptr);
        }
    }
    renderFinishedSemaphores_.clear();

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

    if (windowHandle_ != nullptr) {
        DestroyWindow(windowHandle_);
        windowHandle_ = nullptr;
    }

    ready_ = false;
}

bool Renderer::initWindow(unsigned int width, unsigned int height) {
    instanceHandle_ = GetModuleHandle(nullptr);

    WNDCLASSEX windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = Renderer::WindowProc;
    windowClass.hInstance = instanceHandle_;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = "VulkanRendererWindowClass";

    if (RegisterClassEx(&windowClass) == 0) {
        const DWORD errorCode = GetLastError();
        if (errorCode != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    RECT windowRect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    windowHandle_ = CreateWindowExA(
        0,
        windowClass.lpszClassName,
        "Vulkan Renderer - Stage 1.5 (Win32 + ImGui)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instanceHandle_,
        this
    );

    if (windowHandle_ == nullptr) {
        std::cerr << "[Renderer] CreateWindowExA failed: " << GetLastError() << "\n";
        return false;
    }

    ShowWindow(windowHandle_, SW_SHOWDEFAULT);
    UpdateWindow(windowHandle_);
    return true;
}

bool Renderer::processWindowMessages() {
    MSG message{};
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            windowRunning_ = false;
            return false;
        }
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return windowRunning_;
}

bool Renderer::initVulkan() {
    try {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
        createSwapchain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createLightingDescriptorSetLayout();
        createGraphicsPipeline();
        createLightingPipeline();
        createDepthResources();
        createGBufferResources();
        createFramebuffers();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createLightingDescriptorPool();
        createLightingDescriptorSet();
        updateLightingDescriptorSet();
        createCommandBuffers();
        createSyncObjects();
        createImGuiDescriptorPool();
        initImGui();
    } catch (const std::exception& exception) {
        std::cerr << "[Renderer] Vulkan init failed: " << exception.what() << "\n";
        return false;
    }

    return true;
}

void Renderer::createInstance() {
    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Vulkan Renderer";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    applicationInfo.pEngineName = "VR";
    applicationInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_2;

    std::vector<const char*> enabledInstanceExtensions(kInstanceExtensions.begin(), kInstanceExtensions.end());
    const bool shouldEnableValidation = kEnableValidationLayers && areValidationLayersSupported();
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
        std::cerr << "[Renderer] VK_EXT_debug_utils not available, validation callback disabled\n";
    }
}

void Renderer::setupDebugMessenger() {
    if (!kEnableValidationLayers || !areValidationLayersSupported()) {
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

    if (createDebugUtilsMessenger(instance_, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDebugUtilsMessengerEXT failed");
    }

    std::cerr << "[Renderer] Vulkan validation enabled (debug messenger active)\n";
}

void Renderer::destroyDebugMessenger() {
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

void Renderer::createSurface() {
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = instanceHandle_;
    createInfo.hwnd = windowHandle_;

    if (vkCreateWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateWin32SurfaceKHR failed");
    }
}

void Renderer::pickPhysicalDevice() {
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

void Renderer::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);

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

    if (kEnableValidationLayers && areValidationLayersSupported()) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDevice failed");
    }

    vkGetDeviceQueue(device_, indices.graphicsFamily, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices.presentFamily, 0, &presentQueue_);
}

void Renderer::createSwapchain() {
    SwapchainSupportDetails support = querySwapchainSupport(physicalDevice_);

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

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
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

void Renderer::createImageViews() {
    swapchainImageViews_.resize(swapchainImages_.size());

    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
        swapchainImageViews_[i] = createImageView(swapchainImages_[i], swapchainImageFormat_, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void Renderer::createRenderPass() {
    // ----------------------- 附件（Attachments）：图像资源 ---------------------
    // 颜色（swapchain)
    // 深度
    // GBuffer Postiion
    // Gbuffer Normal
    // GBuffer ALbedo

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainImageFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // 该 attachment 最终输出到屏幕
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    depthFormat_ = findDepthFormat();
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat_;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription gbufferPositionAttachment{};
    gbufferPositionAttachment.format = gbufferPositionFormat_;
    gbufferPositionAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    gbufferPositionAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gbufferPositionAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferPositionAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    gbufferPositionAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gbufferPositionAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // 该 attachment 不会被 present ，是中间数据
    gbufferPositionAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription gbufferNormalAttachment{};
    gbufferNormalAttachment.format = gbufferNormalFormat_;
    gbufferNormalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    gbufferNormalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gbufferNormalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferNormalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    gbufferNormalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gbufferNormalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gbufferNormalAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription gbufferAlbedoAttachment{};
    gbufferAlbedoAttachment.format = gbufferAlbedoFormat_;
    gbufferAlbedoAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    gbufferAlbedoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gbufferAlbedoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferAlbedoAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    gbufferAlbedoAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gbufferAlbedoAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gbufferAlbedoAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // ------------------------- Subpass --------------------------------
    // 使用避免 GBuffer 写回显存
    // 使用 Input Attachment（不是 sampler），更快

    std::array<VkAttachmentReference, 3> geometryColorAttachmentReferences{};
    geometryColorAttachmentReferences[0] = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    geometryColorAttachmentReferences[1] = {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    geometryColorAttachmentReferences[2] = {4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkAttachmentReference depthAttachmentReference{};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Geometry pass（写 GBuffer）
    VkSubpassDescription geometrySubpass{};
    // 输入：顶点数据
    geometrySubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    // 同时写三个 target (MRT)
    geometrySubpass.colorAttachmentCount = static_cast<std::uint32_t>(geometryColorAttachmentReferences.size());
    geometrySubpass.pColorAttachments = geometryColorAttachmentReferences.data();
    // 开启深度测试
    geometrySubpass.pDepthStencilAttachment = &depthAttachmentReference;

    VkAttachmentReference lightingColorAttachmentReference{};
    lightingColorAttachmentReference.attachment = 0;
    lightingColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    std::array<VkAttachmentReference, 3> lightingInputAttachmentReferences{};
    lightingInputAttachmentReferences[0] = {2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    lightingInputAttachmentReferences[1] = {3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    lightingInputAttachmentReferences[2] = {4, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    // Lighting （读 GBuffer -> 写 Framebuffer）
    VkSubpassDescription lightingSubpass{};
    lightingSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    lightingSubpass.colorAttachmentCount = 1;
    lightingSubpass.pColorAttachments = &lightingColorAttachmentReference;
    // 输入：GBuffer，inputAttachement 是 tiled-based 优化，不写回显存，直接在 tile cache 读 GBuffer
    lightingSubpass.inputAttachmentCount = static_cast<std::uint32_t>(lightingInputAttachmentReferences.size());
    lightingSubpass.pInputAttachments = lightingInputAttachmentReferences.data();

    VkAttachmentReference uiColorAttachmentReference{};
    uiColorAttachmentReference.attachment = 0;
    uiColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription uiSubpass{};
    uiSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    uiSubpass.colorAttachmentCount = 1;
    uiSubpass.pColorAttachments = &uiColorAttachmentReference;

    // --------------------------- dependency -------------------------------
    // 上一帧 / 外部 -> Geomettry Buffer
    // 读 Geometry Buffer -> Liginting 写
    // Lighting 写完 -> UI 覆盖显示

    VkSubpassDependency dependencyExternalToGeometry{};
    dependencyExternalToGeometry.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencyExternalToGeometry.dstSubpass = 0;
    dependencyExternalToGeometry.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencyExternalToGeometry.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencyExternalToGeometry.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency dependencyGeometryToLighting{};
    dependencyGeometryToLighting.srcSubpass = 0;
    dependencyGeometryToLighting.dstSubpass = 1;
    dependencyGeometryToLighting.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencyGeometryToLighting.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencyGeometryToLighting.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencyGeometryToLighting.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency dependencyLightingToUi{};
    dependencyLightingToUi.srcSubpass = 1;
    dependencyLightingToUi.dstSubpass = 2;
    dependencyLightingToUi.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencyLightingToUi.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencyLightingToUi.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencyLightingToUi.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // ---------------------- RenderPass ---------------------------

    std::array<VkAttachmentDescription, 5> attachments = {
        colorAttachment,
        depthAttachment,
        gbufferPositionAttachment,
        gbufferNormalAttachment,
        gbufferAlbedoAttachment,
    };
    VkRenderPassCreateInfo renderPassCreateInfo{};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    renderPassCreateInfo.pAttachments = attachments.data();
    std::array<VkSubpassDescription, 3> subpasses = {geometrySubpass, lightingSubpass, uiSubpass};
    std::array<VkSubpassDependency, 3> dependencies = {
        dependencyExternalToGeometry,
        dependencyGeometryToLighting,
        dependencyLightingToUi,
    };
    renderPassCreateInfo.subpassCount = static_cast<std::uint32_t>(subpasses.size());
    renderPassCreateInfo.pSubpasses = subpasses.data();
    renderPassCreateInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
    renderPassCreateInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(device_, &renderPassCreateInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateRenderPass failed");
    }
}

void Renderer::createDescriptorSetLayout() {
    // --- 定义描述符集布局绑定信息 ---

    // 描述描述符集中的一个条目（Binding）
    VkDescriptorSetLayoutBinding uboLayoutBinding{};

    // 绑定点索引：对应着色器中的 layout(binding = 0)
    uboLayoutBinding.binding = 0;

    // 描述符类型：统一缓冲区，用于向着色器传递常量数据（如变换矩阵）
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    // 描述符数量：如果这是数组则为数组大小，单个UBO则为1
    uboLayoutBinding.descriptorCount = 1;

    // 着色器阶段可见性掩码：指定该UBO只能在顶点着色器阶段访问
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // pImmutableSamplers：用于纹理采样器，此处为UBO，设为nullptr即可
    // uboLayoutBinding.pImmutableSamplers = nullptr; 

    // --- 创建描述符集布局 ---
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    // 绑定数量：该描述符集布局中包含的绑定点的数量
    layoutInfo.bindingCount = 1;

    // 指向绑定信息数组的指针
    layoutInfo.pBindings = &uboLayoutBinding;
    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorSetLayout failed");
    }
}

void Renderer::createLightingDescriptorSetLayout() {
    // lighting 需要从 GBuffer 中读取三个数据，因此这里有三个绑定点
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(bindings.size()); ++i) {
        bindings[i].binding = i;
        // 读取输入附件，例如gbuffer或者上一pass的深度/颜色缓冲
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &lightingDescriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorSetLayout for lighting failed");
    }
}

void Renderer::createGraphicsPipeline() {
    // 定义着色器文件的根目录（通常由构建系统传入宏定义）
    const std::string shaderRoot = VR_SHADER_DIR;
    
    // 读取编译好的SPIR-V二进制文件（顶点着色器和片段着色器）
    std::vector<char> vertShaderCode = readBinaryFile((shaderRoot + "/triangle.vert.spv").c_str());
    std::vector<char> fragShaderCode = readBinaryFile((shaderRoot + "/triangle.frag.spv").c_str());

    // 将读取的二进制代码封装为Vulkan着色器模块对象
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    // --- 配置顶点着色器阶段 ---
    VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{};
    vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // 指定为顶点着色阶段
    vertShaderStageCreateInfo.module = vertShaderModule;          // 绑定着色器模块
    vertShaderStageCreateInfo.pName = "main";                     // 指定入口函数名称

    // --- 配置片段着色器阶段 ---
    VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
    fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT; // 指定为片段着色阶段
    fragShaderStageCreateInfo.module = fragShaderModule;
    fragShaderStageCreateInfo.pName = "main";

    // 将两个着色器阶段信息存入数组，传递给管线创建信息
    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertShaderStageCreateInfo,
        fragShaderStageCreateInfo,
    };

    // --- 配置顶点输入状态 ---
    // 描述顶点数据的绑定方式和属性格式（位置、颜色、法线等）
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkVertexInputBindingDescription bindingDescription = Vertex::getBindingDescription(); // 获取绑定描述（跨度、输入速率等）
    std::array<VkVertexInputAttributeDescription, 3> attributes = Vertex::getAttributeDescriptions(); // 获取属性描述（偏移量、格式等）
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

    // --- 配置输入装配状态 ---
    // 描述如何解释顶点数据以构成图元
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // 设置图元拓扑类型为三角形列表
    // inputAssembly.primitiveRestartEnable = VK_FALSE; // 注意：通常需要设置此项，默认初始化可能为0/False

    // --- 配置视口状态 ---
    // 注意：这里只设置了数量，具体的视口和裁剪矩形将在绘制时通过动态状态设置
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // --- 配置光栅化状态 ---
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;       // 多边形填充模式：实心填充
    rasterizer.lineWidth = 1.0f;                         // 线宽
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;         // 背面剔除
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // 设定逆时针方向为正面

    // --- 配置多重采样状态 ---
    // 这里关闭了多重采样抗锯齿（MSAA）
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // 单采样点

    // --- 配置深度/模板测试状态 ---
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;              // 开启深度测试
    depthStencil.depthWriteEnable = VK_TRUE;             // 开启深度写入
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;    // 深度比较操作：更近的物体通过测试
    depthStencil.depthBoundsTestEnable = VK_FALSE;       // 关闭深度边界测试
    depthStencil.stencilTestEnable = VK_FALSE;           // 关闭模板测试

    // --- 配置颜色混合状态 ---
    // 定义单个颜色附件的混合方式
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // 此处通常还有 blendEnable 等设置，默认为关闭混合（直接覆盖）
    
    // 针对多渲染目标（MRT），这里假设有3个颜色附件，使用相同的混合设置
    std::array<VkPipelineColorBlendAttachmentState, 3> colorBlendAttachments = {
        colorBlendAttachment,
        colorBlendAttachment,
        colorBlendAttachment,
    };

    // 全局颜色混合状态
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = static_cast<std::uint32_t>(colorBlendAttachments.size());
    colorBlending.pAttachments = colorBlendAttachments.data();

    // --- 配置动态状态 ---
    // 允许在绘图命令中动态设置的部分管线状态，无需重建管线
    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, // 动态视口
        VK_DYNAMIC_STATE_SCISSOR,  // 动态裁剪矩形
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // --- 创建管线布局 ---
    // 定义了着色器访问的资源（如Uniform Buffer, Sampler等）的布局
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;                         // 绑定一个描述符集布局
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;

    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreatePipelineLayout failed");
    }

    // --- 创建图形管线 ---
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;                       // 着色器阶段数量
    pipelineInfo.pStages = shaderStages;               // 着色器阶段信息
    pipelineInfo.pVertexInputState = &vertexInputInfo; // 顶点输入
    pipelineInfo.pInputAssemblyState = &inputAssembly; // 输入装配
    pipelineInfo.pViewportState = &viewportState;      // 视口状态
    pipelineInfo.pRasterizationState = &rasterizer;    // 光栅化
    pipelineInfo.pMultisampleState = &multisampling;   // 多重采样
    pipelineInfo.pDepthStencilState = &depthStencil;   // 深度模板
    pipelineInfo.pColorBlendState = &colorBlending;    // 颜色混合
    pipelineInfo.pDynamicState = &dynamicState;        // 动态状态
    pipelineInfo.layout = pipelineLayout_;             // 管线布局
    pipelineInfo.renderPass = renderPass_;             // 兼容的渲染通道
    pipelineInfo.subpass = 0;                          // 使用的子通道索引

    // 调用API创建管线
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateGraphicsPipelines failed");
    }

    // 清理临时的着色器模块
    // 管线创建完成后，着色器模块不再需要，可以销毁以释放内存
    vkDestroyShaderModule(device_, fragShaderModule, nullptr);
    vkDestroyShaderModule(device_, vertShaderModule, nullptr);
}

void Renderer::createLightingPipeline() {
    const std::string shaderRoot = VR_SHADER_DIR;
    std::vector<char> vertShaderCode = readBinaryFile((shaderRoot + "/deferred_lighting.vert.spv").c_str());
    std::vector<char> fragShaderCode = readBinaryFile((shaderRoot + "/deferred_lighting.frag.spv").c_str());

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertShaderModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragShaderModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &lightingDescriptorSetLayout_;

    // --- 配置推送常量范围 ---
    // 推送常量是一种快速向着色器传递少量数据的方式（通常用于更新频率高的数据）
    VkPushConstantRange debugModePushConstantRange{};
    // 指定该推送常量在片元着色器中可见
    debugModePushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    debugModePushConstantRange.offset = 0;
    debugModePushConstantRange.size = sizeof(LightingPushConstants);

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &debugModePushConstantRange;

    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &lightingPipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreatePipelineLayout for lighting failed");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = lightingPipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 1;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &lightingPipeline_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateGraphicsPipelines for lighting failed");
    }

    vkDestroyShaderModule(device_, fragShaderModule, nullptr);
    vkDestroyShaderModule(device_, vertShaderModule, nullptr);
}

void Renderer::createDepthResources() {
    depthFormat_ = findDepthFormat();

    createImage(
        swapchainExtent_.width,
        swapchainExtent_.height,
        depthFormat_,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        depthImage_,
        depthImageMemory_
    );

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (hasStencilComponent(depthFormat_)) {
        aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    depthImageView_ = createImageView(depthImage_, depthFormat_, aspectFlags);
}

void Renderer::createGBufferResources() {
    createImage(
        swapchainExtent_.width,
        swapchainExtent_.height,
        gbufferPositionFormat_,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        gbufferPositionImage_,
        gbufferPositionImageMemory_
    );
    gbufferPositionImageView_ = createImageView(gbufferPositionImage_, gbufferPositionFormat_, VK_IMAGE_ASPECT_COLOR_BIT);

    createImage(
        swapchainExtent_.width,
        swapchainExtent_.height,
        gbufferNormalFormat_,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        gbufferNormalImage_,
        gbufferNormalImageMemory_
    );
    gbufferNormalImageView_ = createImageView(gbufferNormalImage_, gbufferNormalFormat_, VK_IMAGE_ASPECT_COLOR_BIT);

    createImage(
        swapchainExtent_.width,
        swapchainExtent_.height,
        gbufferAlbedoFormat_,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        gbufferAlbedoImage_,
        gbufferAlbedoImageMemory_
    );
    gbufferAlbedoImageView_ = createImageView(gbufferAlbedoImage_, gbufferAlbedoFormat_, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Renderer::createFramebuffers() {
    // FrameBuffer, 渲染管线进行渲染的具体目标，
    // 绑定一组 VkImageView，它们作为 Color，Depth/Stencil等附件，供RenderPass使用

    // FrameBuffer 中的图像会写到 swapchain中，再呈现到屏幕上
    // 为了实现多缓冲，因此有多个 swapchain，同时也有对应的 FrameBuffer
    // 每次渲染时会获取到一个 swapchain 索引，从而能够访问到对应的 FrameBuffer
    swapchainFramebuffers_.resize(swapchainImageViews_.size());

    for (std::size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        // renderpass 中的 attachment 是一种抽象的描述，与 FrameBuffer 中具体的 VkImageView 一一对应起来
        std::array<VkImageView, 5> attachments = {
            swapchainImageViews_[i],
            depthImageView_,
            gbufferPositionImageView_,
            gbufferNormalImageView_,
            gbufferAlbedoImageView_,
        };

        VkFramebufferCreateInfo framebufferCreateInfo{};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = renderPass_; // 指定使用改 FrameBuffer 的的 renderpass
        framebufferCreateInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        framebufferCreateInfo.pAttachments = attachments.data();
        framebufferCreateInfo.width = swapchainExtent_.width;
        framebufferCreateInfo.height = swapchainExtent_.height;
        framebufferCreateInfo.layers = 1;

        if (vkCreateFramebuffer(device_, &framebufferCreateInfo, nullptr, &swapchainFramebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateFramebuffer failed");
        }
    }
}

void Renderer::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice_);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateCommandPool failed");
    }
}

void Renderer::loadMeshVertices() {
    meshVertices_.clear();
    meshIndices_.clear();

    std::string filePath = meshInputPath_.empty() ? std::string(VR_ASSET_DIR) + "/models/basic_mesh.obj" : meshInputPath_;

    MeshInputData meshData;
    if (loadObjMesh(filePath, meshData)) {
        meshVertices_.reserve(meshData.vertices.size());
        for (const auto& vertexInput : meshData.vertices) {
            meshVertices_.push_back(Vertex{
                vertexInput.position,
                vertexInput.normal,
                vertexInput.uv,
            });
        }
        meshIndices_ = meshData.indices;
    }

    if (meshVertices_.empty() || meshIndices_.empty()) {
        meshVertices_.assign(kFallbackVertices.begin(), kFallbackVertices.end());
        meshIndices_.assign(kFallbackIndices.begin(), kFallbackIndices.end());
    }

    refreshSceneScaleParams();
}

bool Renderer::openModelFileDialog(std::string& outPath) const {
    std::array<char, MAX_PATH> filePathBuffer{};

    OPENFILENAMEA dialogInfo{};
    dialogInfo.lStructSize = sizeof(dialogInfo);
    dialogInfo.hwndOwner = windowHandle_;
    dialogInfo.lpstrFilter = "OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
    dialogInfo.lpstrFile = filePathBuffer.data();
    dialogInfo.nMaxFile = static_cast<DWORD>(filePathBuffer.size());
    dialogInfo.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    dialogInfo.lpstrDefExt = "obj";

    if (GetOpenFileNameA(&dialogInfo) == FALSE) {
        return false;
    }

    outPath = filePathBuffer.data();
    return true;
}

bool Renderer::reloadModelFromPath(const std::string& path) {
    if (device_ == VK_NULL_HANDLE || modelImportInProgress_) {
        return false;
    }

    startAsyncModelImport(path);
    return true;
}

void Renderer::startAsyncModelImport(const std::string& path) {
    pendingMeshPath_ = path;
    modelImportInProgress_ = true;
    appendOutput(std::string("Importing model in background: ") + path);

    pendingMeshLoadTask_ = std::async(std::launch::async, [path]() {
        MeshInputData meshData;
        loadObjMesh(path, meshData);
        return meshData;
    });
}

void Renderer::pollAsyncModelImport() {
    if (!modelImportInProgress_) {
        return;
    }

    if (!pendingMeshLoadTask_.valid()) {
        modelImportInProgress_ = false;
        appendOutput("Model import failed: invalid async task");
        return;
    }

    if (pendingMeshLoadTask_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    MeshInputData meshData = pendingMeshLoadTask_.get();
    modelImportInProgress_ = false;

    if (meshData.vertices.empty() || meshData.indices.empty()) {
        appendOutput(std::string("Model import failed: ") + pendingMeshPath_);
        return;
    }

    std::vector<Vertex> newVertices;
    newVertices.reserve(meshData.vertices.size());
    for (const auto& vertexInput : meshData.vertices) {
        newVertices.push_back(Vertex{vertexInput.position, vertexInput.normal, vertexInput.uv});
    }

    try {
        vkDeviceWaitIdle(device_);

        if (vertexBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, vertexBuffer_, nullptr);
            vertexBuffer_ = VK_NULL_HANDLE;
        }
        if (vertexBufferMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_, vertexBufferMemory_, nullptr);
            vertexBufferMemory_ = VK_NULL_HANDLE;
        }

        if (indexBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, indexBuffer_, nullptr);
            indexBuffer_ = VK_NULL_HANDLE;
        }
        if (indexBufferMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_, indexBufferMemory_, nullptr);
            indexBufferMemory_ = VK_NULL_HANDLE;
        }

        meshVertices_ = std::move(newVertices);
        meshIndices_ = std::move(meshData.indices);
        meshInputPath_ = pendingMeshPath_;
        refreshSceneScaleParams();

        createVertexBuffer();
        createIndexBuffer();
        appendOutput(std::string("Imported model: ") + meshInputPath_);
    } catch (const std::exception& exception) {
        appendOutput(std::string("Model import failed: ") + exception.what());
    }
}

void Renderer::refreshSceneScaleParams() {
    if (meshVertices_.empty()) {
        sceneRadius_ = 1.0f;
        return;
    }

    glm::vec3 minPosition = meshVertices_.front().position;
    glm::vec3 maxPosition = meshVertices_.front().position;
    for (const Vertex& vertex : meshVertices_) {
        minPosition = glm::min(minPosition, vertex.position);
        maxPosition = glm::max(maxPosition, vertex.position);
    }

    const glm::vec3 center = (minPosition + maxPosition) * 0.5f;
    sceneRadius_ = 0.0f;
    for (const Vertex& vertex : meshVertices_) {
        sceneRadius_ = std::max(sceneRadius_, glm::distance(vertex.position, center));
    }

    if (sceneRadius_ < 1.0f) {
        sceneRadius_ = 1.0f;
    }
}

void Renderer::createVertexBuffer() {
    if (meshVertices_.empty() || meshIndices_.empty()) {
        loadMeshVertices();
    }

    VkDeviceSize bufferSize = sizeof(Vertex) * meshVertices_.size();
    createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertexBuffer_,
        vertexBufferMemory_
    );

    void* data = nullptr;
    vkMapMemory(device_, vertexBufferMemory_, 0, bufferSize, 0, &data);
    std::memcpy(data, meshVertices_.data(), static_cast<std::size_t>(bufferSize));
    vkUnmapMemory(device_, vertexBufferMemory_);
}

void Renderer::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(std::uint32_t) * meshIndices_.size();
    createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBuffer_,
        indexBufferMemory_
    );

    void* data = nullptr;
    vkMapMemory(device_, indexBufferMemory_, 0, bufferSize, 0, &data);
    std::memcpy(data, meshIndices_.data(), static_cast<std::size_t>(bufferSize));
    vkUnmapMemory(device_, indexBufferMemory_);
}

void Renderer::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers_[i],
            uniformBuffersMemory_[i]
        );
    }
}

void Renderer::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = kMaxFramesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = kMaxFramesInFlight;

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorPool failed");
    }
}

void Renderer::createDescriptorSets() {
    std::array<VkDescriptorSetLayout, kMaxFramesInFlight> layouts{};
    layouts.fill(descriptorSetLayout_);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = kMaxFramesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_.data()) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateDescriptorSets failed");
    }

    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers_[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets_[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
    }
}

void Renderer::createLightingDescriptorPool() {
    std::array<VkDescriptorPoolSize, 1> poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 3},
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &lightingDescriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorPool for lighting failed");
    }
}

void Renderer::createLightingDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = lightingDescriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &lightingDescriptorSetLayout_;

    if (vkAllocateDescriptorSets(device_, &allocInfo, &lightingDescriptorSet_) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateDescriptorSets for lighting failed");
    }
}

void Renderer::updateLightingDescriptorSet() {
    std::array<VkDescriptorImageInfo, 3> imageInfos{};
    imageInfos[0].imageView = gbufferPositionImageView_;
    imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[1].imageView = gbufferNormalImageView_;
    imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[2].imageView = gbufferAlbedoImageView_;
    imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 3> writes{};
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(writes.size()); ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = lightingDescriptorSet_;
        writes[i].dstBinding = i;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writes[i].descriptorCount = 1;
        writes[i].pImageInfo = &imageInfos[i];
    }

    vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void Renderer::createImGuiDescriptorPool() {
    std::array<VkDescriptorPoolSize, 1> poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1024;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &imguiDescriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorPool for imgui failed");
    }
}

void Renderer::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

    ImGui_ImplWin32_Init(windowHandle_);
    ImGui::GetPlatformIO().Platform_CreateVkSurface = imguiWin32CreateVkSurface;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance_;
    initInfo.PhysicalDevice = physicalDevice_;
    initInfo.Device = device_;
    initInfo.QueueFamily = indices.graphicsFamily;
    initInfo.Queue = graphicsQueue_;
    initInfo.DescriptorPool = imguiDescriptorPool_;
    initInfo.ApiVersion = VK_API_VERSION_1_2;
    initInfo.MinImageCount = swapchainMinImageCount_;
    initInfo.ImageCount = static_cast<std::uint32_t>(swapchainImages_.size());
    initInfo.PipelineInfoMain.RenderPass = renderPass_;
    initInfo.PipelineInfoMain.Subpass = 2;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
    appendOutput("ImGui initialized");
    appendOutput("Docking enabled");
}

void Renderer::shutdownImGui() {
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    if (imguiDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, imguiDescriptorPool_, nullptr);
        imguiDescriptorPool_ = VK_NULL_HANDLE;
    }
}

void Renderer::buildGui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::Begin("Stage 1.5 Controls");
    ImGui::Text("Right Drag: Rotate");
    ImGui::Text("Left Drag: Move");
    ImGui::Text("Mouse Wheel: Zoom");
    ImGui::SliderFloat3("Model Translate", &modelTranslation_.x, -5.0f, 5.0f);
    ImGui::SliderFloat("Yaw", &modelYawRadians_, -3.14f, 3.14f);
    ImGui::SliderFloat("Pitch", &modelPitchRadians_, -3.14f, 3.14f);
    const char* debugViewItems[] = {"Final", "Position", "Normal", "Albedo"};
    lightingDebugMode_ = std::clamp(lightingDebugMode_, 0, static_cast<int>(IM_ARRAYSIZE(debugViewItems) - 1));
    if (ImGui::Combo("Deferred Debug", &lightingDebugMode_, debugViewItems, IM_ARRAYSIZE(debugViewItems))) {
        appendOutput(std::string("Deferred debug view: ") + debugViewItems[lightingDebugMode_]);
    }
    ImGui::SliderInt("Deferred Light Count", &deferredLightCount_, 1, 32);
    ImGui::SliderFloat("Deferred Light Intensity", &deferredLightIntensity_, 0.1f, 8.0f);
    ImGui::SeparatorText("PBR Material");
    ImGui::SliderFloat("Metallic", &materialMetallic_, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness", &materialRoughness_, 0.04f, 1.0f);
    ImGui::SliderFloat("AO", &materialAo_, 0.0f, 1.0f);
    const float cameraDistanceMax = std::max(20.0f, sceneRadius_ * 20.0f);
    ImGui::SliderFloat("Camera Distance", &cameraDistance_, 1.0f, cameraDistanceMax);
    if (ImGui::Button("Import Model (.obj)")) {
        std::string selectedPath;
        if (openModelFileDialog(selectedPath)) {
            reloadModelFromPath(selectedPath);
        } else {
            appendOutput("Model import canceled");
        }
    }
    if (modelImportInProgress_) {
        ImGui::SameLine();
        ImGui::TextUnformatted("Loading...");
    }
    if (ImGui::Button("Reset")) {
        modelTranslation_ = glm::vec3(0.0f);
        modelYawRadians_ = 0.0f;
        modelPitchRadians_ = 0.0f;
        cameraDistance_ = std::clamp(sceneRadius_ * 2.8f, 1.0f, cameraDistanceMax);
        appendOutput("Camera/model transform reset");
    }
    ImGui::Text("Frame time %.3f ms (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();

    ImGui::Begin("Output");
    if (ImGui::Button("Clear")) {
        outputLines_.clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto Scroll", &autoScrollOutput_);
    ImGui::Separator();

    ImGui::BeginChild("OutputScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const std::string& line : outputLines_) {
        ImGui::TextUnformatted(line.c_str());
    }
    if (autoScrollOutput_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();

    ImGui::Render();
}

void Renderer::appendOutput(std::string message) {
    if (outputLines_.size() >= 512) {
        outputLines_.erase(outputLines_.begin(), outputLines_.begin() + 128);
    }
    outputLines_.push_back(std::move(message));
}

void Renderer::createCommandBuffers() {
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = kMaxFramesInFlight;

    if (vkAllocateCommandBuffers(device_, &allocateInfo, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateCommandBuffers failed");
    }
}

void Renderer::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects");
        }
    }

    renderFinishedSemaphores_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    for (std::size_t i = 0; i < renderFinishedSemaphores_.size(); ++i) {
        if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render-finished semaphore");
        }
    }

    imagesInFlight_.assign(swapchainImages_.size(), VK_NULL_HANDLE);
}

void Renderer::updateUniformBuffer(std::uint32_t frameIndex, float timeSeconds) {
    (void)timeSeconds;
    UniformBufferObject ubo{};

    glm::mat4 model = glm::translate(glm::mat4(1.0f), modelTranslation_);
    model = glm::rotate(model, modelYawRadians_, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, modelPitchRadians_, glm::vec3(1.0f, 0.0f, 0.0f));

    ubo.model = model;
    ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, cameraDistance_), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.projection = glm::perspective(
        glm::radians(45.0f),
        static_cast<float>(swapchainExtent_.width) / static_cast<float>(swapchainExtent_.height),
        0.1f,
        std::max(100.0f, sceneRadius_ * 50.0f)
    );
    ubo.projection[1][1] *= -1.0f;

    void* data = nullptr;
    vkMapMemory(device_, uniformBuffersMemory_[frameIndex], 0, sizeof(ubo), 0, &data);
    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(device_, uniformBuffersMemory_[frameIndex]);
}

void Renderer::drawFrame() {
    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    std::uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        device_,
        swapchain_,
        UINT64_MAX,
        imageAvailableSemaphores_[currentFrame_],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }

    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("vkAcquireNextImageKHR failed");
    }

    if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device_, 1, &imagesInFlight_[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight_[imageIndex] = inFlightFences_[currentFrame_];

    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);
    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);

    static const auto startTime = std::chrono::high_resolution_clock::now();
    const auto now = std::chrono::high_resolution_clock::now();
    float timeSeconds = std::chrono::duration<float, std::chrono::seconds::period>(now - startTime).count();

    pollAsyncModelImport();
    buildGui();
    updateUniformBuffer(currentFrame_, timeSeconds);
    recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[imageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]) != VK_SUCCESS) {
        throw std::runtime_error("vkQueueSubmit failed");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {swapchain_};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || framebufferResized_) {
        framebufferResized_ = false;
        recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("vkQueuePresentKHR failed");
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("vkBeginCommandBuffer failed");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = swapchainFramebuffers_[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent_;

    std::array<VkClearValue, 5> clearValues{};
    clearValues[0].color = {{0.05f, 0.07f, 0.11f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    clearValues[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[3].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[4].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchainExtent_.width);
    viewport.height = static_cast<float>(swapchainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = swapchainExtent_;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_,
        0,
        1,
        &descriptorSets_[currentFrame_],
        0,
        nullptr
    );

    vkCmdDrawIndexed(commandBuffer, static_cast<std::uint32_t>(meshIndices_.size()), 1, 0, 0, 0);
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipeline_);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        lightingPipelineLayout_,
        0,
        1,
        &lightingDescriptorSet_,
        0,
        nullptr
    );
    LightingPushConstants lightingPushConstants{};
    lightingPushConstants.debugMode = lightingDebugMode_;
    lightingPushConstants.lightCount = std::clamp(deferredLightCount_, 1, 32);
    lightingPushConstants.lightIntensity = std::max(0.1f, deferredLightIntensity_);
    lightingPushConstants.positionDebugScale = std::max(1.0f, sceneRadius_ * 2.0f);
    lightingPushConstants.metallic = std::clamp(materialMetallic_, 0.0f, 1.0f);
    lightingPushConstants.roughness = std::clamp(materialRoughness_, 0.04f, 1.0f);
    lightingPushConstants.ao = std::clamp(materialAo_, 0.0f, 1.0f);
    lightingPushConstants.cameraDistance = cameraDistance_;
    vkCmdPushConstants(
        commandBuffer,
        lightingPipelineLayout_,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(LightingPushConstants),
        &lightingPushConstants
    );
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("vkEndCommandBuffer failed");
    }
}

void Renderer::recreateSwapchain() {
    while (windowWidth_ == 0 || windowHeight_ == 0) {
        if (!processWindowMessages()) {
            return;
        }
    }

    vkDeviceWaitIdle(device_);

    cleanupSwapchain();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createLightingPipeline();
    createDepthResources();
    createGBufferResources();
    createFramebuffers();
    updateLightingDescriptorSet();
    ImGui_ImplVulkan_SetMinImageCount(swapchainMinImageCount_);
    appendOutput("Swapchain recreated");

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (VkSemaphore semaphore : renderFinishedSemaphores_) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, semaphore, nullptr);
        }
    }
    renderFinishedSemaphores_.assign(swapchainImages_.size(), VK_NULL_HANDLE);
    for (std::size_t i = 0; i < renderFinishedSemaphores_.size(); ++i) {
        if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to recreate render-finished semaphore");
        }
    }

    imagesInFlight_.assign(swapchainImages_.size(), VK_NULL_HANDLE);
}

void Renderer::cleanupSwapchain() {
    for (VkFramebuffer framebuffer : swapchainFramebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    swapchainFramebuffers_.clear();

    if (depthImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depthImageView_, nullptr);
        depthImageView_ = VK_NULL_HANDLE;
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, depthImage_, nullptr);
        depthImage_ = VK_NULL_HANDLE;
    }
    if (depthImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, depthImageMemory_, nullptr);
        depthImageMemory_ = VK_NULL_HANDLE;
    }

    if (gbufferPositionImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, gbufferPositionImageView_, nullptr);
        gbufferPositionImageView_ = VK_NULL_HANDLE;
    }
    if (gbufferPositionImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, gbufferPositionImage_, nullptr);
        gbufferPositionImage_ = VK_NULL_HANDLE;
    }
    if (gbufferPositionImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, gbufferPositionImageMemory_, nullptr);
        gbufferPositionImageMemory_ = VK_NULL_HANDLE;
    }

    if (gbufferNormalImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, gbufferNormalImageView_, nullptr);
        gbufferNormalImageView_ = VK_NULL_HANDLE;
    }
    if (gbufferNormalImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, gbufferNormalImage_, nullptr);
        gbufferNormalImage_ = VK_NULL_HANDLE;
    }
    if (gbufferNormalImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, gbufferNormalImageMemory_, nullptr);
        gbufferNormalImageMemory_ = VK_NULL_HANDLE;
    }

    if (gbufferAlbedoImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, gbufferAlbedoImageView_, nullptr);
        gbufferAlbedoImageView_ = VK_NULL_HANDLE;
    }
    if (gbufferAlbedoImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, gbufferAlbedoImage_, nullptr);
        gbufferAlbedoImage_ = VK_NULL_HANDLE;
    }
    if (gbufferAlbedoImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, gbufferAlbedoImageMemory_, nullptr);
        gbufferAlbedoImageMemory_ = VK_NULL_HANDLE;
    }

    if (graphicsPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
        graphicsPipeline_ = VK_NULL_HANDLE;
    }

    if (lightingPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, lightingPipeline_, nullptr);
        lightingPipeline_ = VK_NULL_HANDLE;
    }

    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }

    if (lightingPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, lightingPipelineLayout_, nullptr);
        lightingPipelineLayout_ = VK_NULL_HANDLE;
    }

    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

    for (VkImageView imageView : swapchainImageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    swapchainImageViews_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

Renderer::QueueFamilyIndices Renderer::findQueueFamilies(VkPhysicalDevice device) const {
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

Renderer::SwapchainSupportDetails Renderer::querySwapchainSupport(VkPhysicalDevice device) const {
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
    if (formatCount > 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
    }

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
    if (presentModeCount > 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return formats.front();
}

VkPresentModeKHR Renderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes) const {
    for (const auto& presentMode : presentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent = {
        windowWidth_,
        windowHeight_,
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

bool Renderer::isDeviceSuitable(VkPhysicalDevice device) const {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapchainAdequate = false;
    if (extensionsSupported) {
        SwapchainSupportDetails supportDetails = querySwapchainSupport(device);
        swapchainAdequate = !supportDetails.formats.empty() && !supportDetails.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapchainAdequate;
}

bool Renderer::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    std::uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

VkFormat Renderer::findSupportedFormat(
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features
) const {
    for (VkFormat format : candidates) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &properties);

        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features) {
            return format;
        }

        if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("no supported format found");
}

VkFormat Renderer::findDepthFormat() const {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

std::vector<char> Renderer::readBinaryFile(const char* filePath) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("failed to open shader file: ") + filePath);
    }

    std::streamsize fileSize = file.tellg();
    std::vector<char> buffer(static_cast<std::size_t>(fileSize));
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    return buffer;
}

std::uint32_t Renderer::findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
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

void Renderer::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
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

void Renderer::createImage(
    std::uint32_t width,
    std::uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImage& image,
    VkDeviceMemory& imageMemory
) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1; // 当前不支持 mipmap
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

VkImageView Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const {
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

VkShaderModule Renderer::createShaderModule(const std::vector<char>& code) const {
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

LRESULT CALLBACK Renderer::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    Renderer* renderer = nullptr;

    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        renderer = static_cast<Renderer*>(createStruct->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(renderer));
    } else {
        renderer = reinterpret_cast<Renderer*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (renderer != nullptr) {
        return renderer->handleWindowMessage(hWnd, message, wParam, lParam);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT Renderer::handleWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    bool uiCapturingMouse = false;
    if (ImGui::GetCurrentContext() != nullptr) {
        if (::ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam) != 0) {
            return 1;
        }
        uiCapturingMouse = ImGui::GetIO().WantCaptureMouse;
    }

    switch (message) {
    case WM_CLOSE:
        windowRunning_ = false;
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        windowWidth_ = static_cast<unsigned int>(LOWORD(lParam));
        windowHeight_ = static_cast<unsigned int>(HIWORD(lParam));
        if (wParam != SIZE_MINIMIZED) {
            framebufferResized_ = true;
        }
        return 0;
    case WM_RBUTTONDOWN:
        if (uiCapturingMouse) {
            rightDragActive_ = false;
            return 0;
        }
        rightDragActive_ = true;
        lastMousePosition_ = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        return 0;
    case WM_RBUTTONUP:
        rightDragActive_ = false;
        return 0;
    case WM_LBUTTONDOWN:
        if (uiCapturingMouse) {
            leftDragActive_ = false;
            return 0;
        }
        leftDragActive_ = true;
        lastMousePosition_ = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        return 0;
    case WM_LBUTTONUP:
        leftDragActive_ = false;
        return 0;
    case WM_MOUSEMOVE: {
        const POINT currentPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const float deltaX = static_cast<float>(currentPoint.x - lastMousePosition_.x);
        const float deltaY = static_cast<float>(currentPoint.y - lastMousePosition_.y);
        lastMousePosition_ = currentPoint;

        if (uiCapturingMouse) {
            rightDragActive_ = false;
            leftDragActive_ = false;
            return 0;
        }

        if (rightDragActive_) {
            modelYawRadians_ += deltaX * 0.01f;
            modelPitchRadians_ += deltaY * 0.01f;
        }
        if (leftDragActive_) {
            modelTranslation_.x += deltaX * 0.005f;
            modelTranslation_.y -= deltaY * 0.005f;
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (uiCapturingMouse) {
            return 0;
        }
        const short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        cameraDistance_ -= static_cast<float>(wheelDelta) / static_cast<float>(WHEEL_DELTA) * 0.2f;
        cameraDistance_ = std::clamp(cameraDistance_, 1.0f, std::max(20.0f, sceneRadius_ * 20.0f));
        // appendOutput("Zoom updated");
        return 0;
    }
    default:
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

} // namespace vr
