/**
 * @file RendererTeardown.h
 * @brief Declarations for the RendererTeardown module.
 */
#pragma once

#include <array>
#include <vector>

#include <vulkan/vulkan.h>

namespace vr {

class RendererTeardown {
public:
    template <std::size_t MaxFramesInFlight>
    struct TeardownContext {
        VkInstance& instance;
        VkDevice& device;
        VkSurfaceKHR& surface;

        VkDescriptorPool& descriptorPool;
        VkDescriptorSetLayout& descriptorSetLayout;
        VkDescriptorPool& lightingDescriptorPool;
        VkDescriptorSetLayout& lightingDescriptorSetLayout;

        std::array<VkBuffer, MaxFramesInFlight>& uniformBuffers;
        std::array<VkDeviceMemory, MaxFramesInFlight>& uniformBuffersMemory;

        VkBuffer& vertexBuffer;
        VkDeviceMemory& vertexBufferMemory;
        VkBuffer& indexBuffer;
        VkDeviceMemory& indexBufferMemory;

        std::array<VkSemaphore, MaxFramesInFlight>& imageAvailableSemaphores;
        std::array<VkFence, MaxFramesInFlight>& inFlightFences;
        std::vector<VkSemaphore>& renderFinishedSemaphores;

        VkCommandPool& commandPool;
    };

    template <std::size_t MaxFramesInFlight>
    void cleanup(TeardownContext<MaxFramesInFlight>& context) const {
        if (context.descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(context.device, context.descriptorPool, nullptr);
            context.descriptorPool = VK_NULL_HANDLE;
        }

        if (context.descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(context.device, context.descriptorSetLayout, nullptr);
            context.descriptorSetLayout = VK_NULL_HANDLE;
        }

        if (context.lightingDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(context.device, context.lightingDescriptorPool, nullptr);
            context.lightingDescriptorPool = VK_NULL_HANDLE;
        }

        if (context.lightingDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(context.device, context.lightingDescriptorSetLayout, nullptr);
            context.lightingDescriptorSetLayout = VK_NULL_HANDLE;
        }

        for (std::size_t i = 0; i < MaxFramesInFlight; ++i) {
            if (context.uniformBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(context.device, context.uniformBuffers[i], nullptr);
                context.uniformBuffers[i] = VK_NULL_HANDLE;
            }
            if (context.uniformBuffersMemory[i] != VK_NULL_HANDLE) {
                vkFreeMemory(context.device, context.uniformBuffersMemory[i], nullptr);
                context.uniformBuffersMemory[i] = VK_NULL_HANDLE;
            }
        }

        if (context.vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(context.device, context.vertexBuffer, nullptr);
            context.vertexBuffer = VK_NULL_HANDLE;
        }

        if (context.vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(context.device, context.vertexBufferMemory, nullptr);
            context.vertexBufferMemory = VK_NULL_HANDLE;
        }

        if (context.indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(context.device, context.indexBuffer, nullptr);
            context.indexBuffer = VK_NULL_HANDLE;
        }

        if (context.indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(context.device, context.indexBufferMemory, nullptr);
            context.indexBufferMemory = VK_NULL_HANDLE;
        }

        for (std::size_t i = 0; i < MaxFramesInFlight; ++i) {
            if (context.imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(context.device, context.imageAvailableSemaphores[i], nullptr);
                context.imageAvailableSemaphores[i] = VK_NULL_HANDLE;
            }
            if (context.inFlightFences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(context.device, context.inFlightFences[i], nullptr);
                context.inFlightFences[i] = VK_NULL_HANDLE;
            }
        }

        for (VkSemaphore semaphore : context.renderFinishedSemaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(context.device, semaphore, nullptr);
            }
        }
        context.renderFinishedSemaphores.clear();

        if (context.commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(context.device, context.commandPool, nullptr);
            context.commandPool = VK_NULL_HANDLE;
        }

        if (context.device != VK_NULL_HANDLE) {
            vkDestroyDevice(context.device, nullptr);
            context.device = VK_NULL_HANDLE;
        }

        if (context.surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
            context.surface = VK_NULL_HANDLE;
        }
    }
};

} // namespace vr


