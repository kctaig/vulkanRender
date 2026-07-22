#include "renderer/AccelerationStructure.h"

#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "core/VulkanContext.h"
#include "scene/Scene.h"

namespace vr {

void AccelerationStructureBuilder::buildBLAS(VulkanContext& ctx, const Scene& scene) {
    if (!ctx.rayTracingAvailable()) return;

    uint32_t meshCount = scene.assets.meshCount();
    blases_.resize(meshCount);

    for (uint32_t i = 0; i < meshCount; ++i) {
        const auto& mesh = scene.assets.mesh(i);

        VkAccelerationStructureGeometryKHR geom{};
        geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geom.geometry.triangles.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geom.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geom.geometry.triangles.vertexData.deviceAddress =
            ctx.getBufferDeviceAddress(mesh.vertexBuffer);
        geom.geometry.triangles.vertexStride = sizeof(float) * 8;  // pos(3)+norm(3)+uv(2)
        geom.geometry.triangles.maxVertex = mesh.vertexCount - 1;
        geom.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        geom.geometry.triangles.indexData.deviceAddress =
            ctx.getBufferDeviceAddress(mesh.indexBuffer);

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geom;

        uint32_t primitiveCount = mesh.indexCount / 3;

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        ctx.vkGetAccelerationStructureBuildSizesKHR(
            ctx.device(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo, &primitiveCount, &sizeInfo);

        // Create BLAS buffer
        VkBuffer asBuf; VkDeviceMemory asMem;
        ctx.createBuffer(sizeInfo.accelerationStructureSize,
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, asBuf, asMem);

        VkAccelerationStructureCreateInfoKHR asCI{};
        asCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        asCI.buffer = asBuf;
        asCI.size = sizeInfo.accelerationStructureSize;
        asCI.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        VkAccelerationStructureKHR asHandle;
        ctx.vkCreateAccelerationStructureKHR(ctx.device(), &asCI, nullptr, &asHandle);

        // Scratch buffer for build
        VkBuffer scratchBuf; VkDeviceMemory scratchMem;
        ctx.createBuffer(sizeInfo.buildScratchSize,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuf, scratchMem);

        buildInfo.dstAccelerationStructure = asHandle;
        buildInfo.scratchData.deviceAddress = ctx.getBufferDeviceAddress(scratchBuf);

        ctx.executeOneShot([&](VkCommandBuffer cmd) {
            VkAccelerationStructureBuildRangeInfoKHR range{};
            range.primitiveCount = primitiveCount;
            range.primitiveOffset = 0;
            range.firstVertex = 0;
            range.transformOffset = 0;
            const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = {&range};
            ctx.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ranges);
        });

        // Clean up scratch
        vkDestroyBuffer(ctx.device(), scratchBuf, nullptr);
        vkFreeMemory(ctx.device(), scratchMem, nullptr);

        blases_[i].handle = asHandle;
        blases_[i].buffer = UniqueBuffer(ctx.device(), asBuf, asMem);
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addrInfo.accelerationStructure = asHandle;
        blases_[i].deviceAddress =
            ctx.vkGetAccelerationStructureDeviceAddressKHR(ctx.device(), &addrInfo);
    }

    std::cout << "[AccelStruct] Built " << blases_.size() << " BLAS(es)\n";
}

void AccelerationStructureBuilder::buildTLAS(VulkanContext& ctx, const Scene& scene) {
    if (!ctx.rayTracingAvailable() || blases_.empty()) return;

    // Collect instance data
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    for (size_t i = 0; i < scene.instances.size() && i < blases_.size(); ++i) {
        const auto& inst = scene.instances[i];
        uint32_t blasIdx = static_cast<uint32_t>(i % blases_.size());

        VkAccelerationStructureInstanceKHR asInst{};
        asInst.instanceCustomIndex = static_cast<uint32_t>(i);
        asInst.mask = 0xFF;
        asInst.instanceShaderBindingTableRecordOffset = 0;
        asInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        asInst.accelerationStructureReference = blases_[blasIdx].deviceAddress;

        // Write the 3x4 row-major transform matrix
        for (int col = 0; col < 3; ++col)
            for (int row = 0; row < 4; ++row)
                (&asInst.transform.matrix[0][0])[col * 4 + row] = inst.transform[row][col];

        instances.push_back(asInst);
    }

    if (instances.empty()) return;

    // Upload instance data
    VkDeviceSize instSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    VkBuffer instBuf; VkDeviceMemory instMem;
    ctx.createBuffer(instSize,
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     instBuf, instMem);
    void* mapped = nullptr;
    vkMapMemory(ctx.device(), instMem, 0, instSize, 0, &mapped);
    std::memcpy(mapped, instances.data(), static_cast<size_t>(instSize));
    vkUnmapMemory(ctx.device(), instMem);

    VkAccelerationStructureGeometryKHR geom{};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geom.geometry.instances.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geom.geometry.instances.data.deviceAddress = ctx.getBufferDeviceAddress(instBuf);

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    uint32_t instCount = static_cast<uint32_t>(instances.size());

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    ctx.vkGetAccelerationStructureBuildSizesKHR(
        ctx.device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &instCount, &sizeInfo);

    // Destroy old TLAS if exists
    if (tlas_ != VK_NULL_HANDLE) {
        ctx.vkDestroyAccelerationStructureKHR(ctx.device(), tlas_, nullptr);
        tlas_ = VK_NULL_HANDLE;
    }
    tlasBuffer_.reset();
    instanceBuffer_ = UniqueBuffer(ctx.device(), instBuf, instMem);

    // Create TLAS buffer
    VkBuffer asBuf; VkDeviceMemory asMem;
    ctx.createBuffer(sizeInfo.accelerationStructureSize,
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, asBuf, asMem);
    tlasBuffer_ = UniqueBuffer(ctx.device(), asBuf, asMem);

    VkAccelerationStructureCreateInfoKHR asCI{};
    asCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asCI.buffer = asBuf;
    asCI.size = sizeInfo.accelerationStructureSize;
    asCI.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    ctx.vkCreateAccelerationStructureKHR(ctx.device(), &asCI, nullptr, &tlas_);

    // Scratch
    VkBuffer scratchBuf; VkDeviceMemory scratchMem;
    ctx.createBuffer(sizeInfo.buildScratchSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuf, scratchMem);

    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = tlas_;
    buildInfo.scratchData.deviceAddress = ctx.getBufferDeviceAddress(scratchBuf);

    ctx.executeOneShot([&](VkCommandBuffer cmd) {
        VkAccelerationStructureBuildRangeInfoKHR range{};
        range.primitiveCount = instCount;
        const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = {&range};
        ctx.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ranges);
    });

    vkDestroyBuffer(ctx.device(), scratchBuf, nullptr);
    vkFreeMemory(ctx.device(), scratchMem, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = tlas_;
    tlasAddress_ = ctx.vkGetAccelerationStructureDeviceAddressKHR(ctx.device(), &addrInfo);
}

void AccelerationStructureBuilder::shutdown(VulkanContext& ctx) {
    for (auto& blas : blases_) {
        if (blas.handle) {
            ctx.vkDestroyAccelerationStructureKHR(ctx.device(), blas.handle, nullptr);
        }
    }
    blases_.clear();
    if (tlas_ != VK_NULL_HANDLE) {
        ctx.vkDestroyAccelerationStructureKHR(ctx.device(), tlas_, nullptr);
        tlas_ = VK_NULL_HANDLE;
    }
    tlasBuffer_.reset();
    instanceBuffer_.reset();
}

}  // namespace vr
