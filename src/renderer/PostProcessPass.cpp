#include "renderer/PostProcessPass.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include "core/VulkanContext.h"
#include "renderer/PipelineBuilder.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/ResourceTable.h"

namespace vr {

bool PostProcessPass::initialize(VulkanContext& ctx) {
    ctx_ = &ctx;
    try {
        // Sampler for HDR input
        VkSampler samp = VK_NULL_HANDLE;
        {
            VkSamplerCreateInfo si{};
            si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            si.magFilter = VK_FILTER_LINEAR; si.minFilter = VK_FILTER_LINEAR;
            si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            vkCreateSampler(ctx.device(), &si, nullptr, &samp);
        }
        hdrSampler_ = UniqueSampler(samp, DeleterSampler, ctx.device());

        // VkRenderPass: swapchain format
        VkAttachmentDescription cA{};
        cA.format = ctx.swapchainFormat(); cA.samples = VK_SAMPLE_COUNT_1_BIT;
        cA.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; cA.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        cA.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        cA.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference cR{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        VkSubpassDescription sp{};
        sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sp.colorAttachmentCount = 1;
        sp.pColorAttachments = &cR;

        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL; dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        createRenderPass(ctx, {cA}, {sp}, {dep});

        // Framebuffers: per-swapchain-image
        const auto& swapchainViews = ctx.swapchainImageViews();
        swapchainFramebuffers_.resize(swapchainViews.size());
        for (size_t i = 0; i < swapchainViews.size(); ++i) {
            VkFramebufferCreateInfo fb{};
            fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fb.renderPass = renderPass_;
            fb.attachmentCount = 1;
            fb.pAttachments = &swapchainViews[i];
            fb.width = ctx.swapchainExtent().width;
            fb.height = ctx.swapchainExtent().height;
            fb.layers = 1;
            vkCreateFramebuffer(ctx.device(), &fb, nullptr, &swapchainFramebuffers_[i]);
        }

        createDescriptorSetLayout(ctx);
        createUniformBuffers<PostUBO>(ctx, postUBOs_);
        createDescriptorPool(ctx, {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxFramesInFlight},
                                   {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    kMaxFramesInFlight}},
                              descPool_);
        createDescriptorSets(ctx);
        createPipeline(ctx);
    } catch (const std::exception& e) {
        std::cerr << "[PostProcessPass] Init failed: " << e.what() << "\n";
        return false;
    }
    return true;
}

void PostProcessPass::record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                              std::uint32_t imageIndex) {
    // Update UBO
    {
        PostUBO ubo{};
        ubo.exposure = 1.0f;
        ubo.gamma = 2.2f;
        void* m = nullptr;
        vkMapMemory(ctx_->device(), postUBOs_[frameIndex].memory(), 0,
                    sizeof(PostUBO), 0, &m);
        std::memcpy(m, &ubo, sizeof(PostUBO));
        vkUnmapMemory(ctx_->device(), postUBOs_[frameIndex].memory());
    }

    // Bind HDR input
    auto* hdrEntry = resources_->get("lighting.hdr");
    if (hdrEntry) {
        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView = hdrEntry->view;
        imgInfo.sampler = hdrSampler_;

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descSets_[frameIndex];
        w.dstBinding = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.descriptorCount = 1;
        w.pImageInfo = &imgInfo;
        vkUpdateDescriptorSets(ctx_->device(), 1, &w, 0, nullptr);
    }

    VkClearValue cv{};
    cv.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = renderPass_;
    passInfo.framebuffer = swapchainFramebuffers_[imageIndex];
    passInfo.renderArea = {{0, 0}, ctx_->swapchainExtent()};
    passInfo.clearValueCount = 1;
    passInfo.pClearValues = &cv;

    vkCmdBeginRenderPass(cmd, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout_, 0, 1, &descSets_[frameIndex], 0, nullptr);

    VkViewport vp{0, 0, static_cast<float>(ctx_->swapchainExtent().width),
                   static_cast<float>(ctx_->swapchainExtent().height), 0, 1};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D scissor{{0, 0}, ctx_->swapchainExtent()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
}

void PostProcessPass::onSwapchainResize(VulkanContext& ctx) {
    for (auto fb : swapchainFramebuffers_)
        vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    swapchainFramebuffers_.clear();
    pipeline_.reset();
    pipelineLayout_.reset();
    if (renderPass_) {
        vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

    VkAttachmentDescription cA{};
    cA.format = ctx.swapchainFormat(); cA.samples = VK_SAMPLE_COUNT_1_BIT;
    cA.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; cA.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    cA.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    cA.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference cR{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp{};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1; sp.pColorAttachments = &cR;
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL; dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    createRenderPass(ctx, {cA}, {sp}, {dep});

    const auto& swapchainViews = ctx.swapchainImageViews();
    swapchainFramebuffers_.resize(swapchainViews.size());
    for (size_t i = 0; i < swapchainViews.size(); ++i) {
        VkFramebufferCreateInfo fb{};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = renderPass_; fb.attachmentCount = 1;
        fb.pAttachments = &swapchainViews[i];
        fb.width = ctx.swapchainExtent().width;
        fb.height = ctx.swapchainExtent().height; fb.layers = 1;
        vkCreateFramebuffer(ctx.device(), &fb, nullptr, &swapchainFramebuffers_[i]);
    }

    createPipeline(ctx);
}

void PostProcessPass::shutdown() {
    for (auto fb : swapchainFramebuffers_)
        vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    swapchainFramebuffers_.clear();
    if (descPool_) vkDestroyDescriptorPool(ctx_->device(), descPool_, nullptr);
    if (renderPass_) vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
    ctx_ = nullptr;
}

// --- Private ---

void PostProcessPass::createDescriptorSetLayout(VulkanContext& ctx) {
    std::array<VkDescriptorSetLayoutBinding, 2> b{};
    b[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    b[1] = {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 2; ci.pBindings = b.data();
    if (vkCreateDescriptorSetLayout(ctx.device(), &ci, nullptr, descSetLayout_.put()) != VK_SUCCESS)
        throw std::runtime_error("PostProcessPass: desc layout failed");
}

void PostProcessPass::createPipeline(VulkanContext& ctx) {
    std::string sd = VR_SHADER_DIR;
    PipelineBuilder b(ctx);
    auto pipe = b.vertexShader(sd + "/fullscreen.vert.spv")
                     .fragmentShader(sd + "/postProcess.frag.spv")
                     .depthTest(false).depthWrite(false)
                     .cullMode(VK_CULL_MODE_NONE)
                     .colorAttachment(ctx.swapchainFormat())
                     .descriptorSetLayout(descSetLayout_)
                     .build(renderPass_);
    pipeline_ = UniquePipeline(pipe, DeleterPipeline, ctx.device());
    pipelineLayout_ = UniquePipelineLayout(b.pipelineLayout(),
                                           DeleterPipelineLayout, ctx.device());
    b.destroyShaderModules();
}

void PostProcessPass::createDescriptorSets(VulkanContext& ctx) {
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        DescriptorWriter(ctx.device())
            .bindSampler(0, VK_NULL_HANDLE, hdrSampler_)  // patched in record()
            .bindBuffer(1, postUBOs_[i], sizeof(PostUBO))
            .build(descSetLayout_, descPool_, &descSets_[i]);
    }
}

}  // namespace vr
