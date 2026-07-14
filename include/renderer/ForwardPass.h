#pragma once

#include <array>
#include <cstdint>
#include <vector>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include "renderer/RenderPass.h"

namespace vr {

class ForwardPass : public RenderPass {
  public:
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;

        static VkVertexInputBindingDescription getBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 3>
        getAttributeDescriptions();
    };

    struct Uniforms {
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
    };

    bool initialize(VulkanContext& ctx) override;
    void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                std::uint32_t imageIndex) override;
    void onSwapchainResize(VulkanContext& ctx) override;
    void shutdown() override;

  private:
    void createDescriptorSetLayout(VulkanContext& ctx);
    void createGraphicsPipeline(VulkanContext& ctx);
    void createDescriptorSets(VulkanContext& ctx);
    void updateUniformBuffer(VulkanContext& ctx, std::uint32_t frameIndex,
                             const glm::mat4& model);

    UniqueDescriptorSetLayout descriptorSetLayout_;
    UniquePipelineLayout pipelineLayout_;
    UniquePipeline pipeline_;

    UniqueImage depthImage_;

    std::array<UniqueBuffer, kMaxFramesInFlight> uniformBuffers_{};
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, kMaxFramesInFlight> descriptorSets_{};

    UniqueImage textureImage_;
    UniqueSampler textureSampler_;
};

}  // namespace vr
