#pragma once

#include <array>
#include <cstdint>

#include "renderer/RenderPass.h"

namespace vr {

/// Full-screen tonemap + gamma pass. Outputs to swapchain.
class PostProcessPass : public RenderPass {
  public:
    bool initialize(VulkanContext& ctx) override;
    void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                std::uint32_t imageIndex) override;
    void onSwapchainResize(VulkanContext& ctx) override;
    void shutdown() override;

  private:
    void createDescriptorSetLayout(VulkanContext& ctx);
    void createPipeline(VulkanContext& ctx);
    void createDescriptorSets(VulkanContext& ctx);

    UniqueDescriptorSetLayout descSetLayout_;
    UniquePipelineLayout pipelineLayout_;
    UniquePipeline pipeline_;

    UniqueSampler hdrSampler_;

    std::array<UniqueBuffer, kMaxFramesInFlight> postUBOs_{};
    VkDescriptorPool descPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, kMaxFramesInFlight> descSets_{};

    struct PostUBO {
        float exposure = 1.0f;
        float gamma = 2.2f;
        float pad[2];
    };
};

}  // namespace vr
