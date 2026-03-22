/**
 * @file ShaderManager.cpp
 * @brief Implementation for the ShaderManager module.
 */
#include "renderer/shader/ShaderManager.h"

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

namespace vr {

std::vector<char> ShaderManager::readBinaryFile(const char* filePath) const {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("failed to open shader file: ") + filePath);
    }

    std::streamsize fileSize = file.tellg();
    std::vector<char> buffer(static_cast<std::size_t>(fileSize));
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    return buffer;
}

VkShaderModule ShaderManager::createShaderModule(VkDevice device, const std::vector<char>& code) const {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateShaderModule failed");
    }

    return shaderModule;
}

} // namespace vr


