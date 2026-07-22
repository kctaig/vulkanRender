#include "renderer/Renderer.h"

#include <Windowsx.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "renderer/ForwardPass.h"
#include "renderer/PreDepthPass.h"
#include "renderer/GeometryPass.h"
#include "renderer/DeferredLightingPass.h"
#include "renderer/PostProcessPass.h"
#include "renderer/ResourceTable.h"
#include "renderer/AccelerationStructure.h"
#include "renderer/DDGIVolume.h"
#include "renderer/DDGIProbePass.h"

namespace vr {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool Renderer::initialize(unsigned int width, unsigned int height) {
    windowWidth_ = width;
    windowHeight_ = height;

    // --- Window ---
    Window::Desc wd{};
    wd.title = "Vulkan Renderer";
    wd.width = width;
    wd.height = height;
    wd.onMessage = [this](HWND h, UINT m, WPARAM w, LPARAM l) {
        return handleInput(h, m, w, l);
    };
    if (!window_.create(wd)) return false;

    // --- Vulkan core ---
    VulkanContext::CreateInfo ctxInfo{};
    ctxInfo.windowHandle = static_cast<HWND>(window_.nativeHandle());
    ctxInfo.instanceHandle = GetModuleHandle(nullptr);
    ctxInfo.initialWidth = width;
    ctxInfo.initialHeight = height;
    ctxInfo.enableValidation = true;

    if (!ctx_.initialize(ctxInfo)) return false;

    // --- Resource table (shared between passes) ---
    resourceTable_ = std::make_unique<ResourceTable>();
    {
        VkSampler nearestSamp = VK_NULL_HANDLE;
        VkSamplerCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        si.magFilter = VK_FILTER_NEAREST; si.minFilter = VK_FILTER_NEAREST;
        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(ctx_.device(), &si, nullptr, &nearestSamp);
        resourceTable_->setDefaultSampler(nearestSamp);
    }

    // --- Acceleration structures ---
    if (ctx_.rayTracingAvailable()) {
        accelBuilder_ = std::make_unique<AccelerationStructureBuilder>();
        accelBuilder_->buildBLAS(ctx_, scene_);
    }

    // --- Passes ---
    // 1. Z-PrePass
    auto prepass = std::make_unique<PreDepthPass>();
    prepass->setResources(*resourceTable_);
    prepass->setScene(scene_);
    if (!prepass->initialize(ctx_)) {
        std::cerr << "[Renderer] PreDepthPass initialization failed\n";
        return false;
    }
    passes_.push_back(std::move(prepass));

    // 2. Geometry Pass (GBuffer)
    auto geom = std::make_unique<GeometryPass>();
    geom->setResources(*resourceTable_);
    geom->setScene(scene_);
    if (!geom->initialize(ctx_)) {
        std::cerr << "[Renderer] GeometryPass initialization failed\n";
        return false;
    }
    passes_.push_back(std::move(geom));

    // 3. DDGI Probe Pass (compute: ray trace + blend probes)
    if (ctx_.rayTracingAvailable()) {
        auto ddgiVolume = std::make_unique<DDGIVolume>();
        DDGIConfig ddgiCfg;
        ddgiCfg.probeSpacing = scene_.modelRadius * 0.3f;
        ddgiCfg.probeCounts = {16, 6, 16};
        ddgiCfg.raysPerProbe = 64;
        ddgiCfg.hysteresis = 0.98f;
        ddgiCfg.depthSharpness = 50.0f;
        if (ddgiVolume->initialize(ctx_, ddgiCfg, scene_)) {
            auto ddgiProbe = std::make_unique<DDGIProbePass>();
            ddgiProbe->setVolume(ddgiVolume.get());
            ddgiProbe->setTLAS(accelBuilder_->tlas());
            ddgiProbe->setResources(*resourceTable_);
            if (ddgiProbe->initialize(ctx_)) {
                passes_.push_back(std::move(ddgiProbe));
            }

            // Register DDGI outputs to ResourceTable
            resourceTable_->set("ddgi.irradiance", ddgiVolume->irradianceView(),
                                VK_FORMAT_R16G16B16A16_SFLOAT, ddgiVolume->atlasExtent(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            resourceTable_->set("ddgi.depth", ddgiVolume->depthView(),
                                VK_FORMAT_R16G16_SFLOAT, ddgiVolume->atlasExtent(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            resourceTable_->setSampler("ddgi.sampler", ddgiVolume->atlasSampler());

            // Store for shutdown + frame loop
            ddgiVolume_ = std::move(ddgiVolume);
            ddgiEnabled_ = true;
        }
    }

    // 4. Deferred Lighting Pass
    auto lighting = std::make_unique<DeferredLightingPass>();
    lighting->setResources(*resourceTable_);
    lighting->setScene(scene_);
    if (ddgiEnabled_) {
        lighting->setDDGIVolume(ddgiVolume_.get());
        lighting->setDDGIEnabled(true);
    }
    if (!lighting->initialize(ctx_)) {
        std::cerr << "[Renderer] DeferredLightingPass initialization failed\n";
        return false;
    }
    passes_.push_back(std::move(lighting));

    // 4. Post-Process Pass
    auto post = std::make_unique<PostProcessPass>();
    post->setResources(*resourceTable_);
    if (!post->initialize(ctx_)) {
        std::cerr << "[Renderer] PostProcessPass initialization failed\n";
        return false;
    }
    passes_.push_back(std::move(post));

    // --- Command buffers ---
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = ctx_.commandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = RenderPass::kMaxFramesInFlight;

        std::array<VkCommandBuffer, RenderPass::kMaxFramesInFlight> cmdBufs{};
        if (vkAllocateCommandBuffers(ctx_.device(), &allocInfo, cmdBufs.data()) != VK_SUCCESS) {
            std::cerr << "[Renderer] Command buffer allocation failed\n";
            return false;
        }
        commandBuffers_ = cmdBufs;
    }

    // --- Sync objects ---
    {
        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (std::uint32_t i = 0; i < RenderPass::kMaxFramesInFlight; ++i) {
            if (vkCreateSemaphore(
                    ctx_.device(), &semInfo, nullptr, &imageAvailableSemaphores_[i]
                ) != VK_SUCCESS ||
                vkCreateFence(ctx_.device(), &fenceInfo, nullptr, &inFlightFences_[i]) !=
                    VK_SUCCESS) {
                std::cerr << "[Renderer] Sync object creation failed\n";
                return false;
            }
        }

        const std::uint32_t n = ctx_.swapchainImageCount();
        renderFinishedSemaphores_.resize(n, VK_NULL_HANDLE);
        for (std::uint32_t i = 0; i < n; ++i) {
            vkCreateSemaphore(ctx_.device(), &semInfo, nullptr, &renderFinishedSemaphores_[i]);
        }
        imagesInFlight_.assign(n, VK_NULL_HANDLE);
    }

    // Camera projection — set once, updated on resize
    scene_.camera.setPerspective(
        glm::radians(45.0f),
        static_cast<float>(ctx_.swapchainExtent().width) /
            static_cast<float>(ctx_.swapchainExtent().height),
        0.01f, 100000.0f);

    ready_ = true;
    return true;
}

void Renderer::mainLoop() {
    if (!ready_) {
        std::cerr << "[Renderer] Not ready, exiting\n";
        return;
    }
    std::cerr << "[Renderer] Entering main loop\n";
    while (window_.pumpMessages()) {
        try {
            drawFrame();
        } catch (const std::exception& e) {
            std::cerr << "[Renderer] Exception in drawFrame: " << e.what() << "\n";
            break;
        }
    }
    std::cerr << "[Renderer] Main loop ended\n";
    if (ctx_.device() != VK_NULL_HANDLE) vkDeviceWaitIdle(ctx_.device());
}

void Renderer::shutdown() {
    if (ctx_.device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(ctx_.device());
    }

    // Shut down passes
    for (auto& pass : passes_) {
        pass->shutdown();
    }
    passes_.clear();

    // Shut down DDGI
    if (ddgiVolume_) {
        ddgiVolume_->shutdown(ctx_.device());
        ddgiVolume_.reset();
    }

    // Shut down acceleration structures
    if (accelBuilder_) {
        accelBuilder_->shutdown(ctx_);
        accelBuilder_.reset();
    }

    // Destroy sync objects
    for (auto& sem : imageAvailableSemaphores_) {
        if (sem != VK_NULL_HANDLE)
            vkDestroySemaphore(ctx_.device(), sem, nullptr);
    }
    for (auto& sem : renderFinishedSemaphores_) {
        if (sem != VK_NULL_HANDLE)
            vkDestroySemaphore(ctx_.device(), sem, nullptr);
    }
    for (auto& fence : inFlightFences_) {
        if (fence != VK_NULL_HANDLE)
            vkDestroyFence(ctx_.device(), fence, nullptr);
    }

    // Destroy scene assets (meshes, textures) before device
    scene_.shutdown(ctx_.device());

    // VulkanContext cleans up commandPool, device, surface, instance
    ctx_.shutdown();

    window_.close();
    ready_ = false;
}

// ---------------------------------------------------------------------------
// Frame loop
// ---------------------------------------------------------------------------

void Renderer::drawFrame() {
    vkWaitForFences(ctx_.device(), 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    std::uint32_t imageIndex = 0;
    if (!ctx_.acquireNextImage(imageAvailableSemaphores_[currentFrame_], imageIndex)) {
        recreateSwapchain();
        return;
    }

    if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(ctx_.device(), 1, &imagesInFlight_[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight_[imageIndex] = inFlightFences_[currentFrame_];

    vkResetFences(ctx_.device(), 1, &inFlightFences_[currentFrame_]);

    VkCommandBuffer cmd = commandBuffers_[currentFrame_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // WASD camera movement (small delta because keys fire every frame)
    if (keys_['W']) scene_.camera.dolly(0.1f);
    if (keys_['S']) scene_.camera.dolly(-0.1f);
    if (keys_['A']) scene_.camera.pan(-0.1f, 0);
    if (keys_['D']) scene_.camera.pan(0.1f, 0);

    // Rebuild TLAS each frame
    if (accelBuilder_) {
        accelBuilder_->buildTLAS(ctx_, scene_);
    }

    // Scroll DDGI probe grid with camera
    if (ddgiVolume_) {
        ddgiVolume_->scroll(scene_.camera.position());
    }

    // Execute all passes
    for (auto& pass : passes_) {
        pass->record(cmd, currentFrame_, imageIndex);
    }

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSems[] = {imageAvailableSemaphores_[currentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSems;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkSemaphore sigSems[] = {renderFinishedSemaphores_[imageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = sigSems;

    if (vkQueueSubmit(ctx_.graphicsQueue(), 1, &submitInfo, inFlightFences_[currentFrame_]) !=
        VK_SUCCESS) {
        throw std::runtime_error("vkQueueSubmit failed");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = sigSems;
    VkSwapchainKHR swapchains[] = {ctx_.swapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(ctx_.presentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized_) {
        framebufferResized_ = false;
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("vkQueuePresentKHR failed");
    }

    currentFrame_ = (currentFrame_ + 1) % RenderPass::kMaxFramesInFlight;

    // Swap DDGI history after each frame
    if (ddgiVolume_) {
        ddgiVolume_->swapHistory();
    }
}

void Renderer::recreateSwapchain() {
    while (windowWidth_ == 0 || windowHeight_ == 0) {
        if (!window_.pumpMessages()) return;
    }

    ctx_.setFramebufferSize(windowWidth_, windowHeight_);
    ctx_.recreateSwapchain();

    // Notify passes
    for (auto& pass : passes_) {
        pass->onSwapchainResize(ctx_);
    }

    // Update camera projection for new extent
    scene_.camera.setPerspective(
        glm::radians(45.0f),
        static_cast<float>(ctx_.swapchainExtent().width) /
            static_cast<float>(ctx_.swapchainExtent().height),
        0.01f, 100000.0f);

    // Recreate per-image semaphores
    for (auto& sem : renderFinishedSemaphores_) {
        if (sem != VK_NULL_HANDLE)
            vkDestroySemaphore(ctx_.device(), sem, nullptr);
    }
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    renderFinishedSemaphores_.assign(ctx_.swapchainImageCount(), VK_NULL_HANDLE);
    for (auto& sem : renderFinishedSemaphores_) {
        vkCreateSemaphore(ctx_.device(), &semInfo, nullptr, &sem);
    }
    imagesInFlight_.assign(ctx_.swapchainImageCount(), VK_NULL_HANDLE);
}

LRESULT Renderer::handleInput(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            window_.close();
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            windowWidth_ = static_cast<unsigned int>(LOWORD(lParam));
            windowHeight_ = static_cast<unsigned int>(HIWORD(lParam));
            if (wParam != SIZE_MINIMIZED)
                framebufferResized_ = true;
            return 0;
        case WM_RBUTTONDOWN:
            rightDragActive_ = true;
            lastMousePosition_ = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            return 0;
        case WM_RBUTTONUP:
            rightDragActive_ = false;
            return 0;
        case WM_LBUTTONDOWN:
            leftDragActive_ = true;
            lastMousePosition_ = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            return 0;
        case WM_LBUTTONUP:
            leftDragActive_ = false;
            return 0;
        case WM_MOUSEMOVE: {
            POINT cur{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            float dx = static_cast<float>(cur.x - lastMousePosition_.x);
            float dy = static_cast<float>(cur.y - lastMousePosition_.y);
            lastMousePosition_ = cur;

            if (rightDragActive_) {
                scene_.camera.rotate(dx, -dy);
            }
            if (leftDragActive_) {
                scene_.camera.pan(-dx, dy);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            scene_.camera.dolly(static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA));
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam < 256) keys_[wParam] = true;
            return 0;
        case WM_KEYUP:
            if (wParam < 256) keys_[wParam] = false;
            return 0;
        default:
            break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

}  // namespace vr
