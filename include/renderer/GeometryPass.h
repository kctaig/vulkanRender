#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

#include "renderer/RenderPass.h"

namespace vr {

/// MRT GBuffer generation pass. Outputs albedo, world normal+roughness, material.
class GeometryPass : public RenderPass {
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
    void createGBufferImages(VulkanContext& ctx);

    UniqueDescriptorSetLayout descSetLayout_;
    UniquePipelineLayout pipelineLayout_;
    UniquePipeline pipeline_;

    UniqueImage gbufferAlbedo_;          // RGBA8_SRGB
    UniqueImage gbufferNormalRoughness_; // RGBA16F
    UniqueImage gbufferMaterial_;        // RGBA8
    UniqueSampler gbufferSampler_;

    std::array<UniqueBuffer, kMaxFramesInFlight> globalUBOs_{};
    VkDescriptorPool descPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, kMaxFramesInFlight> descSets_{};

    struct GlobalUBO {
        glm::mat4 view;
        glm::mat4 proj;
    };
};

}  // namespace vr
