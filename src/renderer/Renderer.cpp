/**
 * @file Renderer.cpp
 * @brief Implementation for the Renderer module.
 */
#include "renderer/Renderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_win32.h>

#include <commdlg.h>

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/framegraph/passes/SceneFrameGraphPass.h"
#include "renderer/framegraph/passes/UiFrameGraphPass.h"
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

bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
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

    RendererTeardown::TeardownContext<kMaxFramesInFlight> teardownContext{
        .instance = instance_,
        .device = device_,
        .surface = surface_,
        .descriptorPool = descriptorPool_,
        .descriptorSetLayout = descriptorSetLayout_,
        .lightingDescriptorPool = lightingDescriptorPool_,
        .lightingDescriptorSetLayout = lightingDescriptorSetLayout_,
        .uniformBuffers = uniformBuffers_,
        .uniformBuffersMemory = uniformBuffersMemory_,
        .vertexBuffer = vertexBuffer_,
        .vertexBufferMemory = vertexBufferMemory_,
        .indexBuffer = indexBuffer_,
        .indexBufferMemory = indexBufferMemory_,
        .imageAvailableSemaphores = imageAvailableSemaphores_,
        .inFlightFences = inFlightFences_,
        .renderFinishedSemaphores = renderFinishedSemaphores_,
        .commandPool = commandPool_,
    };
    rendererTeardown_.cleanup(teardownContext);

    destroyDebugMessenger();

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    if (windowHandle_ != nullptr) {
        DestroyWindow(windowHandle_);
        windowHandle_ = nullptr;
    }

    lifecycleValidator_.validateShutdownCleared(ResourceLifecycleValidator::ShutdownContext{
        .instance = instance_,
        .device = device_,
        .surface = surface_,
        .windowHandle = windowHandle_,
        .descriptorPool = descriptorPool_,
        .descriptorSetLayout = descriptorSetLayout_,
        .lightingDescriptorPool = lightingDescriptorPool_,
        .lightingDescriptorSetLayout = lightingDescriptorSetLayout_,
        .vertexBuffer = vertexBuffer_,
        .vertexBufferMemory = vertexBufferMemory_,
        .indexBuffer = indexBuffer_,
        .indexBufferMemory = indexBufferMemory_,
        .commandPool = commandPool_,
        .renderFinishedSemaphores = &renderFinishedSemaphores_,
    });

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
        "Vulkan Renderer",
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
        RendererInitPipeline::Context context{
            .createInstance = [this]() {
                createInstance();
            },
            .setupDebugMessenger = [this]() {
                setupDebugMessenger();
            },
            .createSurface = [this]() {
                createSurface();
            },
            .pickPhysicalDevice = [this]() {
                pickPhysicalDevice();
            },
            .createLogicalDevice = [this]() {
                createLogicalDevice();
            },
            .createCommandPool = [this]() {
                createCommandPool();
            },
            .createSwapchain = [this]() {
                createSwapchain();
            },
            .createImageViews = [this]() {
                createImageViews();
            },
            .createRenderPass = [this]() {
                createRenderPass();
            },
            .createDescriptorSetLayout = [this]() {
                createDescriptorSetLayout();
            },
            .createLightingDescriptorSetLayout = [this]() {
                createLightingDescriptorSetLayout();
            },
            .createGraphicsPipeline = [this]() {
                createGraphicsPipeline();
            },
            .createLightingPipeline = [this]() {
                createLightingPipeline();
            },
            .createDepthResources = [this]() {
                createDepthResources();
            },
            .createGBufferResources = [this]() {
                createGBufferResources();
            },
            .createFramebuffers = [this]() {
                createFramebuffers();
            },
            .createVertexBuffer = [this]() {
                createVertexBuffer();
            },
            .createIndexBuffer = [this]() {
                createIndexBuffer();
            },
            .createUniformBuffers = [this]() {
                createUniformBuffers();
            },
            .createDescriptorPool = [this]() {
                createDescriptorPool();
            },
            .createDescriptorSets = [this]() {
                createDescriptorSets();
            },
            .createLightingDescriptorPool = [this]() {
                createLightingDescriptorPool();
            },
            .createLightingDescriptorSet = [this]() {
                createLightingDescriptorSet();
            },
            .updateLightingDescriptorSet = [this]() {
                updateLightingDescriptorSet();
            },
            .createCommandBuffers = [this]() {
                createCommandBuffers();
            },
            .createSyncObjects = [this]() {
                createSyncObjects();
            },
            .createImGuiDescriptorPool = [this]() {
                createImGuiDescriptorPool();
            },
            .initImGui = [this]() {
                initImGui();
            },
        };
        rendererInitPipeline_.execute(context);
    } catch (const std::exception& exception) {
        std::cerr << "[Renderer] Vulkan init failed: " << exception.what() << "\n";
        return false;
    }

    return true;
}

VulkanBootstrap::BootstrapContext Renderer::buildBootstrapContext() {
    std::vector<const char*> instanceExtensions(kInstanceExtensions.begin(), kInstanceExtensions.end());
    std::vector<const char*> deviceExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());
    std::vector<const char*> validationLayers(kValidationLayers.begin(), kValidationLayers.end());

    return VulkanBootstrap::BootstrapContext{
        .instanceHandle = instanceHandle_,
        .windowHandle = windowHandle_,
        .enableValidationLayers = kEnableValidationLayers,
        .instanceExtensions = std::move(instanceExtensions),
        .deviceExtensions = std::move(deviceExtensions),
        .validationLayers = std::move(validationLayers),
        .isDeviceSuitable = [this](VkPhysicalDevice device) {
            return isDeviceSuitable(device);
        },
        .findQueueFamilies = [this](VkPhysicalDevice device) {
            QueueFamilyIndices indices = findQueueFamilies(device);
            return VulkanBootstrap::QueueFamilyIndices{
                .graphicsFamily = indices.graphicsFamily,
                .presentFamily = indices.presentFamily,
            };
        },
        .instance = instance_,
        .debugMessenger = debugMessenger_,
        .surface = surface_,
        .physicalDevice = physicalDevice_,
        .device = device_,
        .graphicsQueue = graphicsQueue_,
        .presentQueue = presentQueue_,
    };
}

void Renderer::createInstance() {
    VulkanBootstrap::BootstrapContext context = buildBootstrapContext();
    vulkanBootstrap_.createInstance(context);
}

void Renderer::setupDebugMessenger() {
    VulkanBootstrap::BootstrapContext context = buildBootstrapContext();
    vulkanBootstrap_.setupDebugMessenger(context);
}

void Renderer::destroyDebugMessenger() {
    vulkanBootstrap_.destroyDebugMessenger(instance_, debugMessenger_);
}

void Renderer::createSurface() {
    VulkanBootstrap::BootstrapContext context = buildBootstrapContext();
    vulkanBootstrap_.createSurface(context);
}

void Renderer::pickPhysicalDevice() {
    VulkanBootstrap::BootstrapContext context = buildBootstrapContext();
    vulkanBootstrap_.pickPhysicalDevice(context);
}

void Renderer::createLogicalDevice() {
    VulkanBootstrap::BootstrapContext context = buildBootstrapContext();
    vulkanBootstrap_.createLogicalDevice(context);
}

void Renderer::createSwapchain() {
    SwapchainSupportDetails support = querySwapchainSupport(physicalDevice_);
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    Swapchain::CreateContext context{
        .device = device_,
        .surface = surface_,
        .support = Swapchain::SupportDetails{
            .capabilities = support.capabilities,
            .formats = support.formats,
            .presentModes = support.presentModes,
        },
        .queueFamilies = Swapchain::QueueFamilyIndices{
            .graphicsFamily = indices.graphicsFamily,
            .presentFamily = indices.presentFamily,
        },
        .chooseSurfaceFormat = [this](const std::vector<VkSurfaceFormatKHR>& formats) {
            return chooseSwapSurfaceFormat(formats);
        },
        .choosePresentMode = [this](const std::vector<VkPresentModeKHR>& presentModes) {
            return chooseSwapPresentMode(presentModes);
        },
        .chooseExtent = [this](const VkSurfaceCapabilitiesKHR& capabilities) {
            return chooseSwapExtent(capabilities);
        },
        .swapchain = swapchain_,
        .swapchainImages = swapchainImages_,
        .swapchainImageFormat = swapchainImageFormat_,
        .swapchainExtent = swapchainExtent_,
        .swapchainMinImageCount = swapchainMinImageCount_,
    };
    swapchainManager_.create(context);
}

void Renderer::createImageViews() {
    Swapchain::ImageViewsContext context{
        .swapchainImages = swapchainImages_,
        .swapchainImageFormat = swapchainImageFormat_,
        .swapchainImageViews = swapchainImageViews_,
        .createImageView = [this](VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
            return createImageView(image, format, aspectFlags);
        },
    };
    swapchainManager_.createImageViews(context);
}

void Renderer::createRenderPass() {
    PipelineBuilder::RenderPassContext context{
        .device = device_,
        .swapchainImageFormat = swapchainImageFormat_,
        .gbufferPositionFormat = gbufferPositionFormat_,
        .gbufferNormalFormat = gbufferNormalFormat_,
        .gbufferAlbedoFormat = gbufferAlbedoFormat_,
        .depthFormat = depthFormat_,
        .renderPass = renderPass_,
        .uiRenderPass = uiRenderPass_,
        .findDepthFormat = [this]() {
            return findDepthFormat();
        },
    };
    pipelineBuilder_.createRenderPass(context);
}

void Renderer::createDescriptorSetLayout() {
    DescriptorBuilder::SetLayoutsContext context{
        .device = device_,
        .descriptorSetLayout = descriptorSetLayout_,
        .lightingDescriptorSetLayout = lightingDescriptorSetLayout_,
    };
    descriptorBuilder_.createSetLayouts(context);
}

void Renderer::createLightingDescriptorSetLayout() {
    // Created together with createDescriptorSetLayout(); kept as a no-op for init sequence compatibility.
}

void Renderer::createGraphicsPipeline() {
    PipelineBuilder::GeometryPipelineContext context{
        .device = device_,
        .shaderRoot = VR_SHADER_DIR,
        .descriptorSetLayout = descriptorSetLayout_,
        .renderPass = renderPass_,
        .pipelineLayout = pipelineLayout_,
        .graphicsPipeline = graphicsPipeline_,
        .readBinaryFile = [this](const char* filePath) {
            return readBinaryFile(filePath);
        },
        .createShaderModule = [this](const std::vector<char>& code) {
            return createShaderModule(code);
        },
        .getBindingDescription = []() {
            return Vertex::getBindingDescription();
        },
        .getAttributeDescriptions = []() {
            return Vertex::getAttributeDescriptions();
        },
    };
    pipelineBuilder_.createGraphicsPipeline(context);
}

void Renderer::createLightingPipeline() {
    PipelineBuilder::LightingPipelineContext context{
        .device = device_,
        .shaderRoot = VR_SHADER_DIR,
        .lightingDescriptorSetLayout = lightingDescriptorSetLayout_,
        .renderPass = renderPass_,
        .lightingPipelineLayout = lightingPipelineLayout_,
        .lightingPipeline = lightingPipeline_,
        .pushConstantSize = sizeof(LightingPushConstants),
        .readBinaryFile = [this](const char* filePath) {
            return readBinaryFile(filePath);
        },
        .createShaderModule = [this](const std::vector<char>& code) {
            return createShaderModule(code);
        },
    };
    pipelineBuilder_.createLightingPipeline(context);
}

void Renderer::createDepthResources() {
    RenderTargetsBuilder::DepthContext context{
        .extent = swapchainExtent_,
        .depthFormat = depthFormat_,
        .depthImage = depthImage_,
        .depthImageMemory = depthImageMemory_,
        .depthImageView = depthImageView_,
        .findDepthFormat = [this]() {
            return findDepthFormat();
        },
        .hasStencilComponent = [](VkFormat format) {
            return hasStencilComponent(format);
        },
        .createImage = [this](
                           std::uint32_t width,
                           std::uint32_t height,
                           VkFormat format,
                           VkImageTiling tiling,
                           VkImageUsageFlags usage,
                           VkMemoryPropertyFlags properties,
                           VkImage& image,
                           VkDeviceMemory& imageMemory
                       ) {
            createImage(width, height, format, tiling, usage, properties, image, imageMemory);
        },
        .createImageView = [this](VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
            return createImageView(image, format, aspectFlags);
        },
    };
    renderTargetsBuilder_.createDepthResources(context);
}

void Renderer::createGBufferResources() {
    RenderTargetsBuilder::GBufferContext context{
        .device = device_,
        .extent = swapchainExtent_,
        .swapchainImageFormat = swapchainImageFormat_,
        .gbufferPositionFormat = gbufferPositionFormat_,
        .gbufferNormalFormat = gbufferNormalFormat_,
        .gbufferAlbedoFormat = gbufferAlbedoFormat_,
        .sceneColorImage = sceneColorImage_,
        .sceneColorImageMemory = sceneColorImageMemory_,
        .sceneColorImageView = sceneColorImageView_,
        .sceneColorSampler = sceneColorSampler_,
        .gbufferPositionImage = gbufferPositionImage_,
        .gbufferPositionImageMemory = gbufferPositionImageMemory_,
        .gbufferPositionImageView = gbufferPositionImageView_,
        .gbufferNormalImage = gbufferNormalImage_,
        .gbufferNormalImageMemory = gbufferNormalImageMemory_,
        .gbufferNormalImageView = gbufferNormalImageView_,
        .gbufferAlbedoImage = gbufferAlbedoImage_,
        .gbufferAlbedoImageMemory = gbufferAlbedoImageMemory_,
        .gbufferAlbedoImageView = gbufferAlbedoImageView_,
        .createImage = [this](
                           std::uint32_t width,
                           std::uint32_t height,
                           VkFormat format,
                           VkImageTiling tiling,
                           VkImageUsageFlags usage,
                           VkMemoryPropertyFlags properties,
                           VkImage& image,
                           VkDeviceMemory& imageMemory
                       ) {
            createImage(width, height, format, tiling, usage, properties, image, imageMemory);
        },
        .createImageView = [this](VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
            return createImageView(image, format, aspectFlags);
        },
    };
    renderTargetsBuilder_.createGBufferResources(context);
}

void Renderer::createFramebuffers() {
    RenderTargetsBuilder::FramebufferContext context{
        .device = device_,
        .renderPass = renderPass_,
        .uiRenderPass = uiRenderPass_,
        .extent = swapchainExtent_,
        .sceneColorImageView = sceneColorImageView_,
        .depthImageView = depthImageView_,
        .gbufferPositionImageView = gbufferPositionImageView_,
        .gbufferNormalImageView = gbufferNormalImageView_,
        .gbufferAlbedoImageView = gbufferAlbedoImageView_,
        .swapchainImageViews = swapchainImageViews_,
        .sceneFramebuffer = sceneFramebuffer_,
        .swapchainFramebuffers = swapchainFramebuffers_,
    };
    renderTargetsBuilder_.createFramebuffers(context);
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
    if (device_ == VK_NULL_HANDLE || modelImportService_.inProgress()) {
        return false;
    }

    startAsyncModelImport(path);
    return true;
}

void Renderer::startAsyncModelImport(const std::string& path) {
    (void)modelImportService_.startAsyncImport(device_, path, [this](std::string message) {
        appendOutput(std::move(message));
    });
}

void Renderer::pollAsyncModelImport() {
    ModelImportService::PollResult result = modelImportService_.poll();
    if (result.status == ModelImportService::PollStatus::NoUpdate) {
        return;
    }

    if (result.status == ModelImportService::PollStatus::InvalidTask) {
        appendOutput("Model import failed: invalid async task");
        return;
    }

    if (result.status == ModelImportService::PollStatus::FailedEmpty) {
        appendOutput(std::string("Model import failed: ") + result.path);
        return;
    }

    MeshInputData meshData = std::move(result.meshData);

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
    meshInputPath_ = std::move(result.path);
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
    DescriptorBuilder::DescriptorPoolContext context{
        .device = device_,
        .maxFramesInFlight = kMaxFramesInFlight,
        .descriptorPool = descriptorPool_,
    };
    descriptorBuilder_.createDescriptorPool(context);
}

void Renderer::createDescriptorSets() {
    DescriptorBuilder::DescriptorSetsContext context{
        .device = device_,
        .maxFramesInFlight = kMaxFramesInFlight,
        .descriptorPool = descriptorPool_,
        .descriptorSetLayout = descriptorSetLayout_,
        .uniformBuffers = uniformBuffers_.data(),
        .descriptorSets = descriptorSets_.data(),
        .uniformBufferRange = sizeof(UniformBufferObject),
    };
    descriptorBuilder_.createDescriptorSets(context);
}

void Renderer::createLightingDescriptorPool() {
    DescriptorBuilder::LightingDescriptorPoolContext context{
        .device = device_,
        .lightingDescriptorPool = lightingDescriptorPool_,
    };
    descriptorBuilder_.createLightingDescriptorPool(context);
}

void Renderer::createLightingDescriptorSet() {
    DescriptorBuilder::LightingDescriptorSetContext context{
        .device = device_,
        .lightingDescriptorPool = lightingDescriptorPool_,
        .lightingDescriptorSetLayout = lightingDescriptorSetLayout_,
        .lightingDescriptorSet = lightingDescriptorSet_,
    };
    descriptorBuilder_.createLightingDescriptorSet(context);
}

void Renderer::updateLightingDescriptorSet() {
    DescriptorBuilder::UpdateLightingDescriptorSetContext context{
        .device = device_,
        .lightingDescriptorSet = lightingDescriptorSet_,
        .gbufferPositionImageView = gbufferPositionImageView_,
        .gbufferNormalImageView = gbufferNormalImageView_,
        .gbufferAlbedoImageView = gbufferAlbedoImageView_,
    };
    descriptorBuilder_.updateLightingDescriptorSet(context);
}

void Renderer::createImGuiDescriptorPool() {
    imguiBackend_.createDescriptorPool(device_, imguiDescriptorPool_);
}

void Renderer::initImGui() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    ImGuiBackend::InitContext context = imguiContextBuilder_.build(ImGuiContextBuilder::Source{
        .windowHandle = windowHandle_,
        .instance = instance_,
        .physicalDevice = physicalDevice_,
        .device = device_,
        .queueFamily = indices.graphicsFamily,
        .queue = graphicsQueue_,
        .descriptorPool = imguiDescriptorPool_,
        .minImageCount = swapchainMinImageCount_,
        .swapchainImages = &swapchainImages_,
        .uiRenderPass = uiRenderPass_,
        .sceneColorSampler = sceneColorSampler_,
        .sceneColorImageView = sceneColorImageView_,
        .sceneTextureId = sceneTextureId_,
        .appendOutput = [this](std::string message) {
            appendOutput(std::move(message));
        },
    });
    imguiBackend_.init(context);
}

void Renderer::shutdownImGui() {
    imguiBackend_.shutdown(device_, imguiDescriptorPool_, sceneTextureId_);
}

void Renderer::buildGui() {
    imguiBackend_.beginFrame();

    GuiLayer::BuildContext context = guiContextBuilder_.build(GuiContextBuilder::Source{
        .swapchainExtent = swapchainExtent_,
        .sceneTextureId = sceneTextureId_,
        .sceneViewHovered = &sceneViewHovered_,
        .sceneViewFocused = &sceneViewFocused_,
        .sceneViewportX = &sceneViewportX_,
        .sceneViewportY = &sceneViewportY_,
        .sceneViewportWidth = &sceneViewportWidth_,
        .sceneViewportHeight = &sceneViewportHeight_,
        .modelTranslation = &modelTranslation_,
        .modelYawRadians = &modelYawRadians_,
        .modelPitchRadians = &modelPitchRadians_,
        .cameraDistance = &cameraDistance_,
        .sceneRadius = sceneRadius_,
        .lightingDebugMode = &lightingDebugMode_,
        .deferredLightCount = &deferredLightCount_,
        .deferredLightIntensity = &deferredLightIntensity_,
        .useProceduralMaterialMaps = &useProceduralMaterialMaps_,
        .materialMetallic = &materialMetallic_,
        .materialRoughness = &materialRoughness_,
        .materialAo = &materialAo_,
        .iblIntensity = &iblIntensity_,
        .modelImportInProgress = modelImportService_.inProgress(),
        .outputLines = &outputLines_,
        .autoScrollOutput = &autoScrollOutput_,
        .smoothedFrameTimeMs = &smoothedFrameTimeMs_,
        .smoothedFps = &smoothedFps_,
        .onAppendOutput = [this](std::string message) {
            appendOutput(std::move(message));
        },
        .onOpenModelDialog = [this](std::string& outPath) {
            return openModelFileDialog(outPath);
        },
        .onReloadModelPath = [this](const std::string& path) {
            return reloadModelFromPath(path);
        },
    });

    guiLayer_.build(context);

    imguiBackend_.endFrame();
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
    SyncManager::CreateContext<kMaxFramesInFlight> context{
        .device = device_,
        .imageAvailableSemaphores = imageAvailableSemaphores_,
        .inFlightFences = inFlightFences_,
        .renderFinishedSemaphores = renderFinishedSemaphores_,
        .imagesInFlight = imagesInFlight_,
        .swapchainImageCount = swapchainImages_.size(),
    };
    syncManager_.createSyncObjects(context);

    lifecycleValidator_.validateSyncCollections(
        swapchainImages_.size(),
        renderFinishedSemaphores_,
        imagesInFlight_
    );
}

void Renderer::updateUniformBuffer(std::uint32_t frameIndex, float timeSeconds) {
    (void)timeSeconds;
    UniformBufferObject ubo{};

    CameraUniformService::Matrices matrices = cameraUniformService_.buildMatrices(CameraUniformService::Source{
        .modelTranslation = modelTranslation_,
        .modelYawRadians = modelYawRadians_,
        .modelPitchRadians = modelPitchRadians_,
        .cameraDistance = cameraDistance_,
        .swapchainExtent = swapchainExtent_,
        .sceneRadius = sceneRadius_,
    });

    ubo.model = matrices.model;
    ubo.view = matrices.view;
    ubo.projection = matrices.projection;

    void* data = nullptr;
    vkMapMemory(device_, uniformBuffersMemory_[frameIndex], 0, sizeof(ubo), 0, &data);
    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(device_, uniformBuffersMemory_[frameIndex]);
}

void Renderer::drawFrame() {
    DrawFrameOrchestrator::Context context{
        .device = device_,
        .graphicsQueue = graphicsQueue_,
        .presentQueue = presentQueue_,
        .swapchain = swapchain_,
        .inFlightFences = inFlightFences_.data(),
        .imageAvailableSemaphores = imageAvailableSemaphores_.data(),
        .commandBuffers = commandBuffers_.data(),
        .renderFinishedSemaphores = &renderFinishedSemaphores_,
        .imagesInFlight = &imagesInFlight_,
        .maxFramesInFlight = kMaxFramesInFlight,
        .currentFrame = currentFrame_,
        .framebufferResized = framebufferResized_,
        .perfReportAccumulatorSeconds = perfReportAccumulatorSeconds_,
        .smoothedFrameTimeMs = smoothedFrameTimeMs_,
        .smoothedFps = smoothedFps_,
        .recreateSwapchain = [this]() {
            recreateSwapchain();
        },
        .pollAsyncModelImport = [this]() {
            pollAsyncModelImport();
        },
        .buildGui = [this]() {
            buildGui();
        },
        .updateUniformBuffer = [this](std::uint32_t frameIndex, float timeSeconds) {
            updateUniformBuffer(frameIndex, timeSeconds);
        },
        .recordCommandBuffer = [this](VkCommandBuffer commandBuffer, std::uint32_t imageIndex) {
            recordCommandBuffer(commandBuffer, imageIndex);
        },
        .appendOutput = [this](std::string message) {
            appendOutput(std::move(message));
        },
    };

    drawFrameOrchestrator_.execute(context);
}

void Renderer::recordScenePass(VkCommandBuffer commandBuffer) {
    LightingPushConstants lightingPushConstants = lightingPushBuilder_.build<LightingPushConstants>(
        LightingPushBuilder::Source{
            .lightingDebugMode = lightingDebugMode_,
            .deferredLightCount = deferredLightCount_,
            .deferredLightIntensity = deferredLightIntensity_,
            .sceneRadius = sceneRadius_,
            .materialMetallic = materialMetallic_,
            .materialRoughness = materialRoughness_,
            .materialAo = materialAo_,
            .cameraDistance = cameraDistance_,
            .useProceduralMaterialMaps = useProceduralMaterialMaps_,
            .iblIntensity = iblIntensity_,
        }
    );

    FrameRecorder::ScenePassContext context{};
    context.renderPass = renderPass_;
    context.framebuffer = sceneFramebuffer_;
    context.extent = swapchainExtent_;
    context.clearValues[0].color = {{0.05f, 0.07f, 0.11f, 1.0f}};
    context.clearValues[1].depthStencil = {1.0f, 0};
    context.clearValues[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    context.clearValues[3].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    context.clearValues[4].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    context.geometryPipeline = graphicsPipeline_;
    context.geometryPipelineLayout = pipelineLayout_;
    context.geometryDescriptorSet = descriptorSets_[currentFrame_];
    context.vertexBuffer = vertexBuffer_;
    context.indexBuffer = indexBuffer_;
    context.indexCount = static_cast<std::uint32_t>(meshIndices_.size());
    context.lightingPipeline = lightingPipeline_;
    context.lightingPipelineLayout = lightingPipelineLayout_;
    context.lightingDescriptorSet = lightingDescriptorSet_;
    context.lightingPushConstants = &lightingPushConstants;
    context.lightingPushConstantsSize = sizeof(LightingPushConstants);

    frameRecorder_.recordScenePass(commandBuffer, context);
}

void Renderer::recordUiPass(VkCommandBuffer commandBuffer, std::uint32_t imageIndex) {
    FrameRecorder::UiPassContext context{};
    context.renderPass = uiRenderPass_;
    context.framebuffer = swapchainFramebuffers_[imageIndex];
    context.extent = swapchainExtent_;
    context.clearValue.color = {{0.02f, 0.03f, 0.05f, 1.0f}};
    context.drawData = ImGui::GetDrawData();

    frameRecorder_.recordUiPass(commandBuffer, context);
}

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("vkBeginCommandBuffer failed");
    }

    frameGraph_.clear();
    frameGraph_.addPass(std::make_unique<SceneFrameGraphPass>([this, commandBuffer]() {
        recordScenePass(commandBuffer);
    }));
    frameGraph_.addPass(std::make_unique<UiFrameGraphPass>([this, commandBuffer, imageIndex]() {
        recordUiPass(commandBuffer, imageIndex);
    }));

    frameGraph_.compile();
    frameGraph_.execute();

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("vkEndCommandBuffer failed");
    }
}

void Renderer::recreateSwapchain() {
    SwapchainRebuilder::RebuildContext context{
        .windowWidth = windowWidth_,
        .windowHeight = windowHeight_,
        .device = device_,
        .processWindowMessages = [this]() {
            return processWindowMessages();
        },
        .cleanupSwapchain = [this]() {
            cleanupSwapchain();
        },
        .createSwapchain = [this]() {
            createSwapchain();
        },
        .createImageViews = [this]() {
            createImageViews();
        },
        .createRenderPass = [this]() {
            createRenderPass();
        },
        .createGraphicsPipeline = [this]() {
            createGraphicsPipeline();
        },
        .createLightingPipeline = [this]() {
            createLightingPipeline();
        },
        .createDepthResources = [this]() {
            createDepthResources();
        },
        .createGBufferResources = [this]() {
            createGBufferResources();
        },
        .createFramebuffers = [this]() {
            createFramebuffers();
        },
        .updateLightingDescriptorSet = [this]() {
            updateLightingDescriptorSet();
        },
        .refreshSceneTextureDescriptor = [this]() {
            swapchainLifecycle_.removeSceneTextureDescriptor(sceneTextureId_);
            swapchainLifecycle_.addSceneTextureDescriptor(sceneTextureId_, sceneColorSampler_, sceneColorImageView_);
        },
        .updateImGuiMinImageCount = [this]() {
            ImGui_ImplVulkan_SetMinImageCount(swapchainMinImageCount_);
        },
        .appendSwapchainRecreatedLog = [this]() {
            appendOutput("Swapchain recreated");
        },
        .recreateRenderFinishedSemaphores = [this]() {
            SyncManager::RecreateRenderFinishedContext syncContext{
                .device = device_,
                .renderFinishedSemaphores = renderFinishedSemaphores_,
                .swapchainImageCount = swapchainImages_.size(),
            };
            syncManager_.recreateRenderFinishedSemaphores(syncContext);
        },
        .resetImagesInFlight = [this]() {
            imagesInFlight_.assign(swapchainImages_.size(), VK_NULL_HANDLE);
            lifecycleValidator_.validateSyncCollections(
                swapchainImages_.size(),
                renderFinishedSemaphores_,
                imagesInFlight_
            );
        },
    };

    (void)swapchainRebuilder_.rebuild(context);
}

void Renderer::cleanupSwapchain() {
    SwapchainLifecycle::CleanupContext cleanupContext{
        .device = device_,
        .sceneTextureId = sceneTextureId_,
        .sceneFramebuffer = sceneFramebuffer_,
        .swapchainFramebuffers = swapchainFramebuffers_,
        .depthImageView = depthImageView_,
        .depthImage = depthImage_,
        .depthImageMemory = depthImageMemory_,
        .gbufferPositionImageView = gbufferPositionImageView_,
        .gbufferPositionImage = gbufferPositionImage_,
        .gbufferPositionImageMemory = gbufferPositionImageMemory_,
        .gbufferNormalImageView = gbufferNormalImageView_,
        .gbufferNormalImage = gbufferNormalImage_,
        .gbufferNormalImageMemory = gbufferNormalImageMemory_,
        .gbufferAlbedoImageView = gbufferAlbedoImageView_,
        .gbufferAlbedoImage = gbufferAlbedoImage_,
        .gbufferAlbedoImageMemory = gbufferAlbedoImageMemory_,
        .sceneColorSampler = sceneColorSampler_,
        .sceneColorImageView = sceneColorImageView_,
        .sceneColorImage = sceneColorImage_,
        .sceneColorImageMemory = sceneColorImageMemory_,
        .graphicsPipeline = graphicsPipeline_,
        .lightingPipeline = lightingPipeline_,
        .pipelineLayout = pipelineLayout_,
        .lightingPipelineLayout = lightingPipelineLayout_,
        .renderPass = renderPass_,
        .uiRenderPass = uiRenderPass_,
        .swapchainImageViews = swapchainImageViews_,
        .swapchain = swapchain_,
    };
    swapchainLifecycle_.cleanupResources(cleanupContext);
}

Renderer::QueueFamilyIndices Renderer::findQueueFamilies(VkPhysicalDevice device) const {
    SwapchainQuery::QueueFamilyIndices indices = swapchainQuery_.findQueueFamilies(device, surface_);
    return QueueFamilyIndices{
        .graphicsFamily = indices.graphicsFamily,
        .presentFamily = indices.presentFamily,
    };
}

Renderer::SwapchainSupportDetails Renderer::querySwapchainSupport(VkPhysicalDevice device) const {
    SwapchainQuery::SwapchainSupportDetails details = swapchainQuery_.querySwapchainSupport(device, surface_);
    return SwapchainSupportDetails{
        .capabilities = details.capabilities,
        .formats = std::move(details.formats),
        .presentModes = std::move(details.presentModes),
    };
}

VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
    return swapchainQuery_.chooseSwapSurfaceFormat(formats);
}

VkPresentModeKHR Renderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes) const {
    return swapchainQuery_.chooseSwapPresentMode(presentModes);
}

VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    return swapchainQuery_.chooseSwapExtent(capabilities, windowWidth_, windowHeight_);
}

bool Renderer::isDeviceSuitable(VkPhysicalDevice device) const {
    std::vector<const char*> requiredExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());
    return swapchainQuery_.isDeviceSuitable(device, surface_, requiredExtensions);
}

bool Renderer::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    std::vector<const char*> requiredExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());
    return swapchainQuery_.checkDeviceExtensionSupport(device, requiredExtensions);
}

VkFormat Renderer::findSupportedFormat(
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features
) const {
    return formatSelector_.findSupportedFormat(physicalDevice_, candidates, tiling, features);
}

VkFormat Renderer::findDepthFormat() const {
    return formatSelector_.findDepthFormat(physicalDevice_);
}

std::vector<char> Renderer::readBinaryFile(const char* filePath) {
    ShaderManager shaderManager;
    return shaderManager.readBinaryFile(filePath);
}

std::uint32_t Renderer::findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    return bufferImageAllocator_.findMemoryType(physicalDevice_, typeFilter, properties);
}

void Renderer::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory
) {
    bufferImageAllocator_.createBuffer(device_, physicalDevice_, size, usage, properties, buffer, bufferMemory);
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
    bufferImageAllocator_.createImage(
        device_,
        physicalDevice_,
        width,
        height,
        format,
        tiling,
        usage,
        properties,
        image,
        imageMemory
    );
}

VkImageView Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const {
    return bufferImageAllocator_.createImageView(device_, image, format, aspectFlags);
}

VkShaderModule Renderer::createShaderModule(const std::vector<char>& code) const {
    return shaderManager_.createShaderModule(device_, code);
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
    const bool isMouseMessage = message == WM_RBUTTONDOWN ||
                                message == WM_RBUTTONUP ||
                                message == WM_LBUTTONDOWN ||
                                message == WM_LBUTTONUP ||
                                message == WM_MOUSEMOVE ||
                                message == WM_MOUSEWHEEL;

    bool imguiHandled = false;
    bool uiCapturingMouse = false;
    if (ImGui::GetCurrentContext() != nullptr) {
        imguiHandled = (::ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam) != 0);
        uiCapturingMouse = ImGui::GetIO().WantCaptureMouse;
    }

    const bool sceneAllowsMouseInteraction = sceneViewHovered_ || sceneViewFocused_;
    if (imguiHandled && !(isMouseMessage && sceneAllowsMouseInteraction)) {
        return 1;
    }

    InputController::State inputState{
        .windowRunning = windowRunning_,
        .rightDragActive = rightDragActive_,
        .leftDragActive = leftDragActive_,
        .windowWidth = windowWidth_,
        .windowHeight = windowHeight_,
        .framebufferResized = framebufferResized_,
        .lastMousePosition = lastMousePosition_,
        .modelTranslation = modelTranslation_,
        .modelYawRadians = modelYawRadians_,
        .modelPitchRadians = modelPitchRadians_,
        .cameraDistance = cameraDistance_,
        .sceneRadius = sceneRadius_,
    };

    InputController::HandleResult result =
        inputController_.handleWindowInput(inputState, message, wParam, lParam, uiCapturingMouse, sceneAllowsMouseInteraction);
    if (result.handled) {
        return result.value;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

} // namespace vr


