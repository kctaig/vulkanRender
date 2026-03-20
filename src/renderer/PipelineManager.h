#pragma once

#include <string>
#include <unordered_map>

namespace vr {

class PipelineManager {
public:
    void registerGraphicsPipeline(const std::string& passName, const std::string& shaderTag);
    [[nodiscard]] bool hasPipeline(const std::string& passName) const;
    void clear();

private:
    std::unordered_map<std::string, std::string> graphicsPipelines_;
};

} // namespace vr
