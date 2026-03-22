/**
 * @file SwapchainRebuilder.cpp
 * @brief Implementation for the SwapchainRebuilder module.
 */
#include "renderer/lifecycle/SwapchainRebuilder.h"

namespace vr {

bool SwapchainRebuilder::rebuild(RebuildContext& context) const {
    while (context.windowWidth == 0 || context.windowHeight == 0) {
        if (!context.processWindowMessages()) {
            return false;
        }
    }

    vkDeviceWaitIdle(context.device);

    context.cleanupSwapchain();
    context.createSwapchain();
    context.createImageViews();
    context.createRenderPass();
    context.createGraphicsPipeline();
    context.createLightingPipeline();
    context.createDepthResources();
    context.createGBufferResources();
    context.createFramebuffers();
    context.updateLightingDescriptorSet();
    context.refreshSceneTextureDescriptor();
    context.updateImGuiMinImageCount();
    context.appendSwapchainRecreatedLog();
    context.recreateRenderFinishedSemaphores();
    context.resetImagesInFlight();

    return true;
}

} // namespace vr


