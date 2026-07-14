#include "renderer/ForwardPass.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>

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
        createRenderPass(ctx);
        createDepthResources(ctx);
        createFramebuffers(ctx);
        createDefaultTexture(ctx);
        createDescriptorSetLayout(ctx);
        createUniformBuffers(ctx);
        createDescriptorPool(ctx);
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

    // Bind first available texture from scene assets, fallback to default
    if (scene_ != nullptr && scene_->assets.textureCount() > 0) {
        static bool once = false;
        if (!once) {
            once = true;
            const auto& tex = scene_->assets.texture(0);
            std::cout << "[ForwardPass] Binding scene texture: view=" << tex.view
                      << " sampler=" << tex.sampler
                      << " (" << tex.width << "x" << tex.height << ")" << std::endl;
        }
        const auto& tex = scene_->assets.texture(0);
        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView = tex.view;
        imgInfo.sampler = tex.sampler;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSets_[frameIndex];
        write.dstBinding = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imgInfo;
        vkUpdateDescriptorSets(ctx_->device(), 1, &write, 0, nullptr);
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
    if (depthImageView_) vkDestroyImageView(ctx_->device(), depthImageView_, nullptr);
    if (depthImage_) vkDestroyImage(ctx_->device(), depthImage_, nullptr);
    if (depthImageMemory_) vkFreeMemory(ctx_->device(), depthImageMemory_, nullptr);
    depthImageView_ = VK_NULL_HANDLE;
    depthImage_ = VK_NULL_HANDLE;
    depthImageMemory_ = VK_NULL_HANDLE;
    if (graphicsPipeline_) vkDestroyPipeline(ctx_->device(), graphicsPipeline_, nullptr);
    if (pipelineLayout_) vkDestroyPipelineLayout(ctx_->device(), pipelineLayout_, nullptr);
    if (renderPass_) vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
    graphicsPipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    renderPass_ = VK_NULL_HANDLE;

    createRenderPass(ctx);
    createDepthResources(ctx);
    createFramebuffers(ctx);
    createGraphicsPipeline(ctx);
}

void ForwardPass::shutdown() {
    std::cout << "[ForwardPass] Shutdown begin, renderPass=" << renderPass_
              << std::endl;
    for (auto fb : swapchainFramebuffers_)
        vkDestroyFramebuffer(ctx_->device(), fb, nullptr);
    swapchainFramebuffers_.clear();
    if (depthImageView_) vkDestroyImageView(ctx_->device(), depthImageView_, nullptr);
    if (depthImage_) vkDestroyImage(ctx_->device(), depthImage_, nullptr);
    if (depthImageMemory_) vkFreeMemory(ctx_->device(), depthImageMemory_, nullptr);
    if (graphicsPipeline_) vkDestroyPipeline(ctx_->device(), graphicsPipeline_, nullptr);
    if (pipelineLayout_) vkDestroyPipelineLayout(ctx_->device(), pipelineLayout_, nullptr);
    if (renderPass_) vkDestroyRenderPass(ctx_->device(), renderPass_, nullptr);
    if (descriptorPool_) vkDestroyDescriptorPool(ctx_->device(), descriptorPool_, nullptr);
    if (descriptorSetLayout_)
        vkDestroyDescriptorSetLayout(ctx_->device(), descriptorSetLayout_, nullptr);
    for (auto& b : uniformBuffers_)
        if (b) vkDestroyBuffer(ctx_->device(), b, nullptr);
    for (auto& m : uniformBuffersMemory_)
        if (m) vkFreeMemory(ctx_->device(), m, nullptr);
    if (textureSampler_) vkDestroySampler(ctx_->device(), textureSampler_, nullptr);
    if (textureImageView_) vkDestroyImageView(ctx_->device(), textureImageView_, nullptr);
    if (textureImage_) vkDestroyImage(ctx_->device(), textureImage_, nullptr);
    if (textureImageMemory_) vkFreeMemory(ctx_->device(), textureImageMemory_, nullptr);
    ctx_ = nullptr;
}

// ===================================================================
// Private: resource creation
// ===================================================================

void ForwardPass::createRenderPass(VulkanContext& ctx) {
    VkAttachmentDescription color{};
    color.format = ctx.swapchainFormat();
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkFormat df = ctx.findDepthFormat();
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

    std::array<VkAttachmentDescription, 2> atts = {color, depth};
    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = static_cast<std::uint32_t>(atts.size());
    info.pAttachments = atts.data();
    info.subpassCount = 1;
    info.pSubpasses = &sp;
    info.dependencyCount = 1;
    info.pDependencies = &dep;
    if (vkCreateRenderPass(ctx.device(), &info, nullptr, &renderPass_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateRenderPass failed");
    std::cout << "[ForwardPass] createRenderPass -> " << renderPass_ << std::endl;
}

void ForwardPass::createDepthResources(VulkanContext& ctx) {
    auto df = ctx.findDepthFormat();
    ctx.createImage(ctx.swapchainExtent().width, ctx.swapchainExtent().height,
                    df, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage_,
                    depthImageMemory_);
    VkImageAspectFlags af = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (VulkanContext::hasStencilComponent(df)) af |= VK_IMAGE_ASPECT_STENCIL_BIT;
    depthImageView_ = ctx.createImageView(depthImage_, df, af);
}

void ForwardPass::createFramebuffers(VulkanContext& ctx) {
    const auto& views = ctx.swapchainImageViews();
    swapchainFramebuffers_.resize(views.size());
    for (size_t i = 0; i < views.size(); ++i) {
        std::array<VkImageView, 2> atts = {views[i], depthImageView_};
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = renderPass_;
        info.attachmentCount = static_cast<std::uint32_t>(atts.size());
        info.pAttachments = atts.data();
        info.width = ctx.swapchainExtent().width;
        info.height = ctx.swapchainExtent().height;
        info.layers = 1;
        if (vkCreateFramebuffer(ctx.device(), &info, nullptr,
                                 &swapchainFramebuffers_[i]) != VK_SUCCESS)
            throw std::runtime_error("vkCreateFramebuffer failed");
    }
}

void ForwardPass::createDescriptorSetLayout(VulkanContext& ctx) {
    std::array<VkDescriptorSetLayoutBinding, 2> b{};
    b[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
    b[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
            VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<std::uint32_t>(b.size());
    info.pBindings = b.data();
    if (vkCreateDescriptorSetLayout(ctx.device(), &info, nullptr,
                                     &descriptorSetLayout_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDescriptorSetLayout failed");
}

void ForwardPass::createGraphicsPipeline(VulkanContext& ctx) {
    auto vert = VulkanContext::readBinaryFile(
        (std::string(VR_SHADER_DIR) + "/forwardPass.vert.spv").c_str());
    auto frag = VulkanContext::readBinaryFile(
        (std::string(VR_SHADER_DIR) + "/forwardPass.frag.spv").c_str());
    auto vm = ctx.createShaderModule(vert);
    auto fm = ctx.createShaderModule(frag);

    VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_VERTEX_BIT, vm, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_FRAGMENT_BIT, fm, "main", nullptr},
    };

    auto bd = Vertex::getBindingDescription();
    auto ad = Vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bd;
    vi.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(ad.size());
    vi.pVertexAttributeDescriptions = ad.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vs{};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1;
    vs.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 1.0f;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cb{};
    cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbs{};
    cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbs.attachmentCount = 1;
    cbs.pAttachments = &cb;

    std::array<VkDynamicState, 2> dyn = {VK_DYNAMIC_STATE_VIEWPORT,
                                          VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy{};
    dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dy.dynamicStateCount = static_cast<std::uint32_t>(dyn.size());
    dy.pDynamicStates = dyn.data();

    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &descriptorSetLayout_;
    if (vkCreatePipelineLayout(ctx.device(), &pl, nullptr, &pipelineLayout_) !=
        VK_SUCCESS)
        throw std::runtime_error("vkCreatePipelineLayout failed");

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
    pi.renderPass = renderPass_;
    pi.subpass = 0;
    if (vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &pi,
                                   nullptr, &graphicsPipeline_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateGraphicsPipelines failed");

    vkDestroyShaderModule(ctx.device(), fm, nullptr);
    vkDestroyShaderModule(ctx.device(), vm, nullptr);
}

void ForwardPass::createUniformBuffers(VulkanContext& ctx) {
    for (auto& b : uniformBuffers_) {
        ctx.createBuffer(sizeof(UniformBufferObject),
                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         b, uniformBuffersMemory_[&b - uniformBuffers_.data()]);
    }
}

void ForwardPass::createDescriptorPool(VulkanContext& ctx) {
    std::array<VkDescriptorPoolSize, 2> ps{};
    ps[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxFramesInFlight};
    ps[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxFramesInFlight};
    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = static_cast<std::uint32_t>(ps.size());
    info.pPoolSizes = ps.data();
    info.maxSets = kMaxFramesInFlight;
    if (vkCreateDescriptorPool(ctx.device(), &info, nullptr, &descriptorPool_) !=
        VK_SUCCESS)
        throw std::runtime_error("vkCreateDescriptorPool failed");
}

void ForwardPass::createDescriptorSets(VulkanContext& ctx) {
    std::array<VkDescriptorSetLayout, kMaxFramesInFlight> layouts{};
    layouts.fill(descriptorSetLayout_);
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = descriptorPool_;
    ai.descriptorSetCount = kMaxFramesInFlight;
    ai.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(ctx.device(), &ai, descriptorSets_.data()) !=
        VK_SUCCESS)
        throw std::runtime_error("vkAllocateDescriptorSets failed");

    for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        VkDescriptorBufferInfo bi{uniformBuffers_[i], 0, sizeof(UniformBufferObject)};
        VkDescriptorImageInfo ii{VK_NULL_HANDLE, textureImageView_,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        ii.sampler = textureSampler_;

        std::array<VkWriteDescriptorSet, 2> w{};
        w[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                descriptorSets_[i], 0, 0, 1,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &bi, nullptr};
        w[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                descriptorSets_[i], 1, 0, 1,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &ii, nullptr, nullptr};
        vkUpdateDescriptorSets(ctx.device(), static_cast<std::uint32_t>(w.size()),
                               w.data(), 0, nullptr);
    }
}

void ForwardPass::updateUniformBuffer(VulkanContext& ctx, std::uint32_t fi,
                                       const glm::mat4& model) {
    UniformBufferObject ubo{};
    ubo.model = model;
    if (scene_ != nullptr) {
        auto& cam = scene_->camera;
        cam.setPerspective(
            glm::radians(45.0f),
            static_cast<float>(ctx.swapchainExtent().width) /
                static_cast<float>(ctx.swapchainExtent().height),
            0.1f, 1000.0f);
        ubo.view = cam.viewMatrix();
        ubo.projection = cam.projectionMatrix();
    }
    ubo.projection[1][1] *= -1.0f;

    void* d;
    vkMapMemory(ctx.device(), uniformBuffersMemory_[fi], 0, sizeof(ubo), 0, &d);
    std::memcpy(d, &ubo, sizeof(ubo));
    vkUnmapMemory(ctx.device(), uniformBuffersMemory_[fi]);
}

// ===================================================================
// Default texture (checkerboard)
// ===================================================================

void ForwardPass::createDefaultTexture(VulkanContext& ctx) {
    constexpr uint32_t W = 256, H = 256, CS = 32;
    std::vector<uint8_t> px(W * H * 4);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            bool w = ((x / CS) + (y / CS)) % 2 == 0;
            uint8_t v = w ? 220 : 60;
            size_t i = (y * W + x) * 4;
            px[i] = v;
            px[i + 1] = v;
            px[i + 2] = v;
            px[i + 3] = 255;
        }

    VkDeviceSize sz = W * H * 4;
    VkBuffer sb;
    VkDeviceMemory sm;
    ctx.createBuffer(sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     sb, sm);
    void* d;
    vkMapMemory(ctx.device(), sm, 0, sz, 0, &d);
    std::memcpy(d, px.data(), sz);
    vkUnmapMemory(ctx.device(), sm);

    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {W, H, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.format = VK_FORMAT_R8G8B8A8_SRGB;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateImage(ctx.device(), &ii, nullptr, &textureImage_);

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(ctx.device(), textureImage_, &mr);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex =
        ctx.findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(ctx.device(), &ai, nullptr, &textureImageMemory_);
    vkBindImageMemory(ctx.device(), textureImage_, textureImageMemory_, 0);

    // Copy + transition via one-shot cmd
    auto cmds = ctx.allocateCommandBuffers(1);
    auto cmd = cmds[0];
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    VkImageMemoryBarrier bar{};
    bar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.image = textureImage_;
    bar.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    bar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    bar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &bar);

    VkBufferImageCopy cp{};
    cp.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    cp.imageExtent = {W, H, 1};
    vkCmdCopyBufferToImage(cmd, sb, textureImage_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);

    bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    bar.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &bar);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.graphicsQueue(), 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.graphicsQueue());
    vkFreeCommandBuffers(ctx.device(), ctx.commandPool(), 1, &cmd);

    vkDestroyBuffer(ctx.device(), sb, nullptr);
    vkFreeMemory(ctx.device(), sm, nullptr);

    textureImageView_ = ctx.createImageView(textureImage_, VK_FORMAT_R8G8B8A8_SRGB,
                                             VK_IMAGE_ASPECT_COLOR_BIT);
    VkSamplerCreateInfo sc{};
    sc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sc.magFilter = VK_FILTER_LINEAR;
    sc.minFilter = VK_FILTER_LINEAR;
    sc.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sc.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sc.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(ctx.device(), &sc, nullptr, &textureSampler_);
}

}  // namespace vr
