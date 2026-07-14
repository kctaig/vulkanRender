#include "renderer/ForwardPass.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>

#include "core/VulkanContext.h"
#include "scene/Scene.h"

namespace vr {

VkVertexInputBindingDescription ForwardPass::Vertex::getBindingDescription() {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::array<VkVertexInputAttributeDescription, 3>
ForwardPass::Vertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributes{};
    attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)};
    attributes[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
    attributes[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)};
    return attributes;
}

// ===================================================================
// RenderPass interface
// ===================================================================

bool ForwardPass::initialize(VulkanContext& ctx) {
    ctx_ = &ctx;
    try {
        auto depthFormat = ctx.findDepthFormat();

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = ctx.swapchainFormat();
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        createRenderPass(ctx, {colorAttachment, depthAttachment}, {subpass}, {dependency});
        createDepth(ctx, depthImage_);
        createFramebuffers(ctx, depthImage_.view());
        createDefaultTexture(ctx, textureImage_, textureSampler_);
        createDescriptorSetLayout(ctx);
        createUniformBuffers<Uniforms>(ctx, uniformBuffers_);
        createDescriptorPool(ctx,
                              {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxFramesInFlight},
                               {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                kMaxFramesInFlight}},
                              descriptorPool_);
        createDescriptorSets(ctx);
        createGraphicsPipeline(ctx);
    } catch (const std::exception& e) {
        std::cerr << "[ForwardPass] Init failed: " << e.what() << "\n";
        return false;
    }
    return true;
}

void ForwardPass::record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                          std::uint32_t imageIndex) {
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.05f, 0.07f, 0.11f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = renderPass_;
    passInfo.framebuffer = swapchainFramebuffers_[imageIndex];
    passInfo.renderArea = {{0, 0}, ctx_->swapchainExtent()};
    passInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
    passInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport viewport{0, 0, static_cast<float>(ctx_->swapchainExtent().width),
                         static_cast<float>(ctx_->swapchainExtent().height), 0, 1};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, ctx_->swapchainExtent()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    if (scene_ != nullptr && scene_->assets.textureCount() > 0) {
        const auto& tex = scene_->assets.texture(0);
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = tex.view;
        imageInfo.sampler = tex.sampler;
        VkWriteDescriptorSet textureWrite{};
        textureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        textureWrite.dstSet = descriptorSets_[frameIndex];
        textureWrite.dstBinding = 1;
        textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureWrite.descriptorCount = 1;
        textureWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(ctx_->device(), 1, &textureWrite, 0, nullptr);
    }

    if (scene_ != nullptr) {
        for (const auto& instance : scene_->instances) {
            const auto& mesh = scene_->assets.mesh(instance.modelId);
            updateUniformBuffer(*ctx_, frameIndex, instance.transform);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout_, 0, 1,
                                    &descriptorSets_[frameIndex], 0, nullptr);
            VkBuffer vertexBuffers[] = {mesh.vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmd, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
        }
    }

    vkCmdEndRenderPass(cmd);
}

void ForwardPass::onSwapchainResize(VulkanContext& ctx) {
    for (auto fb : swapchainFramebuffers_)
        vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    swapchainFramebuffers_.clear();
    depthImage_.reset();
    pipeline_.reset();
    pipelineLayout_.reset();
    if (renderPass_) {
        vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

    auto depthFormat = ctx.findDepthFormat();

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = ctx.swapchainFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    createRenderPass(ctx, {colorAttachment, depthAttachment}, {subpass}, {dependency});
    createDepth(ctx, depthImage_);
    createFramebuffers(ctx, depthImage_.view());
    createGraphicsPipeline(ctx);
}

void ForwardPass::shutdown() {
    for (auto fb : swapchainFramebuffers_)
        vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    swapchainFramebuffers_.clear();
    if (descriptorPool_)
        vkDestroyDescriptorPool(ctx_->device(), descriptorPool_, nullptr);
    if (renderPass_)
        vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
    ctx_ = nullptr;
}

// ===================================================================
// Private
// ===================================================================

void ForwardPass::createDescriptorSetLayout(VulkanContext& ctx) {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                   VK_SHADER_STAGE_VERTEX_BIT, nullptr};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                   VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(ctx.device(), &layoutInfo, nullptr,
                                     descriptorSetLayout_.put(ctx.device())) !=
        VK_SUCCESS)
        throw std::runtime_error("vkCreateDescriptorSetLayout failed");
}

void ForwardPass::createGraphicsPipeline(VulkanContext& ctx) {
    std::string shaderDir = VR_SHADER_DIR;
    PipelineBuilder builder(ctx);
    auto pipe = builder.vertexShader(shaderDir + "/forwardPass.vert.spv")
                     .fragmentShader(shaderDir + "/forwardPass.frag.spv")
                     .vertexInput<Vertex>()
                     .depthTest(true)
                     .cullMode(VK_CULL_MODE_BACK_BIT)
                     .colorAttachment(ctx.swapchainFormat())
                     .descriptorSetLayout(descriptorSetLayout_)
                     .build(renderPass_);
    pipeline_ =
        UniquePipeline(pipe, DeleterPipeline, ctx.device());
    pipelineLayout_ = UniquePipelineLayout(builder.pipelineLayout(),
                                            DeleterPipelineLayout, ctx.device());
    builder.destroyShaderModules();
}

void ForwardPass::createDescriptorSets(VulkanContext& ctx) {
    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        DescriptorWriter(ctx.device())
            .bindBuffer(0, uniformBuffers_[i], sizeof(Uniforms))
            .bindSampler(1, textureImage_.view(), textureSampler_)
            .build(descriptorSetLayout_, descriptorPool_, &descriptorSets_[i]);
    }
}

void ForwardPass::updateUniformBuffer(VulkanContext& ctx, std::uint32_t frameIndex,
                                       const glm::mat4& model) {
    Uniforms uniforms{};
    uniforms.model = model;
    if (scene_ != nullptr) {
        uniforms.view = scene_->camera.viewMatrix();
        uniforms.projection = scene_->camera.projectionMatrix();
    }
    uniforms.projection[1][1] *= -1.0f;

    void* mappedData = nullptr;
    vkMapMemory(ctx.device(), uniformBuffers_[frameIndex].memory(), 0,
                sizeof(Uniforms), 0, &mappedData);
    std::memcpy(mappedData, &uniforms, sizeof(Uniforms));
    vkUnmapMemory(ctx.device(), uniformBuffers_[frameIndex].memory());
}

}  // namespace vr
