/**
 * @file PresentFlow.h
 * @brief Declarations for the PresentFlow module.
 */
#pragma once

#include <functional>

#include <vulkan/vulkan.h>

namespace vr {

class PresentFlow {
public:
    bool shouldSkipFrameOnAcquireResult(VkResult acquireResult, const std::function<void()>& recreateSwapchain) const;
    void handlePresentResult(VkResult presentResult, bool& framebufferResized, const std::function<void()>& recreateSwapchain) const;
};

} // namespace vr


