/**
 * @file RendererInitPipeline.cpp
 * @brief Implementation for the RendererInitPipeline module.
 */
#include "renderer/lifecycle/RendererInitPipeline.h"

namespace vr {

void RendererInitPipeline::execute(Context& context) const {
    context.createInstance();
    context.setupDebugMessenger();
    context.createSurface();
    context.pickPhysicalDevice();
    context.createLogicalDevice();
    context.createCommandPool();
    context.createSwapchain();
    context.createImageViews();
    context.createRenderPass();
    context.createDescriptorSetLayout();
    context.createLightingDescriptorSetLayout();
    context.createGraphicsPipeline();
    context.createLightingPipeline();
    context.createDepthResources();
    context.createGBufferResources();
    context.createFramebuffers();
    context.createVertexBuffer();
    context.createIndexBuffer();
    context.createUniformBuffers();
    context.createDescriptorPool();
    context.createDescriptorSets();
    context.createLightingDescriptorPool();
    context.createLightingDescriptorSet();
    context.updateLightingDescriptorSet();
    context.createCommandBuffers();
    context.createSyncObjects();
    context.createImGuiDescriptorPool();
    context.initImGui();
}

} // namespace vr


