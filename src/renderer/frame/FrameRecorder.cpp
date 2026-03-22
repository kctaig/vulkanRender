/**
 * @file FrameRecorder.cpp
 * @brief Implementation for the FrameRecorder module.
 */
#include "renderer/frame/FrameRecorder.h"

#include <array>

#include <backends/imgui_impl_vulkan.h>

namespace vr {

void FrameRecorder::recordScenePass(VkCommandBuffer commandBuffer, const ScenePassContext& context) const {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = context.renderPass;
    renderPassInfo.framebuffer = context.framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = context.extent;
    renderPassInfo.clearValueCount = static_cast<std::uint32_t>(context.clearValues.size());
    renderPassInfo.pClearValues = context.clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.geometryPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(context.extent.width);
    viewport.height = static_cast<float>(context.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = context.extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = {context.vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, context.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        context.geometryPipelineLayout,
        0,
        1,
        &context.geometryDescriptorSet,
        0,
        nullptr
    );
    vkCmdDrawIndexed(commandBuffer, context.indexCount, 1, 0, 0, 0);

    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.lightingPipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        context.lightingPipelineLayout,
        0,
        1,
        &context.lightingDescriptorSet,
        0,
        nullptr
    );
    vkCmdPushConstants(
        commandBuffer,
        context.lightingPipelineLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        context.lightingPushConstantsSize,
        context.lightingPushConstants
    );
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
}

void FrameRecorder::recordUiPass(VkCommandBuffer commandBuffer, const UiPassContext& context) const {
    VkRenderPassBeginInfo uiRenderPassInfo{};
    uiRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    uiRenderPassInfo.renderPass = context.renderPass;
    uiRenderPassInfo.framebuffer = context.framebuffer;
    uiRenderPassInfo.renderArea.offset = {0, 0};
    uiRenderPassInfo.renderArea.extent = context.extent;
    uiRenderPassInfo.clearValueCount = 1;
    uiRenderPassInfo.pClearValues = &context.clearValue;

    vkCmdBeginRenderPass(commandBuffer, &uiRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(context.drawData, commandBuffer);
    vkCmdEndRenderPass(commandBuffer);
}

} // namespace vr


