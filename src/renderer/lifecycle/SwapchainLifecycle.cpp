/**
 * @file SwapchainLifecycle.cpp
 * @brief Implementation for the SwapchainLifecycle module.
 */
#include "renderer/lifecycle/SwapchainLifecycle.h"

#include <stdexcept>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

namespace vr {

void SwapchainLifecycle::removeSceneTextureDescriptor(void*& sceneTextureId) const {
    if (sceneTextureId != nullptr && ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(sceneTextureId));
        sceneTextureId = nullptr;
    }
}

void SwapchainLifecycle::addSceneTextureDescriptor(void*& sceneTextureId, VkSampler sampler, VkImageView imageView) const {
    sceneTextureId = ImGui_ImplVulkan_AddTexture(
        sampler,
        imageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

void SwapchainLifecycle::cleanupResources(CleanupContext& context) const {
    removeSceneTextureDescriptor(context.sceneTextureId);

    if (context.sceneFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(context.device, context.sceneFramebuffer, nullptr);
        context.sceneFramebuffer = VK_NULL_HANDLE;
    }

    for (VkFramebuffer framebuffer : context.swapchainFramebuffers) {
        vkDestroyFramebuffer(context.device, framebuffer, nullptr);
    }
    context.swapchainFramebuffers.clear();

    if (context.depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, context.depthImageView, nullptr);
        context.depthImageView = VK_NULL_HANDLE;
    }
    if (context.depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(context.device, context.depthImage, nullptr);
        context.depthImage = VK_NULL_HANDLE;
    }
    if (context.depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, context.depthImageMemory, nullptr);
        context.depthImageMemory = VK_NULL_HANDLE;
    }

    if (context.gbufferPositionImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, context.gbufferPositionImageView, nullptr);
        context.gbufferPositionImageView = VK_NULL_HANDLE;
    }
    if (context.gbufferPositionImage != VK_NULL_HANDLE) {
        vkDestroyImage(context.device, context.gbufferPositionImage, nullptr);
        context.gbufferPositionImage = VK_NULL_HANDLE;
    }
    if (context.gbufferPositionImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, context.gbufferPositionImageMemory, nullptr);
        context.gbufferPositionImageMemory = VK_NULL_HANDLE;
    }

    if (context.gbufferNormalImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, context.gbufferNormalImageView, nullptr);
        context.gbufferNormalImageView = VK_NULL_HANDLE;
    }
    if (context.gbufferNormalImage != VK_NULL_HANDLE) {
        vkDestroyImage(context.device, context.gbufferNormalImage, nullptr);
        context.gbufferNormalImage = VK_NULL_HANDLE;
    }
    if (context.gbufferNormalImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, context.gbufferNormalImageMemory, nullptr);
        context.gbufferNormalImageMemory = VK_NULL_HANDLE;
    }

    if (context.gbufferAlbedoImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, context.gbufferAlbedoImageView, nullptr);
        context.gbufferAlbedoImageView = VK_NULL_HANDLE;
    }
    if (context.gbufferAlbedoImage != VK_NULL_HANDLE) {
        vkDestroyImage(context.device, context.gbufferAlbedoImage, nullptr);
        context.gbufferAlbedoImage = VK_NULL_HANDLE;
    }
    if (context.gbufferAlbedoImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, context.gbufferAlbedoImageMemory, nullptr);
        context.gbufferAlbedoImageMemory = VK_NULL_HANDLE;
    }

    if (context.sceneColorSampler != VK_NULL_HANDLE) {
        vkDestroySampler(context.device, context.sceneColorSampler, nullptr);
        context.sceneColorSampler = VK_NULL_HANDLE;
    }
    if (context.sceneColorImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(context.device, context.sceneColorImageView, nullptr);
        context.sceneColorImageView = VK_NULL_HANDLE;
    }
    if (context.sceneColorImage != VK_NULL_HANDLE) {
        vkDestroyImage(context.device, context.sceneColorImage, nullptr);
        context.sceneColorImage = VK_NULL_HANDLE;
    }
    if (context.sceneColorImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, context.sceneColorImageMemory, nullptr);
        context.sceneColorImageMemory = VK_NULL_HANDLE;
    }

    if (context.graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, context.graphicsPipeline, nullptr);
        context.graphicsPipeline = VK_NULL_HANDLE;
    }

    if (context.lightingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, context.lightingPipeline, nullptr);
        context.lightingPipeline = VK_NULL_HANDLE;
    }

    if (context.pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.device, context.pipelineLayout, nullptr);
        context.pipelineLayout = VK_NULL_HANDLE;
    }

    if (context.lightingPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.device, context.lightingPipelineLayout, nullptr);
        context.lightingPipelineLayout = VK_NULL_HANDLE;
    }

    if (context.renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context.device, context.renderPass, nullptr);
        context.renderPass = VK_NULL_HANDLE;
    }

    if (context.uiRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context.device, context.uiRenderPass, nullptr);
        context.uiRenderPass = VK_NULL_HANDLE;
    }

    for (VkImageView imageView : context.swapchainImageViews) {
        vkDestroyImageView(context.device, imageView, nullptr);
    }
    context.swapchainImageViews.clear();

    if (context.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context.device, context.swapchain, nullptr);
        context.swapchain = VK_NULL_HANDLE;
    }
}

void SwapchainLifecycle::recreateRenderFinishedSemaphores(SyncObjectsContext& context) const {
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


