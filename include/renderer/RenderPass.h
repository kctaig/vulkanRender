#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

namespace vr {

class VulkanContext;

class RenderPass {
  public:
    static constexpr std::uint32_t kMaxFramesInFlight = 2;

    virtual ~RenderPass() = default;

    virtual bool initialize(VulkanContext& ctx) = 0;
    virtual void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                        std::uint32_t imageIndex) = 0;
    virtual void onSwapchainResize(VulkanContext& ctx) = 0;
    virtual void shutdown() = 0;

  protected:
    VulkanContext* ctx_ = nullptr;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
};

}  // namespace vr
