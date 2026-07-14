#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace vr {

class VulkanContext;

class RenderPass {
  public:
    virtual ~RenderPass() = default;

    virtual bool initialize(VulkanContext& ctx) = 0;
    virtual void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                        std::uint32_t imageIndex) = 0;
    virtual void onSwapchainResize(VulkanContext& ctx) = 0;
    virtual void shutdown() = 0;
};

}  // namespace vr
