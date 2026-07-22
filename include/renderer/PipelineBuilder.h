#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/VulkanContext.h"

namespace vr {

/// Chainable graphics pipeline builder — eliminates ~80 lines of
/// VkPipelineXxxCreateInfo boilerplate per Pass.
///
/// Usage:
///   auto pipe = PipelineBuilder(ctx)
///       .vertexShader("my.vert.spv")
///       .fragmentShader("my.frag.spv")
///       .vertexInput<MyVertex>()
///       .depthTest(true, VK_COMPARE_OP_LESS)
///       .cullMode(VK_CULL_MODE_BACK_BIT)
///       .colorAttachment(ctx.swapchainFormat())
///       .descriptorSetLayout(layout)
///       .build(renderPass);
class PipelineBuilder {
  public:
    explicit PipelineBuilder(VulkanContext& ctx) : ctx_(&ctx) {}

    // --- Shaders ---
    PipelineBuilder& vertexShader(const std::string& spvPath);
    PipelineBuilder& fragmentShader(const std::string& spvPath);
    PipelineBuilder& computeShader(const std::string& spvPath);

    // --- Vertex input (template) ---
    template <typename VertexType>
    PipelineBuilder& vertexInput() {
        bindingDesc_ = VertexType::getBindingDescription();
        auto attrs = VertexType::getAttributeDescriptions();
        attrDescs_.assign(attrs.begin(), attrs.end());
        return *this;
    }

    // --- Depth ---
    PipelineBuilder& depthTest(bool enable,
                                VkCompareOp op = VK_COMPARE_OP_LESS);
    PipelineBuilder& depthWrite(bool enable);

    // --- Rasterization ---
    PipelineBuilder& cullMode(VkCullModeFlags mode);
    PipelineBuilder& polygonMode(VkPolygonMode mode);
    PipelineBuilder& frontFace(VkFrontFace face);

    // --- Color ---
    PipelineBuilder& colorAttachment(VkFormat format,
                                      bool blend = false);
    PipelineBuilder& colorAttachments(const std::vector<VkFormat>& formats);  // MRT

    // --- Dynamic state ---
    PipelineBuilder& dynamicState(VkDynamicState state);
    PipelineBuilder& dynamicStates(std::initializer_list<VkDynamicState> states);

    // --- Layout ---
    PipelineBuilder& descriptorSetLayout(VkDescriptorSetLayout layout);
    PipelineBuilder& pushConstant(VkShaderStageFlags stages,
                                   std::uint32_t offset, std::uint32_t size);

    // --- Build ---
    [[nodiscard]] VkPipeline build(VkRenderPass renderPass,
                                    std::uint32_t subpass = 0) const;
    [[nodiscard]] VkPipeline buildCompute() const;
    [[nodiscard]] VkPipelineLayout pipelineLayout() const {
        return pipelineLayout_;
    }

    // --- Cleanup (call after build, or keep shader modules alive) ---
    void destroyShaderModules() const;

  private:
    VulkanContext* ctx_;

    // Shaders
    std::string vertSpvPath_;
    std::string fragSpvPath_;
    std::string compSpvPath_;

    // Vertex
    VkVertexInputBindingDescription bindingDesc_{};
    std::vector<VkVertexInputAttributeDescription> attrDescs_;

    // Depth
    bool depthTest_ = true;
    VkCompareOp depthOp_ = VK_COMPARE_OP_LESS;
    bool depthWrite_ = true;

    // Raster
    VkCullModeFlags cullMode_ = VK_CULL_MODE_BACK_BIT;
    VkPolygonMode polyMode_ = VK_POLYGON_MODE_FILL;
    VkFrontFace frontFace_ = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Color
    std::vector<VkFormat> colorFormats_;
    bool blend_ = false;

    // Dynamic
    std::vector<VkDynamicState> dynamicStates_{
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    // Layout
    VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
    VkPushConstantRange pushConst_{};
    bool hasPushConst_ = false;

    // Cached during build
    mutable VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    mutable VkShaderModule vertModule_ = VK_NULL_HANDLE;
    mutable VkShaderModule fragModule_ = VK_NULL_HANDLE;
    mutable VkShaderModule compModule_ = VK_NULL_HANDLE;
};

}  // namespace vr
