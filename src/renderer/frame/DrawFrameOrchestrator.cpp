/**
 * @file DrawFrameOrchestrator.cpp
 * @brief Implementation for the DrawFrameOrchestrator module.
 */
#include "renderer/frame/DrawFrameOrchestrator.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace vr {

void DrawFrameOrchestrator::execute(Context& context) const {
    vkWaitForFences(context.device, 1, &context.inFlightFences[context.currentFrame], VK_TRUE, UINT64_MAX);

    std::uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        context.device,
        context.swapchain,
        UINT64_MAX,
        context.imageAvailableSemaphores[context.currentFrame],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (presentFlow_.shouldSkipFrameOnAcquireResult(acquireResult, context.recreateSwapchain)) {
        return;
    }

    if ((*context.imagesInFlight)[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(context.device, 1, &(*context.imagesInFlight)[imageIndex], VK_TRUE, UINT64_MAX);
    }
    (*context.imagesInFlight)[imageIndex] = context.inFlightFences[context.currentFrame];

    vkResetFences(context.device, 1, &context.inFlightFences[context.currentFrame]);
    vkResetCommandBuffer(context.commandBuffers[context.currentFrame], 0);

    static const auto startTime = std::chrono::high_resolution_clock::now();
    const auto now = std::chrono::high_resolution_clock::now();
    float timeSeconds = std::chrono::duration<float, std::chrono::seconds::period>(now - startTime).count();
    static float lastTimeSeconds = timeSeconds;
    const float deltaSeconds = std::max(0.0f, timeSeconds - lastTimeSeconds);
    lastTimeSeconds = timeSeconds;
    context.perfReportAccumulatorSeconds += deltaSeconds;

    context.pollAsyncModelImport();
    context.buildGui();
    if (context.perfReportAccumulatorSeconds >= 2.0f) {
        context.perfReportAccumulatorSeconds = 0.0f;
        context.appendOutput(
            "Stage3 Perf baseline: " + std::to_string(context.smoothedFrameTimeMs) +
            " ms / " + std::to_string(context.smoothedFps) + " FPS"
        );
    }

    context.updateUniformBuffer(context.currentFrame, timeSeconds);
    context.recordCommandBuffer(context.commandBuffers[context.currentFrame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {context.imageAvailableSemaphores[context.currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &context.commandBuffers[context.currentFrame];

    VkSemaphore signalSemaphores[] = {(*context.renderFinishedSemaphores)[imageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, context.inFlightFences[context.currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("vkQueueSubmit failed");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {context.swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(context.presentQueue, &presentInfo);
    presentFlow_.handlePresentResult(presentResult, context.framebufferResized, context.recreateSwapchain);

    context.currentFrame = (context.currentFrame + 1) % context.maxFramesInFlight;
}

} // namespace vr


