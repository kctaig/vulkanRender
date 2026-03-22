/**
 * @file DrawFrameOrchestrator.h
 * @brief Declarations for the DrawFrameOrchestrator module.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "renderer/frame/PresentFlow.h"

namespace vr {

class DrawFrameOrchestrator {
public:
    struct Context {
        VkDevice device = VK_NULL_HANDLE;
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue presentQueue = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;

        VkFence* inFlightFences = nullptr;
        VkSemaphore* imageAvailableSemaphores = nullptr;
        VkCommandBuffer* commandBuffers = nullptr;
        std::vector<VkSemaphore>* renderFinishedSemaphores = nullptr;
        std::vector<VkFence>* imagesInFlight = nullptr;

        std::uint32_t maxFramesInFlight = 0;
        std::uint32_t& currentFrame;
        bool& framebufferResized;

        float& perfReportAccumulatorSeconds;
        float smoothedFrameTimeMs = 0.0f;
        float smoothedFps = 0.0f;

        std::function<void()> recreateSwapchain;
        std::function<void()> pollAsyncModelImport;
        std::function<void()> buildGui;
        std::function<void(std::uint32_t, float)> updateUniformBuffer;
        std::function<void(VkCommandBuffer, std::uint32_t)> recordCommandBuffer;
        std::function<void(std::string)> appendOutput;
    };

    void execute(Context& context) const;

private:
    PresentFlow presentFlow_;
};

} // namespace vr


