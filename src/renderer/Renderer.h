#pragma once

#include <array>
#include <cstdint>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

namespace vr {

class Renderer {
public:
    struct Vertex {
        glm::vec2 position;
        glm::vec3 color;

        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
    };

    bool initialize(unsigned int width, unsigned int height);
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
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createVertexBuffer();
    void createCommandBuffers();
    void createSyncObjects();
    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex, const glm::mat4& mvp);
    void recreateSwapchain();
    void cleanupSwapchain();

    [[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    [[nodiscard]] SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) const;
    [[nodiscard]] VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    [[nodiscard]] VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes) const;
    [[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    [[nodiscard]] bool isDeviceSuitable(VkPhysicalDevice device) const;
    [[nodiscard]] bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;

    static std::vector<char> readBinaryFile(const char* filePath);
    std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    );
    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* window_ = nullptr;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages_;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, kMaxFramesInFlight> commandBuffers_{};

    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;

    std::array<VkSemaphore, kMaxFramesInFlight> imageAvailableSemaphores_{};
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::array<VkFence, kMaxFramesInFlight> inFlightFences_{};
    std::vector<VkFence> imagesInFlight_;
    std::uint32_t currentFrame_ = 0;

    unsigned int windowWidth_ = 1600;
    unsigned int windowHeight_ = 900;
    bool framebufferResized_ = false;

    bool ready_ = false;
};

} // namespace vr
