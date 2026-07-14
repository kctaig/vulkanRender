#include "renderer/ForwardPass.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/VulkanContext.h"
#include "scene/MeshIO.h"

namespace vr {

namespace {

constexpr std::array<ForwardPass::Vertex, 3> kFallbackVertices = {
    ForwardPass::Vertex{{0.0f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}},
    ForwardPass::Vertex{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    ForwardPass::Vertex{{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
};

constexpr std::array<std::uint32_t, 3> kFallbackIndices = {0U, 1U, 2U};

bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

}  // namespace

// ---------------------------------------------------------------------------
// Vertex descriptor (static)
// ---------------------------------------------------------------------------

VkVertexInputBindingDescription ForwardPass::Vertex::getBindingDescription() {
    VkVertexInputBindingDescription desc{};
    desc.binding = 0;
    desc.stride = sizeof(Vertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
}

std::array<VkVertexInputAttributeDescription, 3> ForwardPass::Vertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attrs{};

    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(Vertex, position);

    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(Vertex, normal);

    attrs[2].binding = 0;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset = offsetof(Vertex, uv);

    return attrs;
}

// ===================================================================
// RenderPass interface
// ===================================================================

bool ForwardPass::initialize(VulkanContext& ctx) {
    ctx_ = &ctx;

    try {
        createRenderPass(ctx);
        createDepthResources(ctx);
        createFramebuffers(ctx);

        createDescriptorSetLayout(ctx);
        createUniformBuffers(ctx);
        createDescriptorPool(ctx);
        createDescriptorSets(ctx);

        createGraphicsPipeline(ctx);

        createVertexBuffer(ctx);
        createIndexBuffer(ctx);
    } catch (const std::exception& e) {
        std::cerr << "[ForwardPass] Init failed: " << e.what() << "\n";
        return false;
    }

    return true;
}

void ForwardPass::record(
    VkCommandBuffer cmd, std::uint32_t frameIndex, std::uint32_t imageIndex
) {
    updateUniformBuffer(*ctx_, frameIndex);

    std::array<VkClearValue, 2> clears{};
    clears[0].color = {{0.05f, 0.07f, 0.11f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = renderPass_;
    rpInfo.framebuffer = swapchainFramebuffers_[imageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = ctx_->swapchainExtent();
    rpInfo.clearValueCount = static_cast<std::uint32_t>(clears.size());
    rpInfo.pClearValues = clears.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width = static_cast<float>(ctx_->swapchainExtent().width);
    vp.height = static_cast<float>(ctx_->swapchainExtent().height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.extent = ctx_->swapchainExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkBuffer vbs[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
        &descriptorSets_[frameIndex], 0, nullptr
    );

    vkCmdDrawIndexed(cmd, static_cast<std::uint32_t>(meshIndices_.size()), 1, 0, 0, 0);
    vkCmdEndRenderPass(cmd);
}

void ForwardPass::onSwapchainResize(VulkanContext& ctx) {
    // Destroy swapchain-dependent resources
    for (VkFramebuffer fb : swapchainFramebuffers_) {
        vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    }
    swapchainFramebuffers_.clear();

    if (depthImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx_->device(), depthImageView_, nullptr);
        depthImageView_ = VK_NULL_HANDLE;
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(ctx_->device(), depthImage_, nullptr);
        depthImage_ = VK_NULL_HANDLE;
    }
    if (depthImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), depthImageMemory_, nullptr);
        depthImageMemory_ = VK_NULL_HANDLE;
    }

    if (graphicsPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), graphicsPipeline_, nullptr);
        graphicsPipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_->device(), pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

    // Rebuild
    createRenderPass(ctx);
    createGraphicsPipeline(ctx);
    createDepthResources(ctx);
    createFramebuffers(ctx);
}

void ForwardPass::shutdown() {
    for (VkFramebuffer fb : swapchainFramebuffers_) {
        vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    }
    swapchainFramebuffers_.clear();

    if (depthImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx_->device(), depthImageView_, nullptr);
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(ctx_->device(), depthImage_, nullptr);
    }
    if (depthImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), depthImageMemory_, nullptr);
    }

    if (graphicsPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_->device(), graphicsPipeline_, nullptr);
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_->device(), pipelineLayout_, nullptr);
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
    }

    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx_->device(), descriptorPool_, nullptr);
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx_->device(), descriptorSetLayout_, nullptr);
    }

    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (uniformBuffers_[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx_->device(), uniformBuffers_[i], nullptr);
        }
        if (uniformBuffersMemory_[i] != VK_NULL_HANDLE) {
            vkFreeMemory(ctx_->device(), uniformBuffersMemory_[i], nullptr);
        }
    }

    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->device(), vertexBuffer_, nullptr);
    }
    if (vertexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), vertexBufferMemory_, nullptr);
    }
    if (indexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->device(), indexBuffer_, nullptr);
    }
    if (indexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), indexBufferMemory_, nullptr);
    }

    ctx_ = nullptr;
}

// ===================================================================
// Camera control
// ===================================================================

void ForwardPass::setMeshInputPath(std::string path) {
    meshInputPath_ = std::move(path);
}

void ForwardPass::addModelYaw(float delta) { modelYawRadians_ += delta; }
void ForwardPass::addModelPitch(float delta) { modelPitchRadians_ += delta; }

void ForwardPass::addModelTranslation(float dx, float dy) {
    modelTranslation_.x += dx;
    modelTranslation_.y += dy;
}

void ForwardPass::addCameraDistance(float delta) {
    cameraDistance_ -= delta;
    cameraDistance_ = std::clamp(cameraDistance_, 1.0f, std::max(20.0f, sceneRadius_ * 20.0f));
}

float ForwardPass::sceneRadius() const { return sceneRadius_; }

// ===================================================================
// Private: resource creation  (all use `ctx` parameter, not ctx_)
// ===================================================================

void ForwardPass::createRenderPass(VulkanContext& ctx) {
    VkAttachmentDescription color{};
    color.format = ctx.swapchainFormat();
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    depthFormat_ = ctx.findDepthFormat();
    VkAttachmentDescription depth{};
    depth.format = depthFormat_;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {color, depth};

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    info.pAttachments = attachments.data();
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dep;

    if (vkCreateRenderPass(ctx.device(), &info, nullptr, &renderPass_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateRenderPass failed");
    }
}

void ForwardPass::createDescriptorSetLayout(VulkanContext& ctx) {
    VkDescriptorSetLayoutBinding ubo{};
    ubo.binding = 0;
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.descriptorCount = 1;
    ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &ubo;

    if (vkCreateDescriptorSetLayout(ctx.device(), &info, nullptr, &descriptorSetLayout_) !=
        VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorSetLayout failed");
    }
}

void ForwardPass::createGraphicsPipeline(VulkanContext& ctx) {
    const std::string shaderRoot = VR_SHADER_DIR;

    auto vertCode =
        VulkanContext::readBinaryFile((shaderRoot + "/triangle.vert.spv").c_str());
    auto fragCode =
        VulkanContext::readBinaryFile((shaderRoot + "/triangle.frag.spv").c_str());

    VkShaderModule vertModule = ctx.createShaderModule(vertCode);
    VkShaderModule fragModule = ctx.createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main", nullptr},
    };

    auto bindingDesc = Vertex::getBindingDescription();
    auto attrDescs = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions = attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlend{};
    colorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlend;

    std::array<VkDynamicState, 2> dyn = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dyn.size());
    dynamicState.pDynamicStates = dyn.data();

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout_;

    if (vkCreatePipelineLayout(ctx.device(), &layoutInfo, nullptr, &pipelineLayout_) !=
        VK_SUCCESS) {
        throw std::runtime_error("vkCreatePipelineLayout failed");
    }

    VkGraphicsPipelineCreateInfo pipeInfo{};
    pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeInfo.stageCount = 2;
    pipeInfo.pStages = stages;
    pipeInfo.pVertexInputState = &vertexInput;
    pipeInfo.pInputAssemblyState = &inputAssembly;
    pipeInfo.pViewportState = &viewportState;
    pipeInfo.pRasterizationState = &rasterizer;
    pipeInfo.pMultisampleState = &multisampling;
    pipeInfo.pDepthStencilState = &depthStencil;
    pipeInfo.pColorBlendState = &colorBlending;
    pipeInfo.pDynamicState = &dynamicState;
    pipeInfo.layout = pipelineLayout_;
    pipeInfo.renderPass = renderPass_;
    pipeInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(
            ctx.device(), VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &graphicsPipeline_
        ) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateGraphicsPipelines failed");
    }

    vkDestroyShaderModule(ctx.device(), fragModule, nullptr);
    vkDestroyShaderModule(ctx.device(), vertModule, nullptr);
}

void ForwardPass::createDepthResources(VulkanContext& ctx) {
    depthFormat_ = ctx.findDepthFormat();

    ctx.createImage(
        ctx.swapchainExtent().width, ctx.swapchainExtent().height, depthFormat_,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage_, depthImageMemory_
    );

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (hasStencilComponent(depthFormat_)) {
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    depthImageView_ = ctx.createImageView(depthImage_, depthFormat_, aspect);
}

void ForwardPass::createFramebuffers(VulkanContext& ctx) {
    const auto& views = ctx.swapchainImageViews();
    swapchainFramebuffers_.resize(views.size());

    for (std::size_t i = 0; i < views.size(); ++i) {
        std::array<VkImageView, 2> attachments = {views[i], depthImageView_};

        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = renderPass_;
        info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        info.pAttachments = attachments.data();
        info.width = ctx.swapchainExtent().width;
        info.height = ctx.swapchainExtent().height;
        info.layers = 1;

        if (vkCreateFramebuffer(
                ctx.device(), &info, nullptr, &swapchainFramebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateFramebuffer failed");
        }
    }
}

void ForwardPass::createVertexBuffer(VulkanContext& ctx) {
    if (meshVertices_.empty() || meshIndices_.empty()) {
        loadMeshVertices();
    }

    VkDeviceSize size = sizeof(Vertex) * meshVertices_.size();
    ctx.createBuffer(
        size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertexBuffer_, vertexBufferMemory_
    );

    void* data = nullptr;
    vkMapMemory(ctx.device(), vertexBufferMemory_, 0, size, 0, &data);
    std::memcpy(data, meshVertices_.data(), static_cast<std::size_t>(size));
    vkUnmapMemory(ctx.device(), vertexBufferMemory_);
}

void ForwardPass::createIndexBuffer(VulkanContext& ctx) {
    VkDeviceSize size = sizeof(std::uint32_t) * meshIndices_.size();
    ctx.createBuffer(
        size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBuffer_, indexBufferMemory_
    );

    void* data = nullptr;
    vkMapMemory(ctx.device(), indexBufferMemory_, 0, size, 0, &data);
    std::memcpy(data, meshIndices_.data(), static_cast<std::size_t>(size));
    vkUnmapMemory(ctx.device(), indexBufferMemory_);
}

void ForwardPass::createUniformBuffers(VulkanContext& ctx) {
    VkDeviceSize size = sizeof(UniformBufferObject);
    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        ctx.createBuffer(
            size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers_[i], uniformBuffersMemory_[i]
        );
    }
}

void ForwardPass::createDescriptorPool(VulkanContext& ctx) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = kMaxFramesInFlight;

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = 1;
    info.pPoolSizes = &poolSize;
    info.maxSets = kMaxFramesInFlight;

    if (vkCreateDescriptorPool(ctx.device(), &info, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorPool failed");
    }
}

void ForwardPass::createDescriptorSets(VulkanContext& ctx) {
    std::array<VkDescriptorSetLayout, kMaxFramesInFlight> layouts{};
    layouts.fill(descriptorSetLayout_);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = kMaxFramesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(
            ctx.device(), &allocInfo, descriptorSets_.data()) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateDescriptorSets failed");
    }

    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers_[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSets_[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);
    }
}

void ForwardPass::updateUniformBuffer(VulkanContext& ctx, std::uint32_t frameIndex) {
    UniformBufferObject ubo{};

    glm::mat4 model = glm::translate(glm::mat4(1.0f), modelTranslation_);
    model = glm::rotate(model, modelYawRadians_, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, modelPitchRadians_, glm::vec3(1.0f, 0.0f, 0.0f));

    ubo.model = model;
    ubo.view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, cameraDistance_), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)
    );
    ubo.projection = glm::perspective(
        glm::radians(45.0f),
        static_cast<float>(ctx.swapchainExtent().width) /
            static_cast<float>(ctx.swapchainExtent().height),
        0.1f, std::max(100.0f, sceneRadius_ * 50.0f)
    );
    ubo.projection[1][1] *= -1.0f;

    void* data = nullptr;
    vkMapMemory(ctx.device(), uniformBuffersMemory_[frameIndex], 0, sizeof(ubo), 0, &data);
    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(ctx.device(), uniformBuffersMemory_[frameIndex]);
}

// ===================================================================
// Mesh loading
// ===================================================================

void ForwardPass::loadMeshVertices() {
    meshVertices_.clear();
    meshIndices_.clear();

    std::string filePath = meshInputPath_.empty()
                               ? std::string(VR_ASSET_DIR) + "/models/basic_mesh.obj"
                               : meshInputPath_;

    MeshInputData meshData;
    if (loadObjMesh(filePath, meshData)) {
        meshVertices_.reserve(meshData.vertices.size());
        for (const auto& v : meshData.vertices) {
            meshVertices_.push_back(Vertex{v.position, v.normal, v.uv});
        }
        meshIndices_ = meshData.indices;
    }

    if (meshVertices_.empty() || meshIndices_.empty()) {
        meshVertices_.assign(kFallbackVertices.begin(), kFallbackVertices.end());
        meshIndices_.assign(kFallbackIndices.begin(), kFallbackIndices.end());
    }

    refreshSceneScaleParams();
}

void ForwardPass::refreshSceneScaleParams() {
    if (meshVertices_.empty()) {
        sceneRadius_ = 1.0f;
        return;
    }

    glm::vec3 minPos = meshVertices_.front().position;
    glm::vec3 maxPos = meshVertices_.front().position;
    for (const Vertex& v : meshVertices_) {
        minPos = glm::min(minPos, v.position);
        maxPos = glm::max(maxPos, v.position);
    }

    const glm::vec3 center = (minPos + maxPos) * 0.5f;
    sceneRadius_ = 0.0f;
    for (const Vertex& v : meshVertices_) {
        sceneRadius_ = std::max(sceneRadius_, glm::distance(v.position, center));
    }
    if (sceneRadius_ < 1.0f) {
        sceneRadius_ = 1.0f;
    }
}

}  // namespace vr
