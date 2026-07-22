#pragma once

#include <vector>

#include "core/VulkanResource.h"

namespace vr {

class VulkanContext;
class Scene;

struct BLAS {
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    UniqueBuffer buffer;
    VkDeviceAddress deviceAddress = 0;
};

/// Builds BLAS (per-mesh) and TLAS (per-frame) for ray tracing.
class AccelerationStructureBuilder {
  public:
    void buildBLAS(VulkanContext& ctx, const Scene& scene);
    void buildTLAS(VulkanContext& ctx, const Scene& scene);
    void shutdown(VulkanContext& ctx);

    VkAccelerationStructureKHR tlas() const { return tlas_; }
    VkDeviceAddress tlasAddress() const { return tlasAddress_; }
    const std::vector<BLAS>& blases() const { return blases_; }

  private:
    std::vector<BLAS> blases_;
    VkAccelerationStructureKHR tlas_ = VK_NULL_HANDLE;
    UniqueBuffer tlasBuffer_;
    UniqueBuffer instanceBuffer_;
    VkDeviceAddress tlasAddress_ = 0;

    void createBLAS(VulkanContext& ctx,
                    VkAccelerationStructureGeometryKHR& geom,
                    uint32_t primitiveCount, VkDeviceSize& scratchSize);
    void createTLAS(VulkanContext& ctx,
                    VkAccelerationStructureGeometryKHR& geom,
                    uint32_t instanceCount, VkDeviceSize& scratchSize);
};

}  // namespace vr
