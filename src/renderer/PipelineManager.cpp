#include "renderer/PipelineManager.h"

namespace vr {

void PipelineManager::registerGraphicsPipeline(const std::string& passName, const std::string& shaderTag) {
    graphicsPipelines_[passName] = shaderTag;
}

bool PipelineManager::hasPipeline(const std::string& passName) const {
    return graphicsPipelines_.contains(passName);
}

void PipelineManager::clear() {
    graphicsPipelines_.clear();
}

} // namespace vr
