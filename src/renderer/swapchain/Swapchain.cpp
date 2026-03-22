/**
 * @file Swapchain.cpp
 * @brief Implementation for the Swapchain module.
 */
#include "renderer/swapchain/Swapchain.h"

#include <stdexcept>

namespace vr {

std::uint32_t Swapchain::resolveImageCount(const VkSurfaceCapabilitiesKHR& capabilities) {
    std::uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    return imageCount;
}

void Swapchain::create(CreateContext& context) const {
    VkSurfaceFormatKHR surfaceFormat = context.chooseSurfaceFormat(context.support.formats);
    VkPresentModeKHR presentMode = context.choosePresentMode(context.support.presentModes);
    VkExtent2D extent = context.chooseExtent(context.support.capabilities);

    std::uint32_t imageCount = resolveImageCount(context.support.capabilities);
    context.swapchainMinImageCount = context.support.capabilities.minImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = context.surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    std::uint32_t queueFamilyIndices[] = {
        context.queueFamilies.graphicsFamily,
        context.queueFamilies.presentFamily,
    };

    if (context.queueFamilies.graphicsFamily != context.queueFamilies.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = context.support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(context.device, &createInfo, nullptr, &context.swapchain) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateSwapchainKHR failed");
    }

    vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, nullptr);
    context.swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, context.swapchainImages.data());

    context.swapchainImageFormat = surfaceFormat.format;
    context.swapchainExtent = extent;
}

void Swapchain::createImageViews(ImageViewsContext& context) const {
    context.swapchainImageViews.resize(context.swapchainImages.size());

    for (std::size_t i = 0; i < context.swapchainImages.size(); ++i) {
        context.swapchainImageViews[i] = context.createImageView(
            context.swapchainImages[i],
            context.swapchainImageFormat,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    }
}

} // namespace vr


