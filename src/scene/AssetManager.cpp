#include "scene/AssetManager.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <vector>

#ifdef VR_HAS_ASSIMP
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "core/VulkanContext.h"

namespace vr {

namespace {

struct LoadedVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

void copyBufferToImage(VulkanContext& ctx, VkBuffer src, VkImage dst,
                       std::uint32_t w, std::uint32_t h) {
    auto cmdBufs = ctx.allocateCommandBuffers(1);
    VkCommandBuffer cmd = cmdBufs[0];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = dst;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {w, h, 1};
    vkCmdCopyBufferToImage(cmd, src, dst,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.graphicsQueue());
    vkFreeCommandBuffers(ctx.device(), ctx.commandPool(), 1, &cmd);
}

void createImageAndUpload(VulkanContext& ctx, std::uint32_t w,
                          std::uint32_t h, const void* pixels,
                          GPUTexture& out) {
    VkDeviceSize size = w * h * 4ULL;
    out.width = w;
    out.height = h;

    // Staging
    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;
    ctx.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuf, stagingMem);
    void* data;
    vkMapMemory(ctx.device(), stagingMem, 0, size, 0, &data);
    std::memcpy(data, pixels, static_cast<std::size_t>(size));
    vkUnmapMemory(ctx.device(), stagingMem);

    // Image
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent = {w, h, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateImage(ctx.device(), &imgInfo, nullptr, &out.image);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(ctx.device(), out.image, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex =
        ctx.findMemoryType(memReqs.memoryTypeBits,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(ctx.device(), &allocInfo, nullptr, &out.memory);
    vkBindImageMemory(ctx.device(), out.image, out.memory, 0);

    copyBufferToImage(ctx, stagingBuf, out.image, w, h);

    vkDestroyBuffer(ctx.device(), stagingBuf, nullptr);
    vkFreeMemory(ctx.device(), stagingMem, nullptr);

    out.view = ctx.createImageView(out.image, VK_FORMAT_R8G8B8A8_SRGB,
                                    VK_IMAGE_ASPECT_COLOR_BIT);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(ctx.device(), &samplerInfo, nullptr, &out.sampler);
}

}  // namespace

// ===================================================================
// Public
// ===================================================================

ImportedModel AssetManager::importModel(VulkanContext& ctx,
                                        const std::string& filePath) {
#ifdef VR_HAS_ASSIMP
    Assimp::Importer importer;
    const aiScene* scene =
        importer.ReadFile(filePath,
                          aiProcess_Triangulate | aiProcess_GenNormals |
                              aiProcess_JoinIdenticalVertices |
                              aiProcess_PreTransformVertices |
                              aiProcess_FlipUVs);
    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) !=
                                0u) {
        std::cerr << "[AssetManager] Assimp error: "
                  << importer.GetErrorString() << "\n";
        return {};
    }

    std::filesystem::path baseDir =
        std::filesystem::path(filePath).parent_path();

    // --- Process materials ---
    std::vector<std::uint32_t> sceneMaterialIds(scene->mNumMaterials);
    for (std::uint32_t i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* aiMat = scene->mMaterials[i];
        MaterialDef mat;
        aiString matName;
        aiMat->Get(AI_MATKEY_NAME, matName);
        mat.name = matName.C_Str();

        aiColor3D col;
        if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, col) == AI_SUCCESS) {
            mat.albedo = glm::vec3(col.r, col.g, col.b);
        }
        float metallic = 0.0f;
        aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
        mat.metallic = metallic;
        float roughness = 0.5f;
        aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        mat.roughness = roughness;

        // Helper: try multiple paths for a texture
        auto tryLoadTexture = [&](aiTextureType type) -> std::uint32_t {
            aiString texPath;
            if (aiMat->GetTexture(type, 0, &texPath) != AI_SUCCESS)
                return UINT32_MAX;

            std::filesystem::path relPath(texPath.C_Str());
            std::string filename = relPath.filename().string();

            // Try: model dir + relative path
            auto fullPath = (baseDir / relPath).string();
            auto id = loadTexture(ctx, fullPath);
            if (id != UINT32_MAX) return id;

            // Try: model dir + filename only
            fullPath = (baseDir / filename).string();
            id = loadTexture(ctx, fullPath);
            if (id != UINT32_MAX) return id;

            // Try: relative path from cwd
            id = loadTexture(ctx, relPath.string());
            return id;
        };

        mat.albedoTexture = tryLoadTexture(aiTextureType_DIFFUSE);
        if (mat.albedoTexture == UINT32_MAX) {
            std::cout << "[AssetManager] No diffuse texture found for material '"
                      << mat.name << "'" << std::endl;
        }

        mat.normalTexture = tryLoadTexture(aiTextureType_NORMALS);

        sceneMaterialIds[i] = addMaterial(mat);
    }

    // --- Process meshes ---
    ImportedModel result;
    for (std::uint32_t i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* aiM = scene->mMeshes[i];

        std::vector<LoadedVertex> verts(aiM->mNumVertices);
        for (std::uint32_t v = 0; v < aiM->mNumVertices; ++v) {
            verts[v].px = aiM->mVertices[v].x;
            verts[v].py = aiM->mVertices[v].y;
            verts[v].pz = aiM->mVertices[v].z;
            if (aiM->HasNormals()) {
                verts[v].nx = aiM->mNormals[v].x;
                verts[v].ny = aiM->mNormals[v].y;
                verts[v].nz = aiM->mNormals[v].z;
            }
            if (aiM->HasTextureCoords(0)) {
                verts[v].u = aiM->mTextureCoords[0][v].x;
                verts[v].v = aiM->mTextureCoords[0][v].y;
            }
        }

        std::vector<std::uint32_t> indices;
        for (std::uint32_t f = 0; f < aiM->mNumFaces; ++f) {
            for (std::uint32_t idx = 0; idx < aiM->mFaces[f].mNumIndices;
                 ++idx) {
                indices.push_back(aiM->mFaces[f].mIndices[idx]);
            }
        }

        std::uint32_t meshId = uploadMesh(
            ctx, reinterpret_cast<const float*>(verts.data()),
            static_cast<std::uint32_t>(verts.size()), indices.data(),
            static_cast<std::uint32_t>(indices.size()));
        result.meshIds.push_back(meshId);
        result.materialIds.push_back(sceneMaterialIds[aiM->mMaterialIndex]);
    }

    auto t = std::chrono::high_resolution_clock::now();
    std::cout << "[AssetManager] Imported " << filePath << " ("
              << result.meshIds.size() << " meshes, " << materials_.size()
              << " materials, " << textures_.size() << " textures)"
              << std::endl;
    return result;
#else
    (void)ctx;
    (void)filePath;
    std::cerr << "[AssetManager] Assimp not available (rebuild with assimp)"
              << std::endl;
    return {};
#endif
}

// ===================================================================
// Accessors
// ===================================================================

const GPUMesh& AssetManager::mesh(std::uint32_t id) const {
    return meshes_.at(id);
}

const MaterialDef& AssetManager::material(std::uint32_t id) const {
    return materials_.at(id);
}

const GPUTexture& AssetManager::texture(std::uint32_t id) const {
    return textures_.at(id);
}

std::uint32_t AssetManager::meshCount() const {
    return static_cast<std::uint32_t>(meshes_.size());
}

std::uint32_t AssetManager::materialCount() const {
    return static_cast<std::uint32_t>(materials_.size());
}

std::uint32_t AssetManager::textureCount() const {
    return static_cast<std::uint32_t>(textures_.size());
}

// ===================================================================
// Shutdown
// ===================================================================

void AssetManager::shutdown(VkDevice device) {
    for (auto& t : textures_) {
        if (t.sampler != VK_NULL_HANDLE)
            vkDestroySampler(device, t.sampler, nullptr);
        if (t.view != VK_NULL_HANDLE)
            vkDestroyImageView(device, t.view, nullptr);
        if (t.image != VK_NULL_HANDLE)
            vkDestroyImage(device, t.image, nullptr);
        if (t.memory != VK_NULL_HANDLE)
            vkFreeMemory(device, t.memory, nullptr);
    }
    textures_.clear();

    for (auto& m : meshes_) {
        if (m.vertexBuffer != VK_NULL_HANDLE)
            vkDestroyBuffer(device, m.vertexBuffer, nullptr);
        if (m.vertexMemory != VK_NULL_HANDLE)
            vkFreeMemory(device, m.vertexMemory, nullptr);
        if (m.indexBuffer != VK_NULL_HANDLE)
            vkDestroyBuffer(device, m.indexBuffer, nullptr);
        if (m.indexMemory != VK_NULL_HANDLE)
            vkFreeMemory(device, m.indexMemory, nullptr);
    }
    meshes_.clear();
    materials_.clear();
}

// ===================================================================
// Private
// ===================================================================

std::uint32_t AssetManager::loadTexture(VulkanContext& ctx,
                                         const std::string& path) {
    // Deduplicate by path
    static std::unordered_map<std::string, std::uint32_t> cache;
    auto it = cache.find(path);
    if (it != cache.end()) {
        return it->second;
    }

    int w = 0, h = 0, ch = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (pixels == nullptr) {
        return UINT32_MAX;
    }

    GPUTexture tex;
    createImageAndUpload(ctx, static_cast<std::uint32_t>(w),
                         static_cast<std::uint32_t>(h), pixels, tex);
    stbi_image_free(pixels);

    std::uint32_t id = static_cast<std::uint32_t>(textures_.size());
    textures_.push_back(tex);
    cache[path] = id;

    std::cout << "[AssetManager] Texture loaded: " << path << " (" << w
              << "x" << h << ")" << std::endl;
    return id;
}

std::uint32_t AssetManager::addMaterial(const MaterialDef& mat) {
    std::uint32_t id = static_cast<std::uint32_t>(materials_.size());
    materials_.push_back(mat);
    return id;
}

std::uint32_t AssetManager::uploadMesh(VulkanContext& ctx,
                                        const float* vertices,
                                        std::uint32_t vertexCount,
                                        const std::uint32_t* indices,
                                        std::uint32_t indexCount) {
    GPUMesh mesh;
    mesh.vertexCount = vertexCount;
    mesh.indexCount = indexCount;

    // Compute bounds
    auto* v = reinterpret_cast<const LoadedVertex*>(vertices);
    mesh.boundsMin = glm::vec3(v[0].px, v[0].py, v[0].pz);
    mesh.boundsMax = mesh.boundsMin;
    for (std::uint32_t i = 1; i < vertexCount; ++i) {
        mesh.boundsMin = glm::min(mesh.boundsMin, glm::vec3(v[i].px, v[i].py, v[i].pz));
        mesh.boundsMax = glm::max(mesh.boundsMax, glm::vec3(v[i].px, v[i].py, v[i].pz));
    }

    // Vertex buffer
    VkDeviceSize vbSize = vertexCount * sizeof(LoadedVertex);
    {
        VkBuffer stagingBuf;
        VkDeviceMemory stagingMem;
        ctx.createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         stagingBuf, stagingMem);
        void* data;
        vkMapMemory(ctx.device(), stagingMem, 0, vbSize, 0, &data);
        std::memcpy(data, vertices, static_cast<std::size_t>(vbSize));
        vkUnmapMemory(ctx.device(), stagingMem);

        ctx.createBuffer(vbSize,
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         mesh.vertexBuffer, mesh.vertexMemory);

        // Copy staging → device-local
        auto cmdBufs = ctx.allocateCommandBuffers(1);
        VkCommandBuffer cmd = cmdBufs[0];
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);
        VkBufferCopy copy{0, 0, vbSize};
        vkCmdCopyBuffer(cmd, stagingBuf, mesh.vertexBuffer, 1, &copy);
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        vkQueueSubmit(ctx.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(ctx.graphicsQueue());
        vkFreeCommandBuffers(ctx.device(), ctx.commandPool(), 1, &cmd);

        vkDestroyBuffer(ctx.device(), stagingBuf, nullptr);
        vkFreeMemory(ctx.device(), stagingMem, nullptr);
    }

    // Index buffer
    VkDeviceSize ibSize = indexCount * sizeof(std::uint32_t);
    {
        VkBuffer stagingBuf;
        VkDeviceMemory stagingMem;
        ctx.createBuffer(ibSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         stagingBuf, stagingMem);
        void* data;
        vkMapMemory(ctx.device(), stagingMem, 0, ibSize, 0, &data);
        std::memcpy(data, indices, static_cast<std::size_t>(ibSize));
        vkUnmapMemory(ctx.device(), stagingMem);

        ctx.createBuffer(ibSize,
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                             VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         mesh.indexBuffer, mesh.indexMemory);

        auto cmdBufs = ctx.allocateCommandBuffers(1);
        VkCommandBuffer cmd = cmdBufs[0];
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);
        VkBufferCopy copy{0, 0, ibSize};
        vkCmdCopyBuffer(cmd, stagingBuf, mesh.indexBuffer, 1, &copy);
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        vkQueueSubmit(ctx.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(ctx.graphicsQueue());
        vkFreeCommandBuffers(ctx.device(), ctx.commandPool(), 1, &cmd);

        vkDestroyBuffer(ctx.device(), stagingBuf, nullptr);
        vkFreeMemory(ctx.device(), stagingMem, nullptr);
    }

    std::uint32_t id = static_cast<std::uint32_t>(meshes_.size());
    meshes_.push_back(mesh);
    return id;
}

}  // namespace vr
