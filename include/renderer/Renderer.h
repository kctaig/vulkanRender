#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>
#include <vulkan/vulkan.h>

#include "core/VulkanContext.h"
#include "renderer/RenderPass.h"
#include "scene/Scene.h"

namespace vr {

class Renderer {
  public:
    bool initialize(unsigned int width, unsigned int height);
    void setMeshInputPath(std::string path);
    void mainLoop();
    void shutdown();

  private:
    bool initWindow(unsigned int width, unsigned int height);
    bool processWindowMessages();
    void drawFrame();
    void recreateSwapchain();

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    VulkanContext ctx_;
    Scene scene_;
    std::vector<std::unique_ptr<RenderPass>> passes_;

    HWND windowHandle_ = nullptr;
    HINSTANCE instanceHandle_ = nullptr;
    bool windowRunning_ = true;

    bool rightDragActive_ = false;
    bool leftDragActive_ = false;
    POINT lastMousePosition_{0, 0};

    std::array<VkSemaphore, RenderPass::kMaxFramesInFlight> imageAvailableSemaphores_{};
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::array<VkFence, RenderPass::kMaxFramesInFlight> inFlightFences_{};
    std::vector<VkFence> imagesInFlight_;
    std::array<VkCommandBuffer, RenderPass::kMaxFramesInFlight> commandBuffers_{};
    std::uint32_t currentFrame_ = 0;

    unsigned int windowWidth_ = 1600;
    unsigned int windowHeight_ = 900;
    bool framebufferResized_ = false;

    bool ready_ = false;
};

}  // namespace vr
