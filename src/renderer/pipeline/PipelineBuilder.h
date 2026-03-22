/**
 * @file PipelineBuilder.h
 * @brief Declarations for the PipelineBuilder module.
 */
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace vr {

class PipelineBuilder {
public:
    /** @brief Inputs/outputs required to create render passes for scene and UI. */
    struct RenderPassContext {
        VkDevice device = VK_NULL_HANDLE;

        VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
        VkFormat gbufferPositionFormat = VK_FORMAT_UNDEFINED;
        VkFormat gbufferNormalFormat = VK_FORMAT_UNDEFINED;
        VkFormat gbufferAlbedoFormat = VK_FORMAT_UNDEFINED;

        VkFormat& depthFormat;
        VkRenderPass& renderPass;
        VkRenderPass& uiRenderPass;

        std::function<VkFormat()> findDepthFormat;
    };

    /** @brief Inputs/outputs required to build the geometry graphics pipeline. */
    struct GeometryPipelineContext {
        VkDevice device = VK_NULL_HANDLE;
        std::string shaderRoot;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;

        VkPipelineLayout& pipelineLayout;
        VkPipeline& graphicsPipeline;

        std::function<std::vector<char>(const char*)> readBinaryFile;
        std::function<VkShaderModule(const std::vector<char>&)> createShaderModule;
        std::function<VkVertexInputBindingDescription()> getBindingDescription;
        std::function<std::array<VkVertexInputAttributeDescription, 3>()> getAttributeDescriptions;
    };

    /** @brief Inputs/outputs required to build the deferred-lighting pipeline. */
    struct LightingPipelineContext {
        VkDevice device = VK_NULL_HANDLE;
        std::string shaderRoot;
        VkDescriptorSetLayout lightingDescriptorSetLayout = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;

        VkPipelineLayout& lightingPipelineLayout;
        VkPipeline& lightingPipeline;

        std::uint32_t pushConstantSize = 0;

        std::function<std::vector<char>(const char*)> readBinaryFile;
        std::function<VkShaderModule(const std::vector<char>&)> createShaderModule;
    };

    /** @brief Creates the scene render pass and UI render pass. */
    void createRenderPass(RenderPassContext& context) const;
    /** @brief Creates the geometry pipeline and its layout. */
    void createGraphicsPipeline(GeometryPipelineContext& context) const;
    /** @brief Creates the deferred-lighting pipeline and its layout. */
    void createLightingPipeline(LightingPipelineContext& context) const;
};

} // namespace vr


