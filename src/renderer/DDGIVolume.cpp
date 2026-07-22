#include "renderer/DDGIVolume.h"

#include <cmath>
#include <cstring>
#include <iostream>

#include "core/VulkanContext.h"
#include "scene/Scene.h"

namespace vr {

int DDGIVolume::totalProbes() const {
    return config_.probeCounts.x * config_.probeCounts.y * config_.probeCounts.z;
}

VkExtent2D DDGIVolume::atlasExtent() const {
    return {
        static_cast<uint32_t>(config_.probeCounts.x * config_.irradianceRes),
        static_cast<uint32_t>(config_.probeCounts.y * config_.probeCounts.z * config_.irradianceRes)
    };
}

bool DDGIVolume::initialize(VulkanContext& ctx, const DDGIConfig& cfg, const Scene& scene) {
    config_ = cfg;

    // Compute grid origin from scene bounds
    float margin = cfg.probeSpacing * 2.0f;
    float halfX = scene.modelRadius;
    float halfY = scene.modelRadius * 0.4f;
    float halfZ = scene.modelRadius;
    gridOrigin_ = glm::vec3(-halfX - margin, -halfY - margin, -halfZ - margin);

    scrollOffset_ = glm::ivec3(0);
    std::cout << "[DDGIVolume] " << totalProbes() << " probes, grid origin ("
              << gridOrigin_.x << ", " << gridOrigin_.y << ", " << gridOrigin_.z << ")\n";

    createAtlas(ctx);
    createSampler(ctx);

    // Upload config UBO
    struct GPUConfig {
        glm::ivec4 probeCounts; glm::vec4 gridOrigin;
        float spacing; float hysteresis; float sharpness; float infDist;
        int raysPerProbe; int irrRes; int depthRes; int pad;
    } gpuCfg;
    gpuCfg.probeCounts = glm::ivec4(config_.probeCounts, totalProbes());
    gpuCfg.gridOrigin  = glm::vec4(gridOrigin_, 0.0f);
    gpuCfg.spacing     = config_.probeSpacing;
    gpuCfg.hysteresis  = config_.hysteresis;
    gpuCfg.sharpness   = config_.depthSharpness;
    gpuCfg.infDist     = config_.infDistance;
    gpuCfg.raysPerProbe = config_.raysPerProbe;
    gpuCfg.irrRes      = config_.irradianceRes;
    gpuCfg.depthRes    = config_.depthRes;
    gpuCfg.pad         = 0;

    VkBuffer cfgBuf; VkDeviceMemory cfgMem;
    ctx.createBuffer(sizeof(gpuCfg),
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     cfgBuf, cfgMem);
    void* mappedCfg = nullptr;
    vkMapMemory(ctx.device(), cfgMem, 0, sizeof(gpuCfg), 0, &mappedCfg);
    std::memcpy(mappedCfg, &gpuCfg, sizeof(gpuCfg));
    vkUnmapMemory(ctx.device(), cfgMem);
    configBuffer_ = UniqueBuffer(ctx.device(), cfgBuf, cfgMem);

    return true;
}

void DDGIVolume::scroll(const glm::vec3& cameraPos) {
    glm::ivec3 targetCell = glm::ivec3(
        glm::floor((cameraPos - gridOrigin_) / config_.probeSpacing));
    glm::ivec3 delta = targetCell - scrollOffset_;
    if (delta != glm::ivec3(0)) {
        // For simplicity, fully reset origin on scroll (probes that wrap around
        // will have their history reset via the hysteresis blend naturally).
        gridOrigin_ += glm::vec3(delta) * config_.probeSpacing;
        scrollOffset_ = targetCell;
    }
}

void DDGIVolume::swapHistory() {
    std::swap(irradianceAtlas_, irradianceHistory_);
    std::swap(depthAtlas_, depthHistory_);
}

void DDGIVolume::shutdown(VkDevice device) {
    irradianceAtlas_.reset();
    depthAtlas_.reset();
    irradianceHistory_.reset();
    depthHistory_.reset();
    configBuffer_.reset();
    atlasSampler_.reset();
}

VkImageView DDGIVolume::irradianceView()        const { return irradianceAtlas_.view(); }
VkImageView DDGIVolume::depthView()             const { return depthAtlas_.view(); }
VkImageView DDGIVolume::irradianceHistoryView() const { return irradianceHistory_.view(); }
VkImageView DDGIVolume::depthHistoryView()      const { return depthHistory_.view(); }

// --- Private ---

void DDGIVolume::createAtlas(VulkanContext& ctx) {
    auto ext = atlasExtent();
    // Irradiance atlas (current + history)
    {
        VkImage img; VkDeviceMemory mem;
        ctx.createStorageImage(ext.width, ext.height, VK_FORMAT_R16G16B16A16_SFLOAT,
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT, img, mem);
        VkImageView view = ctx.createImageView(img, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                VK_IMAGE_ASPECT_COLOR_BIT);
        irradianceAtlas_ = UniqueImage(ctx.device(), img, mem, view);

        VkImage img2; VkDeviceMemory mem2;
        ctx.createStorageImage(ext.width, ext.height, VK_FORMAT_R16G16B16A16_SFLOAT,
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT, img2, mem2);
        VkImageView view2 = ctx.createImageView(img2, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                 VK_IMAGE_ASPECT_COLOR_BIT);
        irradianceHistory_ = UniqueImage(ctx.device(), img2, mem2, view2);
    }
    // Depth atlas (current + history)
    {
        VkImage img; VkDeviceMemory mem;
        ctx.createStorageImage(ext.width, ext.height, VK_FORMAT_R16G16_SFLOAT,
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT, img, mem);
        VkImageView view = ctx.createImageView(img, VK_FORMAT_R16G16_SFLOAT,
                                                VK_IMAGE_ASPECT_COLOR_BIT);
        depthAtlas_ = UniqueImage(ctx.device(), img, mem, view);

        VkImage img2; VkDeviceMemory mem2;
        ctx.createStorageImage(ext.width, ext.height, VK_FORMAT_R16G16_SFLOAT,
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT, img2, mem2);
        VkImageView view2 = ctx.createImageView(img2, VK_FORMAT_R16G16_SFLOAT,
                                                 VK_IMAGE_ASPECT_COLOR_BIT);
        depthHistory_ = UniqueImage(ctx.device(), img2, mem2, view2);
    }
}

void DDGIVolume::createSampler(VulkanContext& ctx) {
    VkSampler samp = VK_NULL_HANDLE;
    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(ctx.device(), &si, nullptr, &samp);
    atlasSampler_ = UniqueSampler(samp, DeleterSampler, ctx.device());
}

}  // namespace vr
