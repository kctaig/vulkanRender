/**
 * @file Renderer.h
 * @brief Main renderer orchestrator declarations.
 */
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include "renderer/frame/FrameRecorder.h"
#include "renderer/frame/CameraUniformService.h"
#include "renderer/frame/DrawFrameOrchestrator.h"
#include "renderer/frame/LightingPushBuilder.h"
#include "renderer/frame/SyncManager.h"
#include "renderer/ui/GuiContextBuilder.h"
#include "renderer/ui/GuiLayer.h"
#include "renderer/ui/ImGuiContextBuilder.h"
#include "renderer/ui/ImGuiBackend.h"
#include "renderer/descriptor/DescriptorBuilder.h"
#include "renderer/format/FormatSelector.h"
#include "renderer/framegraph/RenderGraph.h"
#include "renderer/input/InputController.h"
#include "renderer/lifecycle/RendererTeardown.h"
#include "renderer/lifecycle/RendererInitPipeline.h"
#include "renderer/lifecycle/ResourceLifecycleValidator.h"
#include "renderer/lifecycle/SwapchainLifecycle.h"
#include "renderer/lifecycle/SwapchainRebuilder.h"
#include "renderer/lifecycle/VulkanBootstrap.h"
#include "renderer/memory/BufferImageAllocator.h"
#include "renderer/model/ModelImportService.h"
#include "renderer/pipeline/PipelineBuilder.h"
#include "renderer/query/SwapchainQuery.h"
#include "renderer/resources/RenderTargetsBuilder.h"
#include "renderer/shader/ShaderManager.h"
#include "renderer/swapchain/Swapchain.h"
#include "scene/MeshIO.h"

namespace vr {

/**
 * @brief High-level Vulkan renderer.
 *
 * This class owns the render window, Vulkan instance/device/swapchain state,
 * frame synchronization objects, and UI integration. It delegates specialized
 * tasks to dedicated helper modules while keeping lifecycle orchestration here.
 */
class Renderer {
public:
    /** @brief Vertex layout used by geometry buffers and graphics pipelines. */
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;

        /** @brief Returns the Vulkan binding description for this vertex type. */
        static VkVertexInputBindingDescription getBindingDescription();
        /** @brief Returns Vulkan attribute descriptions for position/normal/uv. */
        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
    };

    /** @brief Uniform data written each frame for the active camera/model state. */
    struct UniformBufferObject {
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
    };

    /** @brief Push constants consumed by the deferred lighting pass. */
    struct LightingPushConstants {
        std::int32_t debugMode = 0;
        std::int32_t lightCount = 8;
        float lightIntensity = 2.0f;
        float positionDebugScale = 1.0f;
        float metallic = 0.1f;
        float roughness = 0.6f;
        float ao = 1.0f;
        float cameraDistance = 3.5f;
        float materialTextureWeight = 1.0f;
        float iblIntensity = 1.0f;
        float padding0 = 0.0f;
        float padding1 = 0.0f;
    };

    /** @brief Initializes the window and Vulkan runtime objects. */
    bool initialize(unsigned int width, unsigned int height);
    /** @brief Sets an optional mesh file path used on startup. */
    void setMeshInputPath(std::string path);
    /** @brief Runs the render loop until shutdown is requested. */
    void mainLoop();
    /** @brief Destroys all renderer-owned resources in dependency-safe order. */
    void shutdown();

private:
    struct QueueFamilyIndices {
        std::uint32_t graphicsFamily = UINT32_MAX;
        std::uint32_t presentFamily = UINT32_MAX;

        bool isComplete() const {
            return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
        }
    };

    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    static constexpr std::uint32_t kMaxFramesInFlight = 2;

    bool initWindow(unsigned int width, unsigned int height);
    bool initVulkan();
    VulkanBootstrap::BootstrapContext buildBootstrapContext();
    bool processWindowMessages();
    void createInstance();
    void setupDebugMessenger();
    void destroyDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createLightingDescriptorSetLayout();
    void createGraphicsPipeline();
    void createLightingPipeline();
    void createDepthResources();
    void createGBufferResources();
    void createFramebuffers();
    void createCommandPool();
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createLightingDescriptorPool();
    void createLightingDescriptorSet();
    void updateLightingDescriptorSet();
    void createImGuiDescriptorPool();
    void initImGui();
    void shutdownImGui();
    void createCommandBuffers();
    void createSyncObjects();
    void updateUniformBuffer(std::uint32_t frameIndex, float timeSeconds);
    void buildGui();
    void appendOutput(std::string message);
    void drawFrame();
    void recordScenePass(VkCommandBuffer commandBuffer);
    void recordUiPass(VkCommandBuffer commandBuffer, std::uint32_t imageIndex);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex);
    void recreateSwapchain();
    void cleanupSwapchain();
    void loadMeshVertices();
    bool openModelFileDialog(std::string& outPath) const;
    bool reloadModelFromPath(const std::string& path);
    void startAsyncModelImport(const std::string& path);
    void pollAsyncModelImport();
    void refreshSceneScaleParams();

    [[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    [[nodiscard]] SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) const;
    [[nodiscard]] VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    [[nodiscard]] VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes) const;
    [[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    [[nodiscard]] bool isDeviceSuitable(VkPhysicalDevice device) const;
    [[nodiscard]] bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    [[nodiscard]] VkFormat findSupportedFormat(
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    ) const;
    [[nodiscard]] VkFormat findDepthFormat() const;

    static std::vector<char> readBinaryFile(const char* filePath);
    std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    );
    void createImage(
        std::uint32_t width,
        std::uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory
    );
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;
    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    HWND windowHandle_ = nullptr;
    HINSTANCE instanceHandle_ = nullptr;
    bool windowRunning_ = true;
    bool rightDragActive_ = false;
    bool leftDragActive_ = false;
    bool sceneViewHovered_ = false;
    bool sceneViewFocused_ = false;
    int sceneViewportX_ = 0;
    int sceneViewportY_ = 0;
    int sceneViewportWidth_ = 0;
    int sceneViewportHeight_ = 0;
    POINT lastMousePosition_{0, 0};

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages_;
    std::uint32_t swapchainMinImageCount_ = 2;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkRenderPass uiRenderPass_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout lightingDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout lightingPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkPipeline lightingPipeline_ = VK_NULL_HANDLE;

    VkFramebuffer sceneFramebuffer_ = VK_NULL_HANDLE;

    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;

    VkImage gbufferPositionImage_ = VK_NULL_HANDLE;
    VkDeviceMemory gbufferPositionImageMemory_ = VK_NULL_HANDLE;
    VkImageView gbufferPositionImageView_ = VK_NULL_HANDLE;
    VkImage gbufferNormalImage_ = VK_NULL_HANDLE;
    VkDeviceMemory gbufferNormalImageMemory_ = VK_NULL_HANDLE;
    VkImageView gbufferNormalImageView_ = VK_NULL_HANDLE;
    VkImage gbufferAlbedoImage_ = VK_NULL_HANDLE;
    VkDeviceMemory gbufferAlbedoImageMemory_ = VK_NULL_HANDLE;
    VkImageView gbufferAlbedoImageView_ = VK_NULL_HANDLE;
    VkImage sceneColorImage_ = VK_NULL_HANDLE;
    VkDeviceMemory sceneColorImageMemory_ = VK_NULL_HANDLE;
    VkImageView sceneColorImageView_ = VK_NULL_HANDLE;
    VkSampler sceneColorSampler_ = VK_NULL_HANDLE;
    VkFormat gbufferPositionFormat_ = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat gbufferNormalFormat_ = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat gbufferAlbedoFormat_ = VK_FORMAT_R8G8B8A8_UNORM;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, kMaxFramesInFlight> commandBuffers_{};

    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    std::vector<Vertex> meshVertices_;
    std::vector<std::uint32_t> meshIndices_;
    ModelImportService modelImportService_;
    float sceneRadius_ = 1.0f;
    RenderGraph frameGraph_;
    CameraUniformService cameraUniformService_;
    DrawFrameOrchestrator drawFrameOrchestrator_;
    FrameRecorder frameRecorder_;
    LightingPushBuilder lightingPushBuilder_;
    SyncManager syncManager_;
    GuiContextBuilder guiContextBuilder_;
    ImGuiContextBuilder imguiContextBuilder_;
    GuiLayer guiLayer_;
    ImGuiBackend imguiBackend_;
    DescriptorBuilder descriptorBuilder_;
    FormatSelector formatSelector_;
    InputController inputController_;
    RendererInitPipeline rendererInitPipeline_;
    RendererTeardown rendererTeardown_;
    ResourceLifecycleValidator lifecycleValidator_;
    SwapchainLifecycle swapchainLifecycle_;
    SwapchainRebuilder swapchainRebuilder_;
    VulkanBootstrap vulkanBootstrap_;
    BufferImageAllocator bufferImageAllocator_;
    SwapchainQuery swapchainQuery_;
    Swapchain swapchainManager_;
    PipelineBuilder pipelineBuilder_;
    RenderTargetsBuilder renderTargetsBuilder_;
    ShaderManager shaderManager_;

    std::array<VkBuffer, kMaxFramesInFlight> uniformBuffers_{};
    std::array<VkDeviceMemory, kMaxFramesInFlight> uniformBuffersMemory_{};
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, kMaxFramesInFlight> descriptorSets_{};
    VkDescriptorPool lightingDescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet lightingDescriptorSet_ = VK_NULL_HANDLE;
    VkDescriptorPool imguiDescriptorPool_ = VK_NULL_HANDLE;
    void* sceneTextureId_ = nullptr;

    std::array<VkSemaphore, kMaxFramesInFlight> imageAvailableSemaphores_{};
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::array<VkFence, kMaxFramesInFlight> inFlightFences_{};
    std::vector<VkFence> imagesInFlight_;
    std::uint32_t currentFrame_ = 0;

    std::string meshInputPath_;
    glm::vec3 modelTranslation_ = glm::vec3(0.0f);
    float modelYawRadians_ = 0.0f;
    float modelPitchRadians_ = 0.0f;
    float cameraDistance_ = 3.5f;
    std::int32_t lightingDebugMode_ = 0;
    std::int32_t deferredLightCount_ = 8;
    float deferredLightIntensity_ = 2.0f;
    float materialMetallic_ = 0.1f;
    float materialRoughness_ = 0.6f;
    float materialAo_ = 1.0f;
    bool useProceduralMaterialMaps_ = true;
    float iblIntensity_ = 1.0f;
    std::vector<std::string> outputLines_;
    bool autoScrollOutput_ = true;
    float smoothedFrameTimeMs_ = 0.0f;
    float smoothedFps_ = 0.0f;
    float perfReportAccumulatorSeconds_ = 0.0f;
    unsigned int windowWidth_ = 1600;
    unsigned int windowHeight_ = 900;
    bool framebufferResized_ = false;

    bool ready_ = false;
};

} // namespace vr

