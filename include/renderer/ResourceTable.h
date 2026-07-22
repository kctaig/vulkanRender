#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <unordered_map>

namespace vr {

/// Named resource table for sharing images/samplers between passes.
class ResourceTable {
  public:
    struct ImageEntry {
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    void set(const std::string& name, VkImageView view, VkFormat fmt,
             VkExtent2D extent, VkImageLayout layout) {
        images_[name] = ImageEntry{view, fmt, extent, layout};
    }

    void setSampler(const std::string& name, VkSampler sampler) {
        samplers_[name] = sampler;
    }

    void setDefaultSampler(VkSampler sampler) { defaultSampler_ = sampler; }

    [[nodiscard]] const ImageEntry* get(const std::string& name) const {
        auto it = images_.find(name);
        return it != images_.end() ? &it->second : nullptr;
    }

    [[nodiscard]] VkSampler sampler(const std::string& name) const {
        auto it = samplers_.find(name);
        return it != samplers_.end() ? it->second : defaultSampler_;
    }

    [[nodiscard]] VkSampler defaultSampler() const { return defaultSampler_; }

    void clearSwapchainResources() {
        images_.clear();
    }

  private:
    std::unordered_map<std::string, ImageEntry> images_;
    std::unordered_map<std::string, VkSampler> samplers_;
    VkSampler defaultSampler_ = VK_NULL_HANDLE;
};

}  // namespace vr
