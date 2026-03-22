/**
 * @file DescriptorBuilder.cpp
 * @brief Implementation for the DescriptorBuilder module.
 */
#include "renderer/descriptor/DescriptorBuilder.h"

#include <array>
#include <stdexcept>
#include <vector>

namespace vr {

void DescriptorBuilder::createSetLayouts(SetLayoutsContext& context) const {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;
    if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &context.descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorSetLayout failed");
    }

    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(bindings.size()); ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo lightingLayoutInfo{};
    lightingLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    lightingLayoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    lightingLayoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(context.device, &lightingLayoutInfo, nullptr, &context.lightingDescriptorSetLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorSetLayout for lighting failed");
    }
}

void DescriptorBuilder::createDescriptorPool(DescriptorPoolContext& context) const {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = context.maxFramesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = context.maxFramesInFlight;

    if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &context.descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorPool failed");
    }
}

void DescriptorBuilder::createDescriptorSets(DescriptorSetsContext& context) const {
    std::vector<VkDescriptorSetLayout> layouts(context.maxFramesInFlight, context.descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = context.descriptorPool;
    allocInfo.descriptorSetCount = context.maxFramesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(context.device, &allocInfo, context.descriptorSets) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateDescriptorSets failed");
    }

    for (std::uint32_t i = 0; i < context.maxFramesInFlight; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = context.uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = context.uniformBufferRange;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = context.descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);
    }
}

void DescriptorBuilder::createLightingDescriptorPool(LightingDescriptorPoolContext& context) const {
    std::array<VkDescriptorPoolSize, 1> poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 3},
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &context.lightingDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorPool for lighting failed");
    }
}

void DescriptorBuilder::createLightingDescriptorSet(LightingDescriptorSetContext& context) const {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = context.lightingDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &context.lightingDescriptorSetLayout;

    if (vkAllocateDescriptorSets(context.device, &allocInfo, &context.lightingDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateDescriptorSets for lighting failed");
    }
}

void DescriptorBuilder::updateLightingDescriptorSet(UpdateLightingDescriptorSetContext& context) const {
    std::array<VkDescriptorImageInfo, 3> imageInfos{};
    imageInfos[0].imageView = context.gbufferPositionImageView;
    imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[1].imageView = context.gbufferNormalImageView;
    imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[2].imageView = context.gbufferAlbedoImageView;
    imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 3> writes{};
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(writes.size()); ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = context.lightingDescriptorSet;
        writes[i].dstBinding = i;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writes[i].descriptorCount = 1;
        writes[i].pImageInfo = &imageInfos[i];
    }

    vkUpdateDescriptorSets(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

} // namespace vr


