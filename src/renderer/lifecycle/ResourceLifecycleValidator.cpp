/**
 * @file ResourceLifecycleValidator.cpp
 * @brief Implementation for the ResourceLifecycleValidator module.
 */
#include "renderer/lifecycle/ResourceLifecycleValidator.h"

#include <iostream>
#include <stdexcept>

namespace vr {

void ResourceLifecycleValidator::validateSyncCollections(
    std::size_t swapchainImageCount,
    const std::vector<VkSemaphore>& renderFinishedSemaphores,
    const std::vector<VkFence>& imagesInFlight
) const {
    if (renderFinishedSemaphores.size() != swapchainImageCount) {
        throw std::runtime_error("sync state mismatch: renderFinishedSemaphores size != swapchain image count");
    }
    if (imagesInFlight.size() != swapchainImageCount) {
        throw std::runtime_error("sync state mismatch: imagesInFlight size != swapchain image count");
    }
}

void ResourceLifecycleValidator::validateShutdownCleared(const ShutdownContext& context) const {
    const bool hasLeak =
        context.instance != VK_NULL_HANDLE ||
        context.device != VK_NULL_HANDLE ||
        context.surface != VK_NULL_HANDLE ||
        context.windowHandle != nullptr ||
        context.descriptorPool != VK_NULL_HANDLE ||
        context.descriptorSetLayout != VK_NULL_HANDLE ||
        context.lightingDescriptorPool != VK_NULL_HANDLE ||
        context.lightingDescriptorSetLayout != VK_NULL_HANDLE ||
        context.vertexBuffer != VK_NULL_HANDLE ||
        context.vertexBufferMemory != VK_NULL_HANDLE ||
        context.indexBuffer != VK_NULL_HANDLE ||
        context.indexBufferMemory != VK_NULL_HANDLE ||
        context.commandPool != VK_NULL_HANDLE ||
        (context.renderFinishedSemaphores != nullptr && !context.renderFinishedSemaphores->empty());

    if (hasLeak) {
        std::cerr << "[LifecycleValidator] Warning: shutdown finished with uncleared resources\n";
    }
}

} // namespace vr


