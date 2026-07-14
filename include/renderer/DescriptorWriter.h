#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

namespace vr {

/// Chainable descriptor set allocator + writer.
///
/// Usage:
///   DescriptorWriter(ctx.device())
///       .bindBuffer(0, uniformBuffer, sizeof(UBO))
///       .bindSampler(1, imageView, sampler)
///       .build(layout, pool, &descriptorSet);
class DescriptorWriter {
  public:
    explicit DescriptorWriter(VkDevice device) : device_(device) {}

    DescriptorWriter& bindBuffer(std::uint32_t binding, VkBuffer buffer,
                                  VkDeviceSize range,
                                  VkDeviceSize offset = 0) {
        VkDescriptorBufferInfo info{buffer, offset, range};
        bufferInfos_.push_back(info);
        writes_.push_back({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                            VK_NULL_HANDLE, binding, 0, 1,
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr,
                            &bufferInfos_.back(), nullptr});
        return *this;
    }

    DescriptorWriter& bindSampler(std::uint32_t binding, VkImageView view,
                                   VkSampler sampler) {
        VkDescriptorImageInfo info{sampler, view,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        imageInfos_.push_back(info);
        writes_.push_back({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                            VK_NULL_HANDLE, binding, 0, 1,
                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            &imageInfos_.back(), nullptr, nullptr});
        return *this;
    }

    void build(VkDescriptorSetLayout layout, VkDescriptorPool pool,
               VkDescriptorSet* outSet) {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &layout;
        vkAllocateDescriptorSets(device_, &ai, outSet);

        for (auto& w : writes_) w.dstSet = *outSet;
        vkUpdateDescriptorSets(device_,
                                static_cast<std::uint32_t>(writes_.size()),
                                writes_.data(), 0, nullptr);
    }

  private:
    VkDevice device_;
    std::vector<VkDescriptorBufferInfo> bufferInfos_;
    std::vector<VkDescriptorImageInfo> imageInfos_;
    std::vector<VkWriteDescriptorSet> writes_;
};

}  // namespace vr
