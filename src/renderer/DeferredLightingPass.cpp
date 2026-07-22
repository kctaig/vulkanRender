#include "renderer/DeferredLightingPass.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include "core/VulkanContext.h"
#include "renderer/DDGIVolume.h"
#include "renderer/PipelineBuilder.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/ResourceTable.h"
#include "scene/Scene.h"

namespace vr {

bool DeferredLightingPass::initialize(VulkanContext& ctx) {
    ctx_ = &ctx;
    try {
        auto ext = ctx.swapchainExtent();

        // Create 1x1 dummy image for unused sampler bindings (bindings 5-7 when DDGI off)
        {
            VkImage img; VkDeviceMemory mem;
            ctx.createImage(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, img, mem);
            auto view = ctx.createImageView(img, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
            dummyImage_ = UniqueImage(ctx.device(), img, mem, view);
        }
        // Dummy DDGI config UBO
        {
            VkBuffer buf; VkDeviceMemory mem;
            ctx.createBuffer(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             buf, mem);
            dummyDDGIUBO_ = UniqueBuffer(ctx.device(), buf, mem);
        }

        // Create HDR target
        VkImage img; VkDeviceMemory mem;
        ctx.createImage(ext.width, ext.height, VK_FORMAT_R16G16B16A16_SFLOAT,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, img, mem);
        auto view = ctx.createImageView(img, VK_FORMAT_R16G16B16A16_SFLOAT,
                                         VK_IMAGE_ASPECT_COLOR_BIT);
        hdrTarget_ = UniqueImage(ctx.device(), img, mem, view);

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

        resources_->set("lighting.hdr", hdrTarget_.view(),
                        VK_FORMAT_R16G16B16A16_SFLOAT, ext,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        resources_->setSampler("lighting.sampler", hdrSampler_);

        // VkRenderPass: 1 color attachment
        VkAttachmentDescription cA{};
        cA.format = VK_FORMAT_R16G16B16A16_SFLOAT; cA.samples = VK_SAMPLE_COUNT_1_BIT;
        cA.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; cA.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        cA.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        cA.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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

        // Single framebuffer with HDR target
        swapchainFramebuffers_.resize(1);
        VkFramebufferCreateInfo fb{};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = renderPass_;
        fb.attachmentCount = 1;
        VkImageView hdrView = hdrTarget_.view();
        fb.pAttachments = &hdrView;
        fb.width = ext.width; fb.height = ext.height; fb.layers = 1;
        vkCreateFramebuffer(ctx.device(), &fb, nullptr, &swapchainFramebuffers_[0]);

        createDescriptorSetLayout(ctx);
        createUniformBuffers<LightingUBO>(ctx, lightingUBOs_);
        createDescriptorPool(ctx,
                             {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxFramesInFlight * 2},
                              {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxFramesInFlight * 7}},
                             descPool_);
        createDescriptorSets(ctx);
        createPipeline(ctx);
    } catch (const std::exception& e) {
        std::cerr << "[DeferredLightingPass] Init failed: " << e.what() << "\n";
        return false;
    }
    return true;
}

void DeferredLightingPass::record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                                   std::uint32_t imageIndex) {
    // Update UBO
    if (scene_) {
        LightingUBO ubo{};
        ubo.invViewProj = glm::inverse(scene_->camera.projectionMatrix() *
                                        scene_->camera.viewMatrix());
        ubo.camPos = glm::vec4(scene_->camera.position(), 0.0f);
        if (!scene_->directionalLights.empty()) {
            auto& dl = scene_->directionalLights[0];
            ubo.lightDir = glm::vec4(glm::normalize(dl.direction), dl.intensity);
            ubo.lightColor = glm::vec4(dl.color, 1.0f);
        } else {
            ubo.lightDir = glm::vec4(0.5f, 0.8f, 0.3f, 1.0f);
            ubo.lightColor = glm::vec4(1.0f, 0.95f, 0.85f, 1.0f);
        }

        void* m = nullptr;
        vkMapMemory(ctx_->device(), lightingUBOs_[frameIndex].memory(), 0,
                    sizeof(LightingUBO), 0, &m);
        std::memcpy(m, &ubo, sizeof(LightingUBO));
        vkUnmapMemory(ctx_->device(), lightingUBOs_[frameIndex].memory());
    }

    // Bind GBuffer inputs from ResourceTable
    auto* albedo = resources_->get("gbuf.albedo");
    auto* normal  = resources_->get("gbuf.normalRoughness");
    auto* material = resources_->get("gbuf.material");
    auto* depth  = resources_->get("prepass.depth");
    if (albedo && normal && material && depth) {
        auto gbufSamp = resources_->sampler("gbuf.sampler");
        auto prepassSamp = resources_->defaultSampler();

        // Create sampler for prepass depth if needed
        auto* lightingSamp = resources_->get("lighting.sampler");
        // Update descriptor with GBuffer images
        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorImageInfo> imgInfos(4);

        imgInfos[0].sampler = gbufSamp;
        imgInfos[0].imageView = albedo->view;
        imgInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imgInfos[1].sampler = gbufSamp;
        imgInfos[1].imageView = normal->view;
        imgInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imgInfos[2].sampler = gbufSamp;
        imgInfos[2].imageView = material->view;
        imgInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imgInfos[3].sampler = VK_NULL_HANDLE;
        imgInfos[3].imageView = depth->view;
        imgInfos[3].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        for (uint32_t j = 0; j < 4; ++j) {
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = descSets_[frameIndex];
            w.dstBinding = 1 + j;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.descriptorCount = 1;
            w.pImageInfo = &imgInfos[j];
            writes.push_back(w);
        }
        vkUpdateDescriptorSets(ctx_->device(), static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }

    // Bind DDGI probe textures (or dummy when DDGI disabled)
    {
        VkSampler ddgiSamp = hdrSampler_;
        VkImageView irrView = dummyImage_.view();
        VkImageView depthView = dummyImage_.view();
        VkBuffer cfgBuf = dummyDDGIUBO_;

        if (ddgiVolume_ && ddgiEnabled_) {
            ddgiSamp = ddgiVolume_->atlasSampler();
            auto* irrEntry = resources_->get("ddgi.irradiance");
            auto* depthEntry = resources_->get("ddgi.depth");
            if (irrEntry) irrView = irrEntry->view;
            if (depthEntry) depthView = depthEntry->view;
            cfgBuf = ddgiVolume_->configBuffer();
        }

        for (uint32_t j = 0; j < 2; ++j) {
            VkDescriptorImageInfo imgInfo{};
            imgInfo.sampler = ddgiSamp;
            imgInfo.imageView = j == 0 ? irrView : depthView;
            imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = descSets_[frameIndex];
            w.dstBinding = 5 + j;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.descriptorCount = 1;
            w.pImageInfo = &imgInfo;
            vkUpdateDescriptorSets(ctx_->device(), 1, &w, 0, nullptr);
        }

        VkDescriptorBufferInfo cfgInfo{cfgBuf, 0, 256};
        VkWriteDescriptorSet cfgWrite{};
        cfgWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cfgWrite.dstSet = descSets_[frameIndex];
        cfgWrite.dstBinding = 7;
        cfgWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cfgWrite.descriptorCount = 1;
        cfgWrite.pBufferInfo = &cfgInfo;
        vkUpdateDescriptorSets(ctx_->device(), 1, &cfgWrite, 0, nullptr);
    }

    VkClearValue cv{};
    cv.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = renderPass_;
    passInfo.framebuffer = swapchainFramebuffers_[0];
    passInfo.renderArea = {{0, 0}, ctx_->swapchainExtent()};
    passInfo.clearValueCount = 1;
    passInfo.pClearValues = &cv;

    vkCmdBeginRenderPass(cmd, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout_, 0, 1, &descSets_[frameIndex], 0, nullptr);

    // Full-screen triangle: 3 vertices, no vertex buffer
    VkViewport vp{0, 0, static_cast<float>(ctx_->swapchainExtent().width),
                   static_cast<float>(ctx_->swapchainExtent().height), 0, 1};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D scissor{{0, 0}, ctx_->swapchainExtent()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
}

void DeferredLightingPass::onSwapchainResize(VulkanContext& ctx) {
    for (auto fb : swapchainFramebuffers_)
        vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    swapchainFramebuffers_.clear();
    hdrTarget_.reset();
    pipeline_.reset();
    pipelineLayout_.reset();
    if (renderPass_) {
        vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

    // Recreate HDR target + render pass
    auto ext = ctx.swapchainExtent();
    VkImage img; VkDeviceMemory mem;
    ctx.createImage(ext.width, ext.height, VK_FORMAT_R16G16B16A16_SFLOAT,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, img, mem);
    auto view = ctx.createImageView(img, VK_FORMAT_R16G16B16A16_SFLOAT,
                                     VK_IMAGE_ASPECT_COLOR_BIT);
    hdrTarget_ = UniqueImage(ctx.device(), img, mem, view);

    VkAttachmentDescription cA{};
    cA.format = VK_FORMAT_R16G16B16A16_SFLOAT; cA.samples = VK_SAMPLE_COUNT_1_BIT;
    cA.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; cA.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    cA.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    cA.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

    swapchainFramebuffers_.resize(1);
    VkFramebufferCreateInfo fb{};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = renderPass_; fb.attachmentCount = 1;
    fb.pAttachments = &view;
    fb.width = ext.width; fb.height = ext.height; fb.layers = 1;
    vkCreateFramebuffer(ctx.device(), &fb, nullptr, &swapchainFramebuffers_[0]);

    createPipeline(ctx);
    resources_->set("lighting.hdr", hdrTarget_.view(),
                    VK_FORMAT_R16G16B16A16_SFLOAT, ext,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void DeferredLightingPass::shutdown() {
    for (auto fb : swapchainFramebuffers_)
        vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    swapchainFramebuffers_.clear();
    if (descPool_) vkDestroyDescriptorPool(ctx_->device(), descPool_, nullptr);
    if (renderPass_) vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
    ctx_ = nullptr;
}

// --- Private ---

void DeferredLightingPass::createDescriptorSetLayout(VulkanContext& ctx) {
    std::array<VkDescriptorSetLayoutBinding, 8> b{};
    b[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    b[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    b[2] = {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    b[3] = {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    b[4] = {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    b[5] = {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    b[6] = {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    b[7] = {7, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 8; ci.pBindings = b.data();
    if (vkCreateDescriptorSetLayout(ctx.device(), &ci, nullptr, descSetLayout_.put()) != VK_SUCCESS)
        throw std::runtime_error("DeferredLightingPass: desc layout failed");
}

void DeferredLightingPass::createPipeline(VulkanContext& ctx) {
    std::string sd = VR_SHADER_DIR;
    PipelineBuilder b(ctx);
    auto pipe = b.vertexShader(sd + "/fullscreen.vert.spv")
                     .fragmentShader(sd + "/deferredLighting.frag.spv")
                     .depthTest(false).depthWrite(false)
                     .cullMode(VK_CULL_MODE_NONE)
                     .colorAttachment(VK_FORMAT_R16G16B16A16_SFLOAT)
                     .descriptorSetLayout(descSetLayout_)
                     .build(renderPass_);
    pipeline_ = UniquePipeline(pipe, DeleterPipeline, ctx.device());
    pipelineLayout_ = UniquePipelineLayout(b.pipelineLayout(),
                                           DeleterPipelineLayout, ctx.device());
    b.destroyShaderModules();
}

void DeferredLightingPass::createDescriptorSets(VulkanContext& ctx) {
    // Only write binding 0 (LightingUBO) at init. Bindings 1-7 are updated in record().
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        DescriptorWriter(ctx.device())
            .bindBuffer(0, lightingUBOs_[i], sizeof(LightingUBO))
            .build(descSetLayout_, descPool_, &descSets_[i]);
    }
}

}  // namespace vr
