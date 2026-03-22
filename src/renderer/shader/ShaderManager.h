/**
 * @file ShaderManager.h
 * @brief Declarations for the ShaderManager module.
 */
#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vr {

class ShaderManager {
public:
    std::vector<char> readBinaryFile(const char* filePath) const;
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) const;
};

} // namespace vr


