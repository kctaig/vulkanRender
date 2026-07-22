#include "renderer/PreDepthPass.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include "core/VulkanContext.h"
#include "renderer/ForwardPass.h"
#include "renderer/PipelineBuilder.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/ResourceTable.h"
#include "scene/Scene.h"

namespace vr {

bool PreDepthPass::initialize(VulkanContext& ctx) {
    ctx_ = &ctx;
    try {
        auto depthFormat = ctx.findDepthFormat();

        // Depth-only render pass
        VkAttachmentDescription depthAtt{};
        depthAtt.format = depthFormat;
        depthAtt.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 0;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        createRenderPass(ctx, {depthAtt}, {subpass}, {dep});
        createDepth(ctx, depthImage_);

        // PreDepth only has depth attachment, so create framebuffers manually
        swapchainFramebuffers_.resize(1);
        VkImageView depthView = depthImage_.view();
        VkFramebufferCreateInfo fb{};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = renderPass_;
        fb.attachmentCount = 1;
        fb.pAttachments = &depthView;
        fb.width = ctx.swapchainExtent().width;
        fb.height = ctx.swapchainExtent().height;
        fb.layers = 1;
        if (vkCreateFramebuffer(ctx.device(), &fb, nullptr, &swapchainFramebuffers_[0]) != VK_SUCCESS)
            throw std::runtime_error("PreDepthPass: framebuffer failed");

        createDescriptorSetLayout(ctx);
        createUniformBuffers<GlobalUBO>(ctx, globalUBOs_);
        createDescriptorPool(ctx, {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxFramesInFlight}},
                              descPool_);
        createDescriptorSets(ctx);
        createPipeline(ctx);

        resources_->set("prepass.depth", depthImage_.view(),
                        depthFormat, ctx.swapchainExtent(),
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    } catch (const std::exception& e) {
        std::cerr << "[PreDepthPass] Init failed: " << e.what() << "\n";
        return false;
    }
    return true;
}

void PreDepthPass::record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                           std::uint32_t imageIndex) {
    if (scene_) {
        GlobalUBO ubo{};
        ubo.view = scene_->camera.viewMatrix();
        ubo.proj = scene_->camera.projectionMatrix();
        ubo.proj[1][1] *= -1.0f;
        void* m = nullptr;
        vkMapMemory(ctx_->device(), globalUBOs_[frameIndex].memory(), 0,
                    sizeof(GlobalUBO), 0, &m);
        std::memcpy(m, &ubo, sizeof(GlobalUBO));
        vkUnmapMemory(ctx_->device(), globalUBOs_[frameIndex].memory());
    }

    VkClearValue clearVal{};
    clearVal.depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = renderPass_;
    passInfo.framebuffer = swapchainFramebuffers_[0];  // single framebuffer (depth-only)
    passInfo.renderArea = {{0, 0}, ctx_->swapchainExtent()};
    passInfo.clearValueCount = 1;
    passInfo.pClearValues = &clearVal;

    vkCmdBeginRenderPass(cmd, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport vp{0, 0, static_cast<float>(ctx_->swapchainExtent().width),
                   static_cast<float>(ctx_->swapchainExtent().height), 0, 1};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D scissor{{0, 0}, ctx_->swapchainExtent()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    if (scene_) {
        for (const auto& inst : scene_->instances) {
            const auto& mesh = scene_->assets.mesh(inst.modelId);
            vkCmdPushConstants(cmd, pipelineLayout_,
                               VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(glm::mat4), &inst.transform);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout_, 0, 1, &descSets_[frameIndex], 0, nullptr);
            VkBuffer vb[] = {mesh.vertexBuffer};
            VkDeviceSize off[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vb, off);
            vkCmdBindIndexBuffer(cmd, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
        }
    }

    vkCmdEndRenderPass(cmd);
}

void PreDepthPass::onSwapchainResize(VulkanContext& ctx) {
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
    VkAttachmentDescription dA{};
    dA.format = depthFormat; dA.samples = VK_SAMPLE_COUNT_1_BIT;
    dA.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; dA.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    dA.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    dA.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    dA.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    dA.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkAttachmentReference dR{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp{}; sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.pDepthStencilAttachment = &dR;
    VkSubpassDependency dep{}; dep.srcSubpass = VK_SUBPASS_EXTERNAL; dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    createRenderPass(ctx, {dA}, {sp}, {dep});
    createDepth(ctx, depthImage_);
    createFramebuffers(ctx, depthImage_.view());
    createPipeline(ctx);
    if (resources_) resources_->set("prepass.depth", depthImage_.view(),
                                    depthFormat, ctx.swapchainExtent(),
                                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
}

void PreDepthPass::shutdown() {
    for (auto fb : swapchainFramebuffers_)
        vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    swapchainFramebuffers_.clear();
    if (descPool_) vkDestroyDescriptorPool(ctx_->device(), descPool_, nullptr);
    if (renderPass_) vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
    ctx_ = nullptr;
}

// --- Private ---

void PreDepthPass::createDescriptorSetLayout(VulkanContext& ctx) {
    VkDescriptorSetLayoutBinding b{};
    b.binding = 0; b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b.descriptorCount = 1; b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 1; ci.pBindings = &b;
    if (vkCreateDescriptorSetLayout(ctx.device(), &ci, nullptr, descSetLayout_.put()) != VK_SUCCESS)
        throw std::runtime_error("PreDepthPass: desc layout failed");
}

void PreDepthPass::createPipeline(VulkanContext& ctx) {
    std::string sd = VR_SHADER_DIR;
    PipelineBuilder b(ctx);
    auto pipe = b.vertexShader(sd + "/preDepth.vert.spv")
                     .vertexInput<ForwardPass::Vertex>()
                     .depthTest(true).depthWrite(true)
                     .cullMode(VK_CULL_MODE_BACK_BIT)
                     .descriptorSetLayout(descSetLayout_)
                     .pushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4))
                     .build(renderPass_);
    pipeline_ = UniquePipeline(pipe, DeleterPipeline, ctx.device());
    pipelineLayout_ = UniquePipelineLayout(b.pipelineLayout(), DeleterPipelineLayout, ctx.device());
    b.destroyShaderModules();
}

void PreDepthPass::createDescriptorSets(VulkanContext& ctx) {
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
        DescriptorWriter(ctx.device())
            .bindBuffer(0, globalUBOs_[i], sizeof(GlobalUBO))
            .build(descSetLayout_, descPool_, &descSets_[i]);
}

}  // namespace vr
