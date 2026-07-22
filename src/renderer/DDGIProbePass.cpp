#include "renderer/DDGIProbePass.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include "core/VulkanContext.h"
#include "renderer/AccelerationStructure.h"
#include "renderer/DDGIVolume.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/PipelineBuilder.h"

namespace vr {

bool DDGIProbePass::initialize(VulkanContext& ctx) {
    if (!ctx.rayTracingAvailable()) {
        std::cerr << "[DDGIProbePass] Ray tracing not available, skipping\n";
        return true;  // graceful skip
    }
    ctx_ = &ctx;
    try {
        createTracePipeline(ctx);
        createBlendPipeline(ctx);
    } catch (const std::exception& e) {
        std::cerr << "[DDGIProbePass] Init failed: " << e.what() << "\n";
        return false;
    }
    return true;
}

void DDGIProbePass::record(VkCommandBuffer cmd, std::uint32_t frameIndex,
                            std::uint32_t imageIndex) {
    if (!volume_ || !tlas_ || !ctx_->rayTracingAvailable()) return;

    // --- Ray trace: dispatch one workgroup per probe ---
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tracePipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            tracePipelineLayout_, 0, 1,
                            &traceDescSets_[frameIndex], 0, nullptr);

    int totalProbes = volume_->totalProbes();
    int raysPerProbe = volume_->config().raysPerProbe;
    uint32_t workgroupsX = (raysPerProbe + 31) / 32;

    for (int i = 0; i < totalProbes; ++i) {
        uint32_t probeIdx = static_cast<uint32_t>(i);
        vkCmdPushConstants(cmd, tracePipelineLayout_,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, 8, &probeIdx);
        vkCmdDispatch(cmd, workgroupsX, 1, 1);
    }

    // --- Pipeline barrier: storage write → storage read (for blend) ---
    VkMemoryBarrier memBarrier{};
    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &memBarrier, 0, nullptr, 0, nullptr);

    // --- Temporal blend: dispatch per atlas texel ---
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, blendPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            blendPipelineLayout_, 0, 1,
                            &blendDescSets_[frameIndex], 0, nullptr);

    float hysteresis = volume_->config().hysteresis;
    vkCmdPushConstants(cmd, blendPipelineLayout_,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &hysteresis);

    auto atlasExt = volume_->atlasExtent();
    uint32_t gx = (atlasExt.width  + 7) / 8;
    uint32_t gy = (atlasExt.height + 7) / 8;
    vkCmdDispatch(cmd, gx, gy, 1);
}

void DDGIProbePass::onSwapchainResize(VulkanContext&) {}
void DDGIProbePass::shutdown() {
    if (traceDescPool_) vkDestroyDescriptorPool(ctx_->device(), traceDescPool_, nullptr);
    if (blendDescPool_) vkDestroyDescriptorPool(ctx_->device(), blendDescPool_, nullptr);
    ctx_ = nullptr;
}

// --- Trace pipeline ---

void DDGIProbePass::createTracePipeline(VulkanContext& ctx) {
    if (!volume_ || !tlas_) return;

    // Descriptor set layout: set=0 = TLAS, set=1 = images + config
    VkDescriptorSetLayoutBinding tlasBinding{};
    tlasBinding.binding = 0;
    tlasBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    tlasBinding.descriptorCount = 1;
    tlasBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo set0CI{};
    set0CI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set0CI.bindingCount = 1;
    set0CI.pBindings = &tlasBinding;
    VkDescriptorSetLayout set0Layout;
    vkCreateDescriptorSetLayout(ctx.device(), &set0CI, nullptr, &set0Layout);

    std::array<VkDescriptorSetLayoutBinding, 3> set1Bindings{};
    set1Bindings[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    set1Bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    set1Bindings[2] = {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo set1CI{};
    set1CI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set1CI.bindingCount = static_cast<uint32_t>(set1Bindings.size());
    set1CI.pBindings = set1Bindings.data();
    vkCreateDescriptorSetLayout(ctx.device(), &set1CI, nullptr,
                                 traceDescSetLayout_.put(ctx.device()));

    // Pipeline layout with 2 sets + push constants
    std::array<VkDescriptorSetLayout, 2> sets = {set0Layout, traceDescSetLayout_};
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.offset = 0;
    pc.size = 8;  // uint probeIndex + uint frameIndex

    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount = 2;
    plCI.pSetLayouts = sets.data();
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges = &pc;
    vkCreatePipelineLayout(ctx.device(), &plCI, nullptr,
                            tracePipelineLayout_.put(ctx.device()));

    // Pipeline
    std::string sd = VR_SHADER_DIR;
    PipelineBuilder pb(ctx);
    auto pipe = pb.computeShader(sd + "/ddgiTrace.comp.spv")
                     .buildCompute();
    tracePipeline_ = UniquePipeline(pipe, DeleterPipeline, ctx.device());
    // Retrieve layout from builder
    if (pb.pipelineLayout() != VK_NULL_HANDLE) {
        tracePipelineLayout_ = UniquePipelineLayout(pb.pipelineLayout(),
                                                     DeleterPipelineLayout, ctx.device());
    }
    pb.destroyShaderModules();

    vkDestroyDescriptorSetLayout(ctx.device(), set0Layout, nullptr);

    // Descriptor pool + sets
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, kMaxFramesInFlight},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kMaxFramesInFlight * 2},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxFramesInFlight},
    };
    VkDescriptorPoolCreateInfo dpCI{};
    dpCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpCI.maxSets = kMaxFramesInFlight * 3;
    dpCI.poolSizeCount = 3;
    dpCI.pPoolSizes = poolSizes;
    vkCreateDescriptorPool(ctx.device(), &dpCI, nullptr, &traceDescPool_);

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = traceDescPool_;
        ai.descriptorSetCount = 1;
        VkDescriptorSetLayout layout = traceDescSetLayout_.get();
        ai.pSetLayouts = &layout;
        vkAllocateDescriptorSets(ctx.device(), &ai, &traceDescSets_[i]);
    }

    // Write descriptors
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        DescriptorWriter dw(ctx.device());
        dw.bindStorageImage(0, volume_->irradianceView(), VK_IMAGE_LAYOUT_GENERAL)
          .bindStorageImage(1, volume_->depthView(), VK_IMAGE_LAYOUT_GENERAL)
          .bindBuffer(2, volume_->configBuffer(), sizeof(DDGIConfig))
          .build(traceDescSetLayout_, traceDescPool_, &traceDescSets_[i]);
    }
}

// --- Blend pipeline ---

void DDGIProbePass::createBlendPipeline(VulkanContext& ctx) {
    if (!volume_) return;

    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    bindings[2] = {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    bindings[3] = {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = static_cast<uint32_t>(bindings.size());
    ci.pBindings = bindings.data();
    vkCreateDescriptorSetLayout(ctx.device(), &ci, nullptr, blendDescSetLayout_.put(ctx.device()));

    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.offset = 0;
    pc.size = 4;  // float hysteresis

    VkDescriptorSetLayout blendLayout = blendDescSetLayout_.get();
    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount = 1;
    plCI.pSetLayouts = &blendLayout;
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges = &pc;
    vkCreatePipelineLayout(ctx.device(), &plCI, nullptr, blendPipelineLayout_.put(ctx.device()));

    std::string sd = VR_SHADER_DIR;
    PipelineBuilder pb(ctx);
    auto pipe = pb.computeShader(sd + "/ddgiBlend.comp.spv")
                     .buildCompute();
    blendPipeline_ = UniquePipeline(pipe, DeleterPipeline, ctx.device());

    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kMaxFramesInFlight * 2},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxFramesInFlight * 2},
    };
    VkDescriptorPoolCreateInfo dpCI{};
    dpCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpCI.maxSets = kMaxFramesInFlight;
    dpCI.poolSizeCount = 2;
    dpCI.pPoolSizes = poolSizes;
    vkCreateDescriptorPool(ctx.device(), &dpCI, nullptr, &blendDescPool_);

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = blendDescPool_;
        ai.descriptorSetCount = 1;
        VkDescriptorSetLayout bl = blendDescSetLayout_.get();
        ai.pSetLayouts = &bl;
        vkAllocateDescriptorSets(ctx.device(), &ai, &blendDescSets_[i]);
    }

    auto samp = volume_->atlasSampler();
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        DescriptorWriter dw(ctx.device());
        dw.bindStorageImage(0, volume_->irradianceView(), VK_IMAGE_LAYOUT_GENERAL)
          .bindStorageImage(1, volume_->depthView(), VK_IMAGE_LAYOUT_GENERAL)
          .bindSampler(2, volume_->irradianceHistoryView(), samp)
          .bindSampler(3, volume_->depthHistoryView(), samp)
          .build(blendDescSetLayout_, blendDescPool_, &blendDescSets_[i]);
    }
}

}  // namespace vr
