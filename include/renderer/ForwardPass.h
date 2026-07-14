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

class Scene;

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

    struct UniformBufferObject {
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
    };

    // --- RenderPass interface ---
    bool initialize(VulkanContext& ctx) override;
    void record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                std::uint32_t imageIndex) override;
    void onSwapchainResize(VulkanContext& ctx) override;
    void shutdown() override;

    // --- Scene ---
    void setScene(Scene& scene) { scene_ = &scene; }

  private:
    void createRenderPass(VulkanContext& ctx);
    void createDescriptorSetLayout(VulkanContext& ctx);
    void createGraphicsPipeline(VulkanContext& ctx);
    void createDepthResources(VulkanContext& ctx);
    void createFramebuffers(VulkanContext& ctx);
    void createDefaultTexture(VulkanContext& ctx);
    void createUniformBuffers(VulkanContext& ctx);
    void createDescriptorPool(VulkanContext& ctx);
    void createDescriptorSets(VulkanContext& ctx);
    void updateUniformBuffer(VulkanContext& ctx, std::uint32_t frameIndex,
                             const glm::mat4& model);

    Scene* scene_ = nullptr;

    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;

    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;

    std::array<VkBuffer, kMaxFramesInFlight> uniformBuffers_{};
    std::array<VkDeviceMemory, kMaxFramesInFlight> uniformBuffersMemory_{};
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, kMaxFramesInFlight> descriptorSets_{};

    VkImage textureImage_ = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory_ = VK_NULL_HANDLE;
    VkImageView textureImageView_ = VK_NULL_HANDLE;
    VkSampler textureSampler_ = VK_NULL_HANDLE;
};

}  // namespace vr
