#include "renderer/Renderer.h"

#include <Windowsx.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "renderer/ForwardPass.h"

namespace vr {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool Renderer::initialize(unsigned int width, unsigned int height) {
    windowWidth_ = width;
    windowHeight_ = height;

    if (!initWindow(width, height)) {
        return false;
    }

    // --- Vulkan core ---
    VulkanContext::CreateInfo ctxInfo{};
    ctxInfo.windowHandle = windowHandle_;
    ctxInfo.instanceHandle = instanceHandle_;
    ctxInfo.initialWidth = width;
    ctxInfo.initialHeight = height;
    ctxInfo.enableValidation = true;

    if (!ctx_.initialize(ctxInfo)) {
        return false;
    }

    // --- Passes ---
    auto fp = std::make_unique<ForwardPass>();
    if (!fp->initialize(ctx_)) {
        std::cerr << "[Renderer] ForwardPass initialization failed\n";
        return false;
    }
    fp->setScene(scene_);
    passes_.push_back(std::move(fp));

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

    ready_ = true;
    return true;
}

void Renderer::mainLoop() {
    if (!ready_)
        return;

    while (windowRunning_) {
        if (!processWindowMessages())
            break;
        drawFrame();
    }

    if (ctx_.device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(ctx_.device());
    }
}

void Renderer::shutdown() {
    std::cout << "[Renderer] Shutdown begin" << std::endl;
    if (ctx_.device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(ctx_.device());
    }

    // Shut down passes
    for (auto& pass : passes_) {
        pass->shutdown();
    }
    passes_.clear();

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

    if (windowHandle_ != nullptr) {
        DestroyWindow(windowHandle_);
        windowHandle_ = nullptr;
    }

    ready_ = false;
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

bool Renderer::initWindow(unsigned int width, unsigned int height) {
    instanceHandle_ = GetModuleHandle(nullptr);

    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Renderer::WindowProc;
    wc.hInstance = instanceHandle_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "VulkanRendererWindowClass";

    if (RegisterClassEx(&wc) == 0) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS)
            return false;
    }

    RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    windowHandle_ = CreateWindowExA(
        0, wc.lpszClassName, "Vulkan Renderer", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instanceHandle_, this
    );

    if (windowHandle_ == nullptr) {
        std::cerr << "[Renderer] CreateWindowExA failed: " << GetLastError() << "\n";
        return false;
    }

    ShowWindow(windowHandle_, SW_SHOWDEFAULT);
    UpdateWindow(windowHandle_);
    return true;
}

bool Renderer::processWindowMessages() {
    MSG msg{};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            windowRunning_ = false;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return windowRunning_;
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
}

void Renderer::recreateSwapchain() {
    while (windowWidth_ == 0 || windowHeight_ == 0) {
        if (!processWindowMessages())
            return;
    }

    ctx_.setFramebufferSize(windowWidth_, windowHeight_);
    ctx_.recreateSwapchain();

    // Notify passes
    for (auto& pass : passes_) {
        pass->onSwapchainResize(ctx_);
    }

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

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

LRESULT CALLBACK Renderer::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Renderer* r = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        r = static_cast<Renderer*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(r));
    } else {
        r = reinterpret_cast<Renderer*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }
    if (r != nullptr)
        return r->handleWindowMessage(hWnd, msg, wParam, lParam);
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT Renderer::handleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            windowRunning_ = false;
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
                scene_.camera.rotate(dx * 0.01f, -dy * 0.01f);
            }
            if (leftDragActive_) {
                scene_.camera.pan(glm::vec2(-dx * 0.005f, dy * 0.005f));
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            scene_.camera.zoom(static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA) * 0.2f);
            return 0;
        }
        default:
            break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

}  // namespace vr
