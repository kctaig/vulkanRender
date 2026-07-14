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
    const std::vector<VkSubpassDependency>& dependencies) {
    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.subpassCount = static_cast<std::uint32_t>(subpasses.size());
    createInfo.pSubpasses = subpasses.data();
    createInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
    createInfo.pDependencies = dependencies.data();
    if (vkCreateRenderPass(ctx.device(), &createInfo, nullptr, &renderPass_) !=
        VK_SUCCESS)
        throw std::runtime_error("RenderPass: vkCreateRenderPass failed");
}

void RenderPass::createDepth(VulkanContext& ctx, UniqueImage& outDepth) {
    auto depthFormat = ctx.findDepthFormat();
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    ctx.createImage(ctx.swapchainExtent().width, ctx.swapchainExtent().height,
                    depthFormat, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (VulkanContext::hasStencilComponent(depthFormat))
        aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    VkImageView view = ctx.createImageView(image, depthFormat, aspectFlags);
    outDepth = UniqueImage(ctx.device(), image, memory, view);
}

void RenderPass::createFramebuffers(VulkanContext& ctx,
                                     VkImageView depthView) {
    const auto& swapchainViews = ctx.swapchainImageViews();
    swapchainFramebuffers_.resize(swapchainViews.size());
    for (std::size_t i = 0; i < swapchainViews.size(); ++i) {
        std::array<VkImageView, 2> attachments = {swapchainViews[i], depthView};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = ctx.swapchainExtent().width;
        framebufferInfo.height = ctx.swapchainExtent().height;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(ctx.device(), &framebufferInfo, nullptr,
                                 &swapchainFramebuffers_[i]) != VK_SUCCESS)
            throw std::runtime_error("RenderPass: vkCreateFramebuffer failed");
    }
}

void RenderPass::createDescriptorPool(
    VulkanContext& ctx, const std::vector<VkDescriptorPoolSize>& poolSizes,
    VkDescriptorPool& outPool) {
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = kMaxFramesInFlight;
    if (vkCreateDescriptorPool(ctx.device(), &poolInfo, nullptr, &outPool) !=
        VK_SUCCESS)
        throw std::runtime_error("RenderPass: vkCreateDescriptorPool failed");
}

void RenderPass::allocateDescriptorSets(
    VkDescriptorSetLayout layout, VkDescriptorPool pool,
    std::array<VkDescriptorSet, kMaxFramesInFlight>& outSets) {
    std::array<VkDescriptorSetLayout, kMaxFramesInFlight> layouts{};
    layouts.fill(layout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = kMaxFramesInFlight;
    allocInfo.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(ctx_->device(), &allocInfo, outSets.data()) !=
        VK_SUCCESS)
        throw std::runtime_error("RenderPass: vkAllocateDescriptorSets failed");
}

void RenderPass::createDefaultTexture(VulkanContext& ctx,
                                       UniqueImage& outImage,
                                       UniqueSampler& outSampler) {
    constexpr uint32_t kWidth = 256, kHeight = 256, kCheckerSize = 32;
    std::vector<uint8_t> pixels(kWidth * kHeight * 4);
    for (uint32_t y = 0; y < kHeight; ++y)
        for (uint32_t x = 0; x < kWidth; ++x) {
            bool white = ((x / kCheckerSize) + (y / kCheckerSize)) % 2 == 0;
            uint8_t value = white ? 220 : 60;
            size_t idx = (y * kWidth + x) * 4;
            pixels[idx + 0] = value;
            pixels[idx + 1] = value;
            pixels[idx + 2] = value;
            pixels[idx + 3] = 255;
        }

    VkDeviceSize imageByteSize = kWidth * kHeight * 4;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    ctx.createBuffer(imageByteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);
    void* mappedData = nullptr;
    vkMapMemory(ctx.device(), stagingMemory, 0, imageByteSize, 0, &mappedData);
    std::memcpy(mappedData, pixels.data(), static_cast<std::size_t>(imageByteSize));
    vkUnmapMemory(ctx.device(), stagingMemory);

    VkImage rawImage = VK_NULL_HANDLE;
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {kWidth, kHeight, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateImage(ctx.device(), &imageInfo, nullptr, &rawImage);

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(ctx.device(), rawImage, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex =
        ctx.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory rawMemory = VK_NULL_HANDLE;
    vkAllocateMemory(ctx.device(), &allocInfo, nullptr, &rawMemory);
    vkBindImageMemory(ctx.device(), rawImage, rawMemory, 0);

    ctx.executeOneShot([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = rawImage;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy copyRegion{};
        copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.imageExtent = {kWidth, kHeight, 1};
        vkCmdCopyBufferToImage(cmd, stagingBuffer, rawImage,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &barrier);
    });

    vkDestroyBuffer(ctx.device(), stagingBuffer, nullptr);
    vkFreeMemory(ctx.device(), stagingMemory, nullptr);

    VkImageView rawView = ctx.createImageView(rawImage, VK_FORMAT_R8G8B8A8_SRGB,
                                                VK_IMAGE_ASPECT_COLOR_BIT);
    VkSampler rawSampler = VK_NULL_HANDLE;
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(ctx.device(), &samplerInfo, nullptr, &rawSampler);

    outImage = UniqueImage(ctx.device(), rawImage, rawMemory, rawView);
    outSampler = UniqueSampler(rawSampler, DeleterSampler, ctx.device());
}

}  // namespace vr
