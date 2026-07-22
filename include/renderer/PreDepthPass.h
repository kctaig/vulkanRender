#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

#include "renderer/RenderPass.h"

namespace vr {

/// Z-only pre-pass. Writes depth for early-Z culling and world-pos reconstruction.
class PreDepthPass : public RenderPass {
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
    UniqueImage depthImage_;

    std::array<UniqueBuffer, kMaxFramesInFlight> globalUBOs_{};
    VkDescriptorPool descPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, kMaxFramesInFlight> descSets_{};

    struct GlobalUBO {
        glm::mat4 view;
        glm::mat4 proj;
    };
};

}  // namespace vr
