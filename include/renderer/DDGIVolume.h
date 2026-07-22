#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <string>

#include "core/VulkanResource.h"

namespace vr {

class VulkanContext;
class Scene;

/// Configuration for one DDGI probe volume.
struct DDGIConfig {
    glm::ivec3 probeCounts  = {16, 8, 16};   // X, Y, Z probes
    float     probeSpacing  = 2.0f;            // world-space distance
    int       raysPerProbe  = 128;             // rays per probe per frame
    int       irradianceRes = 8;               // octahedral texel width per probe
    int       depthRes      = 8;               // octahedral texel width for depth
    float     hysteresis    = 0.98f;           // temporal blend weight
    float     depthSharpness = 50.0f;          // visibility test sharpness
    float     infDistance   = 1000.0f;         // max ray distance
};

/// Manages the probe grid, octahedral-map atlas textures, and history buffers.
class DDGIVolume {
  public:
    bool initialize(VulkanContext& ctx, const DDGIConfig& cfg, const Scene& scene);
    void scroll(const glm::vec3& cameraPos);
    void swapHistory();
    void shutdown(VkDevice device);

    // --- Accessors ---
    [[nodiscard]] VkImageView  irradianceView()        const;
    [[nodiscard]] VkImageView  depthView()             const;
    [[nodiscard]] VkImageView  irradianceHistoryView() const;
    [[nodiscard]] VkImageView  depthHistoryView()      const;
    [[nodiscard]] VkBuffer      configBuffer()          const { return configBuffer_; }
    [[nodiscard]] VkSampler     atlasSampler()          const { return atlasSampler_; }
    [[nodiscard]] const DDGIConfig& config()            const { return config_; }
    [[nodiscard]] int           totalProbes()           const;
    [[nodiscard]] VkExtent2D    atlasExtent()           const;
    [[nodiscard]] const glm::vec3& gridOrigin()         const { return gridOrigin_; }
    [[nodiscard]] glm::ivec3    scrollOffset()          const { return scrollOffset_; }

  private:
    void createAtlas(VulkanContext& ctx);
    void createSampler(VulkanContext& ctx);

    DDGIConfig   config_;
    glm::vec3    gridOrigin_{0.0f};
    glm::ivec3   scrollOffset_{0};

    // Current frame (written by trace shader, blended by blend shader)
    UniqueImage  irradianceAtlas_;       // RGBA16F
    UniqueImage  depthAtlas_;            // RG16F
    // History frame (read in blend shader, swapped each frame)
    UniqueImage  irradianceHistory_;
    UniqueImage  depthHistory_;

    UniqueBuffer configBuffer_;          // DDGIConfig UBO
    UniqueSampler atlasSampler_;         // LINEAR + CLAMP_TO_EDGE
};

}  // namespace vr
