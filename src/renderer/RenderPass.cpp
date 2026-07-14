#include "renderer/RenderPass.h"

#include <cstring>
#include <stdexcept>
#include <vector>

#include "core/VulkanContext.h"

namespace vr {

void RenderPass::createRenderPass(
    VulkanContext& ctx,
    const std::vector<VkAttachmentDescription>& attachments,
    const std::vector<VkSubpassDescription>& subpasses,
    const std::vector<VkSubpassDependency>& deps) {
    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    info.pAttachments = attachments.data();
    info.subpassCount = static_cast<std::uint32_t>(subpasses.size());
    info.pSubpasses = subpasses.data();
    info.dependencyCount = static_cast<std::uint32_t>(deps.size());
    info.pDependencies = deps.data();
    if (vkCreateRenderPass(ctx.device(), &info, nullptr, &renderPass_) !=
        VK_SUCCESS)
        throw std::runtime_error("RenderPass: vkCreateRenderPass failed");
}

void RenderPass::createDepth(VulkanContext& ctx, UniqueImage& outDepth) {
    auto df = ctx.findDepthFormat();
    VkImage img;
    VkDeviceMemory mem;
    ctx.createImage(ctx.swapchainExtent().width, ctx.swapchainExtent().height,
                    df, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, img, mem);
    VkImageAspectFlags af = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (VulkanContext::hasStencilComponent(df))
        af |= VK_IMAGE_ASPECT_STENCIL_BIT;
    VkImageView v = ctx.createImageView(img, df, af);
    outDepth = UniqueImage(ctx.device(), img, mem, v);
}

void RenderPass::createFramebuffers(VulkanContext& ctx,
                                     VkImageView depthView) {
    const auto& views = ctx.swapchainImageViews();
    swapchainFramebuffers_.resize(views.size());
    for (std::size_t i = 0; i < views.size(); ++i) {
        std::array<VkImageView, 2> att = {views[i], depthView};
        VkFramebufferCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fi.renderPass = renderPass_;
        fi.attachmentCount = static_cast<std::uint32_t>(att.size());
        fi.pAttachments = att.data();
        fi.width = ctx.swapchainExtent().width;
        fi.height = ctx.swapchainExtent().height;
        fi.layers = 1;
        if (vkCreateFramebuffer(ctx.device(), &fi, nullptr,
                                 &swapchainFramebuffers_[i]) != VK_SUCCESS)
            throw std::runtime_error("RenderPass: vkCreateFramebuffer failed");
    }
}

void RenderPass::createDescriptorPool(
    VulkanContext& ctx, const std::vector<VkDescriptorPoolSize>& sizes,
    VkDescriptorPool& outPool) {
    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
    info.pPoolSizes = sizes.data();
    info.maxSets = kMaxFramesInFlight;
    if (vkCreateDescriptorPool(ctx.device(), &info, nullptr, &outPool) !=
        VK_SUCCESS)
        throw std::runtime_error("RenderPass: vkCreateDescriptorPool failed");
}

void RenderPass::allocateDescriptorSets(
    VkDescriptorSetLayout layout, VkDescriptorPool pool,
    std::array<VkDescriptorSet, kMaxFramesInFlight>& outSets) {
    std::array<VkDescriptorSetLayout, kMaxFramesInFlight> layouts{};
    layouts.fill(layout);
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = pool;
    ai.descriptorSetCount = kMaxFramesInFlight;
    ai.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(ctx_->device(), &ai, outSets.data()) !=
        VK_SUCCESS)
        throw std::runtime_error("RenderPass: vkAllocateDescriptorSets failed");
}

void RenderPass::createDefaultTexture(VulkanContext& ctx,
                                       UniqueImage& outImage,
                                       UniqueSampler& outSampler) {
    constexpr uint32_t W = 256, H = 256, CS = 32;
    std::vector<uint8_t> px(W * H * 4);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            bool w = ((x / CS) + (y / CS)) % 2 == 0;
            uint8_t c = w ? 220 : 60;
            size_t i = (y * W + x) * 4;
            px[i] = c;
            px[i + 1] = c;
            px[i + 2] = c;
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

    VkImage rawImage;
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
    vkCreateImage(ctx.device(), &ii, nullptr, &rawImage);

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(ctx.device(), rawImage, &mr);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex =
        ctx.findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory rawMem;
    vkAllocateMemory(ctx.device(), &ai, nullptr, &rawMem);
    vkBindImageMemory(ctx.device(), rawImage, rawMem, 0);

    ctx.executeOneShot([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier bar{};
        bar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bar.image = rawImage;
        bar.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        bar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        bar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &bar);
        VkBufferImageCopy cp{};
        cp.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        cp.imageExtent = {W, H, 1};
        vkCmdCopyBufferToImage(cmd, sb, rawImage,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
        bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        bar.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &bar);
    });

    vkDestroyBuffer(ctx.device(), sb, nullptr);
    vkFreeMemory(ctx.device(), sm, nullptr);

    VkImageView rawView =
        ctx.createImageView(rawImage, VK_FORMAT_R8G8B8A8_SRGB,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    VkSampler rawSampler;
    VkSamplerCreateInfo sc{};
    sc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sc.magFilter = VK_FILTER_LINEAR;
    sc.minFilter = VK_FILTER_LINEAR;
    sc.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sc.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sc.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(ctx.device(), &sc, nullptr, &rawSampler);

    outImage = UniqueImage(ctx.device(), rawImage, rawMem, rawView);
    outSampler = UniqueSampler(rawSampler, DeleterSampler, ctx.device());
}

}  // namespace vr
