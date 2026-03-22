/**
 * @file ImGuiBackend.cpp
 * @brief Implementation for the ImGuiBackend module.
 */
#include "renderer/ui/ImGuiBackend.h"

#include <array>
#include <stdexcept>
#include <utility>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_win32.h>

namespace vr {

namespace {

int imguiWin32CreateVkSurface(ImGuiViewport* viewport, ImU64 vkInstance, const void* vkAllocators, ImU64* outVkSurface) {
    if (viewport == nullptr || outVkSurface == nullptr || viewport->PlatformHandleRaw == nullptr) {
        return static_cast<int>(VK_ERROR_INITIALIZATION_FAILED);
    }

    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandle(nullptr);
    createInfo.hwnd = static_cast<HWND>(viewport->PlatformHandleRaw);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const VkResult result = vkCreateWin32SurfaceKHR(
        reinterpret_cast<VkInstance>(static_cast<uintptr_t>(vkInstance)),
        &createInfo,
        static_cast<const VkAllocationCallbacks*>(vkAllocators),
        &surface
    );

    *outVkSurface = static_cast<ImU64>(reinterpret_cast<uintptr_t>(surface));
    return static_cast<int>(result);
}

} // namespace

void ImGuiBackend::createDescriptorPool(VkDevice device, VkDescriptorPool& descriptorPool) const {
    std::array<VkDescriptorPoolSize, 1> poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1024;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorPool for imgui failed");
    }
}

void ImGuiBackend::init(InitContext& context) const {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

    ImGui_ImplWin32_Init(context.windowHandle);
    ImGui::GetPlatformIO().Platform_CreateVkSurface = imguiWin32CreateVkSurface;

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = context.instance;
    initInfo.PhysicalDevice = context.physicalDevice;
    initInfo.Device = context.device;
    initInfo.QueueFamily = context.queueFamily;
    initInfo.Queue = context.queue;
    initInfo.DescriptorPool = context.descriptorPool;
    initInfo.ApiVersion = VK_API_VERSION_1_2;
    initInfo.MinImageCount = context.minImageCount;
    initInfo.ImageCount = context.imageCount;
    initInfo.PipelineInfoMain.RenderPass = context.uiRenderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);

    context.sceneTextureId = ImGui_ImplVulkan_AddTexture(
        context.sceneColorSampler,
        context.sceneColorImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    if (context.appendOutput) {
        context.appendOutput("ImGui initialized");
        context.appendOutput("Docking enabled");
    }
}

void ImGuiBackend::beginFrame() const {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiBackend::endFrame() const {
    ImGui::Render();
}

void ImGuiBackend::shutdown(VkDevice device, VkDescriptorPool& descriptorPool, void*& sceneTextureId) const {
    if (ImGui::GetCurrentContext() != nullptr) {
        if (sceneTextureId != nullptr) {
            ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(sceneTextureId));
            sceneTextureId = nullptr;
        }
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
}

} // namespace vr


