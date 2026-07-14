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

// ---------------------------------------------------------------------------
// Vertex descriptor (static)
// ---------------------------------------------------------------------------

VkVertexInputBindingDescription ForwardPass::Vertex::getBindingDescription() {
    VkVertexInputBindingDescription d{};
    d.binding = 0;
    d.stride = sizeof(Vertex);
    d.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return d;
}

std::array<VkVertexInputAttributeDescription, 3>
ForwardPass::Vertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> a{};
    a[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)};
    a[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
    a[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)};
    return a;
}

// ===================================================================
// RenderPass interface
// ===================================================================

bool ForwardPass::initialize(VulkanContext& ctx) {
    ctx_ = &ctx;
    try {
        auto df = ctx.findDepthFormat();

        VkAttachmentDescription color{};
        color.format = ctx.swapchainFormat();
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depth{};
        depth.format = df;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference cr{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference dr{1,
                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sp{};
        sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sp.colorAttachmentCount = 1;
        sp.pColorAttachments = &cr;
        sp.pDepthStencilAttachment = &dr;

        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        createRenderPass(ctx, {color, depth}, {sp}, {dep});
        createDepth(ctx, depthImage_);
        createFramebuffers(ctx, depthImage_.view());
        createDefaultTexture(ctx, textureImage_, textureSampler_);
        createDescriptorSetLayout(ctx);
        createUniformBuffers<UniformBufferObject>(ctx, uniformBuffers_);
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
    std::array<VkClearValue, 2> clears{};
    clears[0].color = {{0.05f, 0.07f, 0.11f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = renderPass_;
    rp.framebuffer = swapchainFramebuffers_[imageIndex];
    rp.renderArea = {{0, 0}, ctx_->swapchainExtent()};
    rp.clearValueCount = static_cast<std::uint32_t>(clears.size());
    rp.pClearValues = clears.data();

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

    VkViewport vp{0, 0, static_cast<float>(ctx_->swapchainExtent().width),
                   static_cast<float>(ctx_->swapchainExtent().height), 0, 1};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D scissor{{0, 0}, ctx_->swapchainExtent()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    if (scene_ != nullptr && scene_->assets.textureCount() > 0) {
        const auto& tex = scene_->assets.texture(0);
        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView = tex.view;
        imgInfo.sampler = tex.sampler;
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descriptorSets_[frameIndex];
        w.dstBinding = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.descriptorCount = 1;
        w.pImageInfo = &imgInfo;
        vkUpdateDescriptorSets(ctx_->device(), 1, &w, 0, nullptr);
    }

    if (scene_ != nullptr) {
        for (const auto& inst : scene_->instances) {
            const auto& mesh = scene_->assets.mesh(inst.modelId);
            updateUniformBuffer(*ctx_, frameIndex, inst.transform);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout_, 0, 1,
                                    &descriptorSets_[frameIndex], 0, nullptr);
            VkBuffer vbs[] = {mesh.vertexBuffer};
            VkDeviceSize off[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vbs, off);
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
    graphicsPipeline_.reset();
    pipelineLayout_.reset();
    if (renderPass_) {
        vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

    auto df = ctx.findDepthFormat();
    VkAttachmentDescription color{};
    color.format = ctx.swapchainFormat();
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentDescription depth{};
    depth.format = df;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference cr{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dr{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp{};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &cr;
    sp.pDepthStencilAttachment = &dr;
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    createRenderPass(ctx, {color, depth}, {sp}, {dep});
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
    std::array<VkDescriptorSetLayoutBinding, 2> b{};
    b[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
            VK_SHADER_STAGE_VERTEX_BIT, nullptr};
    b[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
            VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<std::uint32_t>(b.size());
    info.pBindings = b.data();
    if (vkCreateDescriptorSetLayout(ctx.device(), &info, nullptr,
                                     descriptorSetLayout_.put(ctx.device())) !=
        VK_SUCCESS)
        throw std::runtime_error("vkCreateDescriptorSetLayout failed");
}

void ForwardPass::createGraphicsPipeline(VulkanContext& ctx) {
    std::string sd = VR_SHADER_DIR;
    PipelineBuilder builder(ctx);
    auto pipe = builder.vertexShader(sd + "/forwardPass.vert.spv")
                     .fragmentShader(sd + "/forwardPass.frag.spv")
                     .vertexInput<Vertex>()
                     .depthTest(true)
                     .cullMode(VK_CULL_MODE_BACK_BIT)
                     .colorAttachment(ctx.swapchainFormat())
                     .descriptorSetLayout(descriptorSetLayout_)
                     .build(renderPass_);
    graphicsPipeline_ =
        UniquePipeline(pipe, DeleterPipeline, ctx.device());
    pipelineLayout_ = UniquePipelineLayout(builder.pipelineLayout(),
                                            DeleterPipelineLayout, ctx.device());
    builder.destroyShaderModules();
}

void ForwardPass::createDescriptorSets(VulkanContext& ctx) {
    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        DescriptorWriter(ctx.device())
            .bindBuffer(0, uniformBuffers_[i], sizeof(UniformBufferObject))
            .bindSampler(1, textureImage_.view(), textureSampler_)
            .build(descriptorSetLayout_, descriptorPool_, &descriptorSets_[i]);
    }
}

void ForwardPass::updateUniformBuffer(VulkanContext& ctx, std::uint32_t fi,
                                       const glm::mat4& model) {
    UniformBufferObject ubo{};
    ubo.model = model;
    if (scene_ != nullptr) {
        ubo.view = scene_->camera.viewMatrix();
        ubo.projection = scene_->camera.projectionMatrix();
    }
    ubo.projection[1][1] *= -1.0f;

    void* d;
    vkMapMemory(ctx.device(), uniformBuffers_[fi].memory(), 0, sizeof(ubo),
                0, &d);
    std::memcpy(d, &ubo, sizeof(ubo));
    vkUnmapMemory(ctx.device(), uniformBuffers_[fi].memory());
}

}  // namespace vr
