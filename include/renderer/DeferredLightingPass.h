#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

#include "renderer/RenderPass.h"

namespace vr {

class DDGIVolume;

/// Full-screen pass: reads GBuffer + DDGI probes, outputs HDR lighting.
class DeferredLightingPass : public RenderPass {
  public:
    bool initialize(VulkanContext& ctx) override;
    void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                std::uint32_t imageIndex) override;
    void onSwapchainResize(VulkanContext& ctx) override;
    void shutdown() override;

    void setDDGIEnabled(bool enabled) { ddgiEnabled_ = enabled; }
    void setDDGIVolume(DDGIVolume* vol) { ddgiVolume_ = vol; }

  private:
    void createDescriptorSetLayout(VulkanContext& ctx);
    void createPipeline(VulkanContext& ctx);
    void createDescriptorSets(VulkanContext& ctx);

    UniqueDescriptorSetLayout descSetLayout_;
    UniquePipelineLayout pipelineLayout_;
    UniquePipeline pipeline_;

    UniqueImage hdrTarget_;
    UniqueSampler hdrSampler_;
    UniqueImage dummyImage_;       // 1x1 placeholder for unused bindings
    UniqueBuffer dummyDDGIUBO_;    // placeholder DDGI config

    std::array<UniqueBuffer, kMaxFramesInFlight> lightingUBOs_{};
    VkDescriptorPool descPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, kMaxFramesInFlight> descSets_{};

    DDGIVolume* ddgiVolume_ = nullptr;
    bool ddgiEnabled_ = false;

    struct LightingUBO {
        glm::mat4 invViewProj;
        glm::vec4 camPos;           // .w unused
        glm::vec4 lightDir;         // .xyz direction, .w intensity
        glm::vec4 lightColor;
    };
};

}  // namespace vr
