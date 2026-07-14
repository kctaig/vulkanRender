#pragma once

#include <vulkan/vulkan.h>

#include <utility>

namespace vr {

/// Generic move-only Vulkan handle with custom deleter.
template <typename Handle, typename Deleter>
class UniqueVkHandle {
  public:
    UniqueVkHandle() = default;
    explicit UniqueVkHandle(Handle h, Deleter d, VkDevice dev = VK_NULL_HANDLE)
        : handle_(h), deleter_(std::move(d)), device_(dev) {}

    ~UniqueVkHandle() {
        if (handle_ != VK_NULL_HANDLE) deleter_(device_, handle_);
    }

    UniqueVkHandle(const UniqueVkHandle&) = delete;
    UniqueVkHandle& operator=(const UniqueVkHandle&) = delete;

    UniqueVkHandle(UniqueVkHandle&& o) noexcept
        : handle_(o.handle_), deleter_(std::move(o.deleter_)), device_(o.device_) {
        o.handle_ = VK_NULL_HANDLE;
    }

    UniqueVkHandle& operator=(UniqueVkHandle&& o) noexcept {
        if (this != &o) {
            if (handle_ != VK_NULL_HANDLE) deleter_(device_, handle_);
            handle_ = o.handle_;
            deleter_ = std::move(o.deleter_);
            device_ = o.device_;
            o.handle_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    Handle get() const { return handle_; }
    operator Handle() const { return handle_; }
    Handle* put(VkDevice dev) {
        if (handle_ != VK_NULL_HANDLE) deleter_(device_, handle_);
        handle_ = VK_NULL_HANDLE;
        device_ = dev;
        return &handle_;
    }
    Handle* put() { return put(VK_NULL_HANDLE); }

    void reset(VkDevice dev = VK_NULL_HANDLE) {
        if (handle_ != VK_NULL_HANDLE) deleter_(device_, handle_);
        handle_ = VK_NULL_HANDLE;
        device_ = dev;
    }

  private:
    Handle handle_ = VK_NULL_HANDLE;
    Deleter deleter_{};
    VkDevice device_ = VK_NULL_HANDLE;
};

// --- Common deleters as lambdas ---
inline auto DeleterPipeline = [](VkDevice d, VkPipeline h) { vkDestroyPipeline(d, h, nullptr); };
inline auto DeleterPipelineLayout = [](VkDevice d, VkPipelineLayout h) {
    vkDestroyPipelineLayout(d, h, nullptr);
};
inline auto DeleterRenderPass = [](VkDevice d, VkRenderPass h) {
    vkDestroyRenderPass(d, h, nullptr);
};
inline auto DeleterFramebuffer = [](VkDevice d, VkFramebuffer h) {
    vkDestroyFramebuffer(d, h, nullptr);
};
inline auto DeleterDescriptorPool = [](VkDevice d, VkDescriptorPool h) {
    vkDestroyDescriptorPool(d, h, nullptr);
};
inline auto DeleterDescriptorSetLayout = [](VkDevice d, VkDescriptorSetLayout h) {
    vkDestroyDescriptorSetLayout(d, h, nullptr);
};
inline auto DeleterSampler = [](VkDevice d, VkSampler h) { vkDestroySampler(d, h, nullptr); };
inline auto DeleterShaderModule = [](VkDevice d, VkShaderModule h) {
    vkDestroyShaderModule(d, h, nullptr);
};
inline auto DeleterImageView = [](VkDevice d, VkImageView h) {
    vkDestroyImageView(d, h, nullptr);
};

// --- Type aliases ---
using UniquePipeline = UniqueVkHandle<VkPipeline, decltype(DeleterPipeline)>;
using UniquePipelineLayout =
    UniqueVkHandle<VkPipelineLayout, decltype(DeleterPipelineLayout)>;
using UniqueRenderPass =
    UniqueVkHandle<VkRenderPass, decltype(DeleterRenderPass)>;
using UniqueFramebuffer =
    UniqueVkHandle<VkFramebuffer, decltype(DeleterFramebuffer)>;
using UniqueDescriptorPool =
    UniqueVkHandle<VkDescriptorPool, decltype(DeleterDescriptorPool)>;
using UniqueDescriptorSetLayout =
    UniqueVkHandle<VkDescriptorSetLayout, decltype(DeleterDescriptorSetLayout)>;
using UniqueSampler = UniqueVkHandle<VkSampler, decltype(DeleterSampler)>;
using UniqueShaderModule =
    UniqueVkHandle<VkShaderModule, decltype(DeleterShaderModule)>;
using UniqueImageView =
    UniqueVkHandle<VkImageView, decltype(DeleterImageView)>;

// ================================================================
// Buffer (VkBuffer + VkDeviceMemory pair)
// ================================================================

class UniqueBuffer {
  public:
    UniqueBuffer() = default;
    UniqueBuffer(VkDevice d, VkBuffer b, VkDeviceMemory m)
        : device_(d), buffer_(b), memory_(m) {}

    ~UniqueBuffer() { destroy(); }

    UniqueBuffer(const UniqueBuffer&) = delete;
    UniqueBuffer& operator=(const UniqueBuffer&) = delete;

    UniqueBuffer(UniqueBuffer&& o) noexcept
        : device_(o.device_), buffer_(o.buffer_), memory_(o.memory_) {
        o.device_ = VK_NULL_HANDLE;
        o.buffer_ = VK_NULL_HANDLE;
        o.memory_ = VK_NULL_HANDLE;
    }

    UniqueBuffer& operator=(UniqueBuffer&& o) noexcept {
        if (this != &o) {
            destroy();
            device_ = o.device_;
            buffer_ = o.buffer_;
            memory_ = o.memory_;
            o.device_ = VK_NULL_HANDLE;
            o.buffer_ = VK_NULL_HANDLE;
            o.memory_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    VkBuffer get() const { return buffer_; }
    VkDeviceMemory memory() const { return memory_; }
    operator VkBuffer() const { return buffer_; }
    VkBuffer* put() { destroy(); return &buffer_; }
    void reset() { destroy(); }

  private:
    void destroy() {
        if (buffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, buffer_, nullptr);
            buffer_ = VK_NULL_HANDLE;
        }
        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_, memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }
    }

    VkDevice device_ = VK_NULL_HANDLE;
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
};

// ================================================================
// Image (VkImage + VkDeviceMemory + VkImageView trio)
// ================================================================

class UniqueImage {
  public:
    UniqueImage() = default;
    UniqueImage(VkDevice d, VkImage i, VkDeviceMemory m, VkImageView v)
        : device_(d), image_(i), memory_(m), view_(v) {}

    ~UniqueImage() { destroy(); }

    UniqueImage(const UniqueImage&) = delete;
    UniqueImage& operator=(const UniqueImage&) = delete;

    UniqueImage(UniqueImage&& o) noexcept
        : device_(o.device_), image_(o.image_), memory_(o.memory_), view_(o.view_) {
        o.device_ = VK_NULL_HANDLE;
        o.image_ = VK_NULL_HANDLE;
        o.memory_ = VK_NULL_HANDLE;
        o.view_ = VK_NULL_HANDLE;
    }

    UniqueImage& operator=(UniqueImage&& o) noexcept {
        if (this != &o) {
            destroy();
            device_ = o.device_;
            image_ = o.image_;
            memory_ = o.memory_;
            view_ = o.view_;
            o.device_ = VK_NULL_HANDLE;
            o.image_ = VK_NULL_HANDLE;
            o.memory_ = VK_NULL_HANDLE;
            o.view_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    VkImage get() const { return image_; }
    VkDeviceMemory memory() const { return memory_; }
    VkImageView view() const { return view_; }
    operator VkImage() const { return image_; }
    VkImage* put() { destroy(); return &image_; }
    void reset() { destroy(); }

  private:
    void destroy() {
        if (view_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, view_, nullptr);
            view_ = VK_NULL_HANDLE;
        }
        if (image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, image_, nullptr);
            image_ = VK_NULL_HANDLE;
        }
        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_, memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }
    }

    VkDevice device_ = VK_NULL_HANDLE;
    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
};

}  // namespace vr
