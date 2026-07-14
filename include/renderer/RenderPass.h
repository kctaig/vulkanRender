#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

#include "core/VulkanResource.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/PipelineBuilder.h"

namespace vr {

class VulkanContext;
class Scene;

class RenderPass {
  public:
    static constexpr std::uint32_t kMaxFramesInFlight = 2;

    virtual ~RenderPass() = default;

    virtual bool initialize(VulkanContext& ctx) = 0;
    virtual void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                        std::uint32_t imageIndex) = 0;
    virtual void onSwapchainResize(VulkanContext& ctx) = 0;
    virtual void shutdown() = 0;

    void setScene(Scene& s) { scene_ = &s; }

  protected:
    // ================================================================
    // Shared helpers — call from subclass initialize()
    // ================================================================

    /// Create a VkRenderPass from building blocks.
    void createRenderPass(VulkanContext& ctx,
                          const std::vector<VkAttachmentDescription>& attachments,
                          const std::vector<VkSubpassDescription>& subpasses,
                          const std::vector<VkSubpassDependency>& deps);

    /// Create depth image + view.
    void createDepth(VulkanContext& ctx, UniqueImage& outDepth);

    /// Create one framebuffer per swapchain image.
    void createFramebuffers(VulkanContext& ctx, VkImageView depthView);

    /// Create a descriptor pool from pool-size specs.
    void createDescriptorPool(VulkanContext& ctx,
                               const std::vector<VkDescriptorPoolSize>& sizes,
                               VkDescriptorPool& outPool);

    /// Allocate kMaxFramesInFlight descriptor sets from pool.
    void allocateDescriptorSets(VkDescriptorSetLayout layout,
                                 VkDescriptorPool pool,
                                 std::array<VkDescriptorSet, kMaxFramesInFlight>& outSets);

    /// Create kMaxFramesInFlight uniform buffers (host-visible).
    template <typename UBO>
    void createUniformBuffers(VulkanContext& ctx,
                               std::array<UniqueBuffer, kMaxFramesInFlight>& out) {
        for (auto& ub : out) {
            VkBuffer b;
            VkDeviceMemory m;
            ctx.createBuffer(sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             b, m);
            ub = UniqueBuffer(ctx.device(), b, m);
        }
    }

    /// Create a 256x256 checkerboard texture + sampler (fallback for any pass).
    void createDefaultTexture(VulkanContext& ctx, UniqueImage& outImage,
                               UniqueSampler& outSampler);

    VulkanContext* ctx_ = nullptr;
    Scene* scene_ = nullptr;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
};

}  // namespace vr
