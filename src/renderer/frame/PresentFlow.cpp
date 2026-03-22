/**
 * @file PresentFlow.cpp
 * @brief Implementation for the PresentFlow module.
 */
#include "renderer/frame/PresentFlow.h"

#include <stdexcept>

namespace vr {

bool PresentFlow::shouldSkipFrameOnAcquireResult(VkResult acquireResult, const std::function<void()>& recreateSwapchain) const {
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return true;
    }

    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("vkAcquireNextImageKHR failed");
    }

    return false;
}

void PresentFlow::handlePresentResult(
    VkResult presentResult,
    bool& framebufferResized,
    const std::function<void()>& recreateSwapchain
) const {
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapchain();
        return;
    }

    if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("vkQueuePresentKHR failed");
    }
}

} // namespace vr


