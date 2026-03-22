/**
 * @file DescriptorBuilder.h
 * @brief Declarations for the DescriptorBuilder module.
 */
#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace vr {

class DescriptorBuilder {
public:
    /** @brief Parameters required to build geometry and lighting descriptor set layouts. */
    struct SetLayoutsContext {
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSetLayout& descriptorSetLayout;
        VkDescriptorSetLayout& lightingDescriptorSetLayout;
    };

    /** @brief Parameters required to create the per-frame descriptor pool. */
    struct DescriptorPoolContext {
        VkDevice device = VK_NULL_HANDLE;
        std::uint32_t maxFramesInFlight = 0;
        VkDescriptorPool& descriptorPool;
    };

    /** @brief Parameters required to allocate and write per-frame descriptor sets. */
    struct DescriptorSetsContext {
        VkDevice device = VK_NULL_HANDLE;
        std::uint32_t maxFramesInFlight = 0;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkBuffer* uniformBuffers = nullptr;
        VkDescriptorSet* descriptorSets = nullptr;
        VkDeviceSize uniformBufferRange = 0;
    };

    /** @brief Parameters required to create the deferred-lighting descriptor pool. */
    struct LightingDescriptorPoolContext {
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool& lightingDescriptorPool;
    };

    /** @brief Parameters required to allocate the lighting descriptor set. */
    struct LightingDescriptorSetContext {
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool lightingDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout lightingDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet& lightingDescriptorSet;
    };

    /** @brief Parameters required to update GBuffer bindings for lighting. */
    struct UpdateLightingDescriptorSetContext {
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorSet lightingDescriptorSet = VK_NULL_HANDLE;
        VkImageView gbufferPositionImageView = VK_NULL_HANDLE;
        VkImageView gbufferNormalImageView = VK_NULL_HANDLE;
        VkImageView gbufferAlbedoImageView = VK_NULL_HANDLE;
    };

    /** @brief Creates descriptor set layouts used by geometry and lighting passes. */
    void createSetLayouts(SetLayoutsContext& context) const;
    /** @brief Creates the descriptor pool used by per-frame uniform descriptors. */
    void createDescriptorPool(DescriptorPoolContext& context) const;
    /** @brief Allocates and writes per-frame descriptor sets. */
    void createDescriptorSets(DescriptorSetsContext& context) const;
    /** @brief Creates descriptor pool used by the lighting pass. */
    void createLightingDescriptorPool(LightingDescriptorPoolContext& context) const;
    /** @brief Allocates lighting descriptor set from the lighting pool. */
    void createLightingDescriptorSet(LightingDescriptorSetContext& context) const;
    /** @brief Writes GBuffer image views into the lighting descriptor set. */
    void updateLightingDescriptorSet(UpdateLightingDescriptorSetContext& context) const;
};

} // namespace vr


