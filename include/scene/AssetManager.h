#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

namespace vr {

class VulkanContext;

// ---------------------------------------------------------------------------
// GPU resource handles
// ---------------------------------------------------------------------------

struct GPUTexture {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct GPUMesh {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    std::uint32_t vertexCount = 0;
    std::uint32_t indexCount = 0;
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
};

// ---------------------------------------------------------------------------
// Asset definitions
// ---------------------------------------------------------------------------

struct MaterialDef {
    glm::vec3 albedo = glm::vec3(1.0f);
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    std::uint32_t albedoTexture = UINT32_MAX;
    std::uint32_t normalTexture = UINT32_MAX;
    std::uint32_t metallicRoughnessTexture = UINT32_MAX;
    std::string name;
};

struct ImportedModel {
    std::vector<std::uint32_t> meshIds;
    std::vector<std::uint32_t> materialIds;
};

// ---------------------------------------------------------------------------
// AssetManager
// ---------------------------------------------------------------------------

class AssetManager {
  public:
    /// Import a 3D model file (OBJ, FBX, glTF, etc.) and all its textures.
    /// Returns handles into the internal mesh/material/texture pools.
    [[nodiscard]] ImportedModel importModel(VulkanContext& ctx, std::string_view filePath);

    [[nodiscard]] const GPUMesh& mesh(std::uint32_t id) const;
    [[nodiscard]] const MaterialDef& material(std::uint32_t id) const;
    [[nodiscard]] const GPUTexture& texture(std::uint32_t id) const;

    [[nodiscard]] std::uint32_t meshCount() const;
    [[nodiscard]] std::uint32_t materialCount() const;
    [[nodiscard]] std::uint32_t textureCount() const;

    void shutdown(VkDevice device);

  private:
    [[nodiscard]] std::optional<std::uint32_t> loadTexture(VulkanContext& ctx,
                                                            std::string_view path);
    [[nodiscard]] std::uint32_t addMaterial(const MaterialDef& mat);
    [[nodiscard]] std::uint32_t uploadMesh(VulkanContext& ctx, const float* vertices,
        std::uint32_t vertexCount, const std::uint32_t* indices,
        std::uint32_t indexCount);

    std::vector<GPUMesh> meshes_;
    std::vector<MaterialDef> materials_;
    std::vector<GPUTexture> textures_;
};

}  // namespace vr
