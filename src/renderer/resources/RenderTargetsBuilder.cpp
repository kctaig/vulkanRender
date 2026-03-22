/**
 * @file RenderTargetsBuilder.cpp
 * @brief Implementation for the RenderTargetsBuilder module.
 */
#include "renderer/resources/RenderTargetsBuilder.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace vr {

void RenderTargetsBuilder::createDepthResources(DepthContext& context) const {
    context.depthFormat = context.findDepthFormat();

    context.createImage(
        context.extent.width,
        context.extent.height,
        context.depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        context.depthImage,
        context.depthImageMemory
    );

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (context.hasStencilComponent(context.depthFormat)) {
        aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    context.depthImageView = context.createImageView(context.depthImage, context.depthFormat, aspectFlags);
}

void RenderTargetsBuilder::createGBufferResources(GBufferContext& context) const {
    context.createImage(
        context.extent.width,
        context.extent.height,
        context.swapchainImageFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        context.sceneColorImage,
        context.sceneColorImageMemory
    );
    context.sceneColorImageView =
        context.createImageView(context.sceneColorImage, context.swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    if (vkCreateSampler(context.device, &samplerInfo, nullptr, &context.sceneColorSampler) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateSampler for scene color failed");
    }

    context.createImage(
        context.extent.width,
        context.extent.height,
        context.gbufferPositionFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        context.gbufferPositionImage,
        context.gbufferPositionImageMemory
    );
    context.gbufferPositionImageView =
        context.createImageView(context.gbufferPositionImage, context.gbufferPositionFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    context.createImage(
        context.extent.width,
        context.extent.height,
        context.gbufferNormalFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        context.gbufferNormalImage,
        context.gbufferNormalImageMemory
    );
    context.gbufferNormalImageView =
        context.createImageView(context.gbufferNormalImage, context.gbufferNormalFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    context.createImage(
        context.extent.width,
        context.extent.height,
        context.gbufferAlbedoFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        context.gbufferAlbedoImage,
        context.gbufferAlbedoImageMemory
    );
    context.gbufferAlbedoImageView =
        context.createImageView(context.gbufferAlbedoImage, context.gbufferAlbedoFormat, VK_IMAGE_ASPECT_COLOR_BIT);
}

void RenderTargetsBuilder::createFramebuffers(FramebufferContext& context) const {
    {
        std::array<VkImageView, 5> sceneAttachments = {
            context.sceneColorImageView,
            context.depthImageView,
            context.gbufferPositionImageView,
            context.gbufferNormalImageView,
            context.gbufferAlbedoImageView,
        };

        VkFramebufferCreateInfo sceneFramebufferCreateInfo{};
        sceneFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        sceneFramebufferCreateInfo.renderPass = context.renderPass;
        sceneFramebufferCreateInfo.attachmentCount = static_cast<std::uint32_t>(sceneAttachments.size());
        sceneFramebufferCreateInfo.pAttachments = sceneAttachments.data();
        sceneFramebufferCreateInfo.width = context.extent.width;
        sceneFramebufferCreateInfo.height = context.extent.height;
        sceneFramebufferCreateInfo.layers = 1;

        if (vkCreateFramebuffer(context.device, &sceneFramebufferCreateInfo, nullptr, &context.sceneFramebuffer) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateFramebuffer for scene failed");
        }
    }

    context.swapchainFramebuffers.resize(context.swapchainImageViews.size());
    for (std::size_t i = 0; i < context.swapchainImageViews.size(); ++i) {
        VkImageView uiAttachment = context.swapchainImageViews[i];

        VkFramebufferCreateInfo uiFramebufferCreateInfo{};
        uiFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        uiFramebufferCreateInfo.renderPass = context.uiRenderPass;
        uiFramebufferCreateInfo.attachmentCount = 1;
        uiFramebufferCreateInfo.pAttachments = &uiAttachment;
        uiFramebufferCreateInfo.width = context.extent.width;
        uiFramebufferCreateInfo.height = context.extent.height;
        uiFramebufferCreateInfo.layers = 1;

        if (vkCreateFramebuffer(context.device, &uiFramebufferCreateInfo, nullptr, &context.swapchainFramebuffers[i]) !=
            VK_SUCCESS) {
            throw std::runtime_error("vkCreateFramebuffer for UI failed");
        }
    }
}

} // namespace vr


