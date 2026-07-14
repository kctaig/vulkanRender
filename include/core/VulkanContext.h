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

namespace vr {

class VulkanContext {
  public:
    struct CreateInfo {
        HWND windowHandle = nullptr;
        HINSTANCE instanceHandle = nullptr;
        unsigned int initialWidth = 1600;
        unsigned int initialHeight = 900;
        bool enableValidation = true;
    };

    struct QueueFamilyIndices {
        std::uint32_t graphicsFamily = UINT32_MAX;
        std::uint32_t presentFamily = UINT32_MAX;

        [[nodiscard]] bool isComplete() const {
            return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
        }
    };

    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    bool initialize(const CreateInfo& info);
    void shutdown();

    // --- Core accessors ---
    VkInstance instance() const;
    VkPhysicalDevice physicalDevice() const;
    VkDevice device() const;
    VkQueue graphicsQueue() const;
    VkQueue presentQueue() const;
    VkSurfaceKHR surface() const;
    VkCommandPool commandPool() const;
    VkSwapchainKHR swapchain() const;

    // --- Swapchain accessors ---
    VkFormat swapchainFormat() const;
    VkExtent2D swapchainExtent() const;
    std::uint32_t swapchainMinImageCount() const;
    std::uint32_t swapchainImageCount() const;
    const std::vector<VkImage>& swapchainImages() const;
    const std::vector<VkImageView>& swapchainImageViews() const;

    // --- Framebuffer size (for swapchain extent fallback) ---
    void setFramebufferSize(std::uint32_t width, std::uint32_t height);

    // --- Swapchain operations ---
    bool acquireNextImage(VkSemaphore signalSemaphore, std::uint32_t& outImageIndex);
    void recreateSwapchain();
    void cleanupSwapchain();

    // --- Resource creation helpers ---
    [[nodiscard]] std::uint32_t findMemoryType(
        std::uint32_t typeFilter, VkMemoryPropertyFlags properties
    ) const;
    [[nodiscard]] VkFormat findSupportedFormat(
        const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features
    ) const;
    [[nodiscard]] VkFormat findDepthFormat() const;
    void createBuffer(
        VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& bufferMemory
    );
    void createImage(
        std::uint32_t width, std::uint32_t height, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image,
        VkDeviceMemory& imageMemory
    );
    [[nodiscard]] VkImageView createImageView(
        VkImage image, VkFormat format, VkImageAspectFlags aspectFlags
    ) const;
    [[nodiscard]] VkShaderModule createShaderModule(const std::vector<char>& code) const;

    // --- Command buffer allocation ---
    [[nodiscard]] std::vector<VkCommandBuffer> allocateCommandBuffers(std::uint32_t count) const;

    // --- Query helpers ---
    [[nodiscard]] QueueFamilyIndices findQueueFamilies() const;
    [[nodiscard]] SwapchainSupportDetails querySwapchainSupport() const;
    [[nodiscard]] bool validationEnabled() const;

    // --- Static utilities ---
    static std::vector<char> readBinaryFile(const char* filePath);
    static bool hasStencilComponent(VkFormat format);

  private:
    void createInstance();
    void setupDebugMessenger();
    void destroyDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createCommandPool();

    [[nodiscard]] bool isDeviceSuitable(VkPhysicalDevice device) const;
    [[nodiscard]] bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    [[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    [[nodiscard]] SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) const;
    [[nodiscard]] VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& formats
    ) const;
    [[nodiscard]] VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& presentModes
    ) const;
    [[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    HWND windowHandle_ = nullptr;
    HINSTANCE instanceHandle_ = nullptr;
    bool validationEnabled_ = false;

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

    VkCommandPool commandPool_ = VK_NULL_HANDLE;

    unsigned int initialWidth_ = 1600;
    unsigned int initialHeight_ = 900;
    unsigned int framebufferWidth_ = 1600;
    unsigned int framebufferHeight_ = 900;
};

}  // namespace vr
