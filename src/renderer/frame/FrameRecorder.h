/**
 * @file FrameRecorder.h
 * @brief Declarations for the FrameRecorder module.
 */
#pragma once

#include <array>
#include <cstdint>

#include <imgui.h>
#include <vulkan/vulkan.h>

namespace vr {

class FrameRecorder {
public:
    struct ScenePassContext {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkExtent2D extent{};
        std::array<VkClearValue, 5> clearValues{};

        VkPipeline geometryPipeline = VK_NULL_HANDLE;
        VkPipelineLayout geometryPipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSet geometryDescriptorSet = VK_NULL_HANDLE;
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        std::uint32_t indexCount = 0;

        VkPipeline lightingPipeline = VK_NULL_HANDLE;
        VkPipelineLayout lightingPipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSet lightingDescriptorSet = VK_NULL_HANDLE;
        const void* lightingPushConstants = nullptr;
        std::uint32_t lightingPushConstantsSize = 0;
    };

    struct UiPassContext {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkExtent2D extent{};
        VkClearValue clearValue{};
        ImDrawData* drawData = nullptr;
    };

    void recordScenePass(VkCommandBuffer commandBuffer, const ScenePassContext& context) const;
    void recordUiPass(VkCommandBuffer commandBuffer, const UiPassContext& context) const;
};

} // namespace vr


