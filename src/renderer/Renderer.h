#pragma once

#include <array>
#include <cstdint>
#include <future>
#include <string>
#include <vector>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include "scene/MeshIO.h"

namespace vr {

class Renderer {
public:
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;

        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
    };

    struct UniformBufferObject {
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
    };

    struct LightingPushConstants {
        std::int32_t debugMode = 0;
        std::int32_t lightCount = 8;
        float lightIntensity = 2.0f;
        float positionDebugScale = 1.0f;
        float metallic = 0.1f;
        float roughness = 0.6f;
        float ao = 1.0f;
        float cameraDistance = 3.5f;
    };

    bool initialize(unsigned int width, unsigned int height);
    void setMeshInputPath(std::string path);
    void mainLoop();
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
    std::future<MeshInputData> pendingMeshLoadTask_;
    bool modelImportInProgress_ = false;
    std::string pendingMeshPath_;
    float sceneRadius_ = 1.0f;

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
    std::vector<std::string> outputLines_;
    bool autoScrollOutput_ = true;
    unsigned int windowWidth_ = 1600;
    unsigned int windowHeight_ = 900;
    bool framebufferResized_ = false;

    bool ready_ = false;
};

} // namespace vr
