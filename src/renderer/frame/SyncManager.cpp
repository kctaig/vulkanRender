/**
 * @file SyncManager.cpp
 * @brief Implementation for the SyncManager module.
 */
#include "renderer/frame/SyncManager.h"

#include <stdexcept>

namespace vr {

void SyncManager::recreateRenderFinishedSemaphores(RecreateRenderFinishedContext context) const {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (VkSemaphore semaphore : context.renderFinishedSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(context.device, semaphore, nullptr);
        }
    }

    context.renderFinishedSemaphores.assign(context.swapchainImageCount, VK_NULL_HANDLE);
    for (std::size_t i = 0; i < context.renderFinishedSemaphores.size(); ++i) {
        if (vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &context.renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to recreate render-finished semaphore");
        }
    }
}

} // namespace vr


