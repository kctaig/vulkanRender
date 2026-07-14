#include "renderer/PipelineBuilder.h"

#include <stdexcept>

namespace vr {

PipelineBuilder& PipelineBuilder::vertexShader(const std::string& spvPath) {
    vertSpvPath_ = spvPath;
    return *this;
}

PipelineBuilder& PipelineBuilder::fragmentShader(const std::string& spvPath) {
    fragSpvPath_ = spvPath;
    return *this;
}

PipelineBuilder& PipelineBuilder::depthTest(bool enable, VkCompareOp op) {
    depthTest_ = enable;
    depthOp_ = op;
    return *this;
}

PipelineBuilder& PipelineBuilder::depthWrite(bool enable) {
    depthWrite_ = enable;
    return *this;
}

PipelineBuilder& PipelineBuilder::cullMode(VkCullModeFlags mode) {
    cullMode_ = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::polygonMode(VkPolygonMode mode) {
    polyMode_ = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::frontFace(VkFrontFace face) {
    frontFace_ = face;
    return *this;
}

PipelineBuilder& PipelineBuilder::colorAttachment(VkFormat format, bool blend) {
    colorFormat_ = format;
    blend_ = blend;
    return *this;
}

PipelineBuilder& PipelineBuilder::dynamicState(VkDynamicState state) {
    dynamicStates_.push_back(state);
    return *this;
}

PipelineBuilder& PipelineBuilder::dynamicStates(
    std::initializer_list<VkDynamicState> states) {
    dynamicStates_.assign(states);
    return *this;
}

PipelineBuilder& PipelineBuilder::descriptorSetLayout(
    VkDescriptorSetLayout layout) {
    setLayout_ = layout;
    return *this;
}

PipelineBuilder& PipelineBuilder::pushConstant(VkShaderStageFlags stages,
                                                 std::uint32_t offset,
                                                 std::uint32_t size) {
    pushConst_.stageFlags = stages;
    pushConst_.offset = offset;
    pushConst_.size = size;
    hasPushConst_ = true;
    return *this;
}

void PipelineBuilder::destroyShaderModules() const {
    if (vertModule_) vkDestroyShaderModule(ctx_->device(), vertModule_, nullptr);
    if (fragModule_) vkDestroyShaderModule(ctx_->device(), fragModule_, nullptr);
}

VkPipeline PipelineBuilder::build(VkRenderPass renderPass,
                                   std::uint32_t subpass) const {
    // --- Shader modules ---
    auto vertCode = VulkanContext::readBinaryFile(vertSpvPath_.c_str());
    auto fragCode = VulkanContext::readBinaryFile(fragSpvPath_.c_str());
    vertModule_ = ctx_->createShaderModule(vertCode);
    fragModule_ = ctx_->createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_VERTEX_BIT, vertModule_, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_FRAGMENT_BIT, fragModule_, "main", nullptr},
    };

    // --- Vertex input ---
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bindingDesc_;
    vi.vertexAttributeDescriptionCount =
        static_cast<std::uint32_t>(attrDescs_.size());
    vi.pVertexAttributeDescriptions = attrDescs_.data();

    // --- Input assembly ---
    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // --- Viewport ---
    VkPipelineViewportStateCreateInfo vs{};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1;
    vs.scissorCount = 1;

    // --- Rasterization ---
    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = polyMode_;
    rs.cullMode = cullMode_;
    rs.frontFace = frontFace_;
    rs.lineWidth = 1.0f;

    // --- Multisample ---
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // --- Depth ---
    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = depthTest_ ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = depthWrite_ ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = depthOp_;

    // --- Color blend ---
    VkPipelineColorBlendAttachmentState cb{};
    cb.blendEnable = blend_ ? VK_TRUE : VK_FALSE;
    cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbs{};
    cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbs.attachmentCount = 1;
    cbs.pAttachments = &cb;

    // --- Dynamic ---
    VkPipelineDynamicStateCreateInfo dy{};
    dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dy.dynamicStateCount =
        static_cast<std::uint32_t>(dynamicStates_.size());
    dy.pDynamicStates = dynamicStates_.data();

    // --- Pipeline layout ---
    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = setLayout_ ? 1u : 0u;
    pl.pSetLayouts = &setLayout_;
    pl.pushConstantRangeCount = hasPushConst_ ? 1u : 0u;
    pl.pPushConstantRanges = &pushConst_;

    if (vkCreatePipelineLayout(ctx_->device(), &pl, nullptr,
                                &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("PipelineBuilder: vkCreatePipelineLayout failed");
    }

    // --- Pipeline ---
    VkGraphicsPipelineCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vi;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vs;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;
    pi.pDepthStencilState = &ds;
    pi.pColorBlendState = &cbs;
    pi.pDynamicState = &dy;
    pi.layout = pipelineLayout_;
    pi.renderPass = renderPass;
    pi.subpass = subpass;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(ctx_->device(), VK_NULL_HANDLE, 1, &pi,
                                   nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("PipelineBuilder: vkCreateGraphicsPipelines failed");
    }

    return pipeline;
}

}  // namespace vr
