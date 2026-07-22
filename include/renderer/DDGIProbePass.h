#pragma once

#include <array>
#include <cstdint>

#include "renderer/RenderPass.h"

namespace vr {

class DDGIVolume;
class AccelerationStructureBuilder;

/// Compute-only pass: ray-traces DDGI probes and blends results with history.
/// Does NOT use a VkRenderPass — dispatch compute directly.
class DDGIProbePass : public RenderPass {
  public:
    bool initialize(VulkanContext& ctx) override;
    void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                std::uint32_t imageIndex) override;
    void onSwapchainResize(VulkanContext& ctx) override;
    void shutdown() override;

    void setVolume(DDGIVolume* vol) { volume_ = vol; }
    void setTLAS(VkAccelerationStructureKHR tlas) { tlas_ = tlas; }

  private:
    void createTracePipeline(VulkanContext& ctx);
    void createBlendPipeline(VulkanContext& ctx);

    // Trace pipeline
    UniquePipeline tracePipeline_;
    UniquePipelineLayout tracePipelineLayout_;
    UniqueDescriptorSetLayout traceDescSetLayout_;
    VkDescriptorPool traceDescPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, kMaxFramesInFlight> traceDescSets_{};

    // Blend pipeline
    UniquePipeline blendPipeline_;
    UniquePipelineLayout blendPipelineLayout_;
    UniqueDescriptorSetLayout blendDescSetLayout_;
    VkDescriptorPool blendDescPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, kMaxFramesInFlight> blendDescSets_{};

    DDGIVolume* volume_ = nullptr;
    VkAccelerationStructureKHR tlas_ = VK_NULL_HANDLE;
};

}  // namespace vr
