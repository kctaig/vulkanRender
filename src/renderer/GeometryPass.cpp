#include "renderer/GeometryPass.h"

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

bool GeometryPass::initialize(VulkanContext& ctx) {
    ctx_ = &ctx;
    try {
        createGBufferImages(ctx);
        createDescriptorSetLayout(ctx);
        createUniformBuffers<GlobalUBO>(ctx, globalUBOs_);
        createDescriptorPool(ctx, {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxFramesInFlight},
                                   {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    kMaxFramesInFlight}},
                              descPool_);
        createDescriptorSets(ctx);
        createPipeline(ctx);
    } catch (const std::exception& e) {
        std::cerr << "[GeometryPass] Init failed: " << e.what() << "\n";
        return false;
    }
    return true;
}

void GeometryPass::record(VkCommandBuffer cmd, std::uint32_t frameIndex,
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

    // Update texture descriptor if scene has textures
    if (scene_ && scene_->assets.textureCount() > 0) {
        const auto& tex = scene_->assets.texture(0);
        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView = tex.view;
        imgInfo.sampler = tex.sampler;
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descSets_[frameIndex];
        w.dstBinding = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.descriptorCount = 1;
        w.pImageInfo = &imgInfo;
        vkUpdateDescriptorSets(ctx_->device(), 1, &w, 0, nullptr);
    }

    // Clear values: 3 color + 1 depth (depth loaded, not cleared)
    std::array<VkClearValue, 4> cv{};
    cv[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    cv[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    cv[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    cv[3].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = renderPass_;
    passInfo.framebuffer = swapchainFramebuffers_[0];
    passInfo.renderArea = {{0, 0}, ctx_->swapchainExtent()};
    passInfo.clearValueCount = 4;
    passInfo.pClearValues = cv.data();

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

void GeometryPass::onSwapchainResize(VulkanContext& ctx) {
    pipeline_.reset();
    pipelineLayout_.reset();
    gbufferAlbedo_.reset();
    gbufferNormalRoughness_.reset();
    gbufferMaterial_.reset();
    if (renderPass_) {
        vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    createGBufferImages(ctx);
    createPipeline(ctx);
}

void GeometryPass::shutdown() {
    for (auto fb : swapchainFramebuffers_)
        vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    swapchainFramebuffers_.clear();
    if (descPool_) vkDestroyDescriptorPool(ctx_->device(), descPool_, nullptr);
    if (renderPass_) vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
    ctx_ = nullptr;
}

// --- Private ---

void GeometryPass::createGBufferImages(VulkanContext& ctx) {
    auto ext = ctx.swapchainExtent();
    auto& resources = *resources_;
    VkSampler samp = VK_NULL_HANDLE;
    {
        VkSamplerCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        si.magFilter = VK_FILTER_NEAREST;
        si.minFilter = VK_FILTER_NEAREST;
        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(ctx.device(), &si, nullptr, &samp);
    }
    gbufferSampler_ = UniqueSampler(samp, DeleterSampler, ctx.device());

    // RT0: Albedo RGBA8_SRGB
    {
        VkImage img; VkDeviceMemory mem;
        ctx.createImage(ext.width, ext.height, VK_FORMAT_R8G8B8A8_SRGB,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, img, mem);
        auto view = ctx.createImageView(img, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
        gbufferAlbedo_ = UniqueImage(ctx.device(), img, mem, view);
    }
    // RT1: Normal+Roughness RGBA16F
    {
        VkImage img; VkDeviceMemory mem;
        ctx.createImage(ext.width, ext.height, VK_FORMAT_R16G16B16A16_SFLOAT,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, img, mem);
        auto view = ctx.createImageView(img, VK_FORMAT_R16G16B16A16_SFLOAT,
                                         VK_IMAGE_ASPECT_COLOR_BIT);
        gbufferNormalRoughness_ = UniqueImage(ctx.device(), img, mem, view);
    }
    // RT2: Material RGBA8
    {
        VkImage img; VkDeviceMemory mem;
        ctx.createImage(ext.width, ext.height, VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, img, mem);
        auto view = ctx.createImageView(img, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
        gbufferMaterial_ = UniqueImage(ctx.device(), img, mem, view);
    }

    // Build VkRenderPass: 3 color + 1 depth (loaded from prepass)
    auto depthFormat = ctx.findDepthFormat();
    auto* depthEntry = resources.get("prepass.depth");
    if (!depthEntry) throw std::runtime_error("GeometryPass: missing prepass.depth");

    VkAttachmentDescription c0{}, c1{}, c2{}, dA{};
    c0.format = VK_FORMAT_R8G8B8A8_SRGB; c0.samples = VK_SAMPLE_COUNT_1_BIT;
    c0.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; c0.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    c0.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    c0.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    c1.format = VK_FORMAT_R16G16B16A16_SFLOAT; c1.samples = VK_SAMPLE_COUNT_1_BIT;
    c1.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; c1.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    c1.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    c1.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    c2.format = VK_FORMAT_R8G8B8A8_UNORM; c2.samples = VK_SAMPLE_COUNT_1_BIT;
    c2.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; c2.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    c2.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    c2.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    dA.format = depthFormat; dA.samples = VK_SAMPLE_COUNT_1_BIT;
    dA.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // keep prepass depth
    dA.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    dA.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    dA.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    dA.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    dA.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    std::vector<VkAttachmentDescription> atts = {c0, c1, c2, dA};

    VkAttachmentReference cR0{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference cR1{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference cR2{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dR{3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
    std::vector<VkAttachmentReference> cRefs = {cR0, cR1, cR2};

    VkSubpassDescription sp{};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 3;
    sp.pColorAttachments = cRefs.data();
    sp.pDepthStencilAttachment = &dR;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    createRenderPass(ctx, atts, {sp}, {dep});

    // Create single framebuffer (reused each frame — no per-swapchain views needed)
    swapchainFramebuffers_.resize(1);
    std::array<VkImageView, 4> fbViews = {
        gbufferAlbedo_.view(), gbufferNormalRoughness_.view(),
        gbufferMaterial_.view(), depthEntry->view};
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = renderPass_;
    fbInfo.attachmentCount = 4;
    fbInfo.pAttachments = fbViews.data();
    fbInfo.width = ext.width;
    fbInfo.height = ext.height;
    fbInfo.layers = 1;
    if (vkCreateFramebuffer(ctx.device(), &fbInfo, nullptr, &swapchainFramebuffers_[0]) != VK_SUCCESS)
        throw std::runtime_error("GeometryPass: framebuffer failed");

    // Register outputs
    resources.set("gbuf.albedo", gbufferAlbedo_.view(), VK_FORMAT_R8G8B8A8_SRGB,
                  ext, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    resources.set("gbuf.normalRoughness", gbufferNormalRoughness_.view(),
                  VK_FORMAT_R16G16B16A16_SFLOAT, ext, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    resources.set("gbuf.material", gbufferMaterial_.view(), VK_FORMAT_R8G8B8A8_UNORM,
                  ext, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    resources.setSampler("gbuf.sampler", gbufferSampler_);
}

void GeometryPass::createDescriptorSetLayout(VulkanContext& ctx) {
    std::array<VkDescriptorSetLayoutBinding, 2> b{};
    b[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
    b[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
            VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 2; ci.pBindings = b.data();
    if (vkCreateDescriptorSetLayout(ctx.device(), &ci, nullptr, descSetLayout_.put()) != VK_SUCCESS)
        throw std::runtime_error("GeometryPass: desc layout failed");
}

void GeometryPass::createPipeline(VulkanContext& ctx) {
    std::string sd = VR_SHADER_DIR;
    std::vector<VkFormat> fmts = {
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R8G8B8A8_UNORM};

    PipelineBuilder b(ctx);
    auto pipe = b.vertexShader(sd + "/geometry.vert.spv")
                     .fragmentShader(sd + "/geometry.frag.spv")
                     .vertexInput<ForwardPass::Vertex>()
                     .depthTest(true).depthWrite(false)  // read-only depth from prepass
                     .cullMode(VK_CULL_MODE_BACK_BIT)
                     .colorAttachments(fmts)
                     .descriptorSetLayout(descSetLayout_)
                     .pushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4))
                     .build(renderPass_);
    pipeline_ = UniquePipeline(pipe, DeleterPipeline, ctx.device());
    pipelineLayout_ = UniquePipelineLayout(b.pipelineLayout(),
                                           DeleterPipelineLayout, ctx.device());
    b.destroyShaderModules();
}

void GeometryPass::createDescriptorSets(VulkanContext& ctx) {
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
        DescriptorWriter(ctx.device())
            .bindBuffer(0, globalUBOs_[i], sizeof(GlobalUBO))
            .bindSampler(1, gbufferAlbedo_.view(), gbufferSampler_) // temp; updated in record
            .build(descSetLayout_, descPool_, &descSets_[i]);
}

}  // namespace vr
