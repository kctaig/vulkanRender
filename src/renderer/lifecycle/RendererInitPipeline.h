/**
 * @file RendererInitPipeline.h
 * @brief Declarations for the RendererInitPipeline module.
 */
#pragma once

#include <functional>

namespace vr {

class RendererInitPipeline {
public:
    struct Context {
        std::function<void()> createInstance;
        std::function<void()> setupDebugMessenger;
        std::function<void()> createSurface;
        std::function<void()> pickPhysicalDevice;
        std::function<void()> createLogicalDevice;
        std::function<void()> createCommandPool;
        std::function<void()> createSwapchain;
        std::function<void()> createImageViews;
        std::function<void()> createRenderPass;
        std::function<void()> createDescriptorSetLayout;
        std::function<void()> createLightingDescriptorSetLayout;
        std::function<void()> createGraphicsPipeline;
        std::function<void()> createLightingPipeline;
        std::function<void()> createDepthResources;
        std::function<void()> createGBufferResources;
        std::function<void()> createFramebuffers;
        std::function<void()> createVertexBuffer;
        std::function<void()> createIndexBuffer;
        std::function<void()> createUniformBuffers;
        std::function<void()> createDescriptorPool;
        std::function<void()> createDescriptorSets;
        std::function<void()> createLightingDescriptorPool;
        std::function<void()> createLightingDescriptorSet;
        std::function<void()> updateLightingDescriptorSet;
        std::function<void()> createCommandBuffers;
        std::function<void()> createSyncObjects;
        std::function<void()> createImGuiDescriptorPool;
        std::function<void()> initImGui;
    };

    void execute(Context& context) const;
};

} // namespace vr


