/**
 * @file SyncManager.h
 * @brief Declarations for the SyncManager module.
 */
#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.h>

namespace vr {

class SyncManager {
public:
    template <std::size_t MaxFramesInFlight>
    struct CreateContext {
        VkDevice device = VK_NULL_HANDLE;
        std::array<VkSemaphore, MaxFramesInFlight>& imageAvailableSemaphores;
        std::array<VkFence, MaxFramesInFlight>& inFlightFences;
        std::vector<VkSemaphore>& renderFinishedSemaphores;
        std::vector<VkFence>& imagesInFlight;
        std::size_t swapchainImageCount = 0;
    };

    struct RecreateRenderFinishedContext {
        VkDevice device = VK_NULL_HANDLE;
        std::vector<VkSemaphore>& renderFinishedSemaphores;
        std::size_t swapchainImageCount = 0;
    };

    template <std::size_t MaxFramesInFlight>
    void createSyncObjects(CreateContext<MaxFramesInFlight>& context) const {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (std::size_t i = 0; i < MaxFramesInFlight; ++i) {
            if (vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &context.imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(context.device, &fenceInfo, nullptr, &context.inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects");
            }
        }

        recreateRenderFinishedSemaphores(RecreateRenderFinishedContext{
            .device = context.device,
            .renderFinishedSemaphores = context.renderFinishedSemaphores,
            .swapchainImageCount = context.swapchainImageCount,
        });

        context.imagesInFlight.assign(context.swapchainImageCount, VK_NULL_HANDLE);
    }

    void recreateRenderFinishedSemaphores(RecreateRenderFinishedContext context) const;
};

} // namespace vr


