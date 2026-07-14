#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "scene/AssetManager.h"
#include "scene/Camera.h"

namespace vr {

struct DirectionalLight {
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
};

struct PointLight {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float range = 10.0f;
};

class Scene {
  public:
    struct Instance {
        std::uint32_t modelId = UINT32_MAX;
        glm::mat4 transform = glm::mat4(1.0f);
    };

    Camera camera;
    AssetManager assets;
    float modelRadius = 1.0f;

    std::vector<Instance> instances;
    std::vector<DirectionalLight> directionalLights;
    std::vector<PointLight> pointLights;

    /// One-shot import: loads model + textures into assets, adds an instance.
    /// Auto-adjusts camera to frame the model.
    void importModel(VulkanContext& ctx, std::string_view path) {
        auto m = assets.importModel(ctx, path);
        if (m.meshIds.empty()) return;

        // Compute combined bounding box from all meshes
        glm::vec3 bbMin(std::numeric_limits<float>::max());
        glm::vec3 bbMax(std::numeric_limits<float>::lowest());
        for (auto id : m.meshIds) {
            const auto& mesh = assets.mesh(id);
            bbMin = glm::min(bbMin, mesh.boundsMin);
            bbMax = glm::max(bbMax, mesh.boundsMax);
        }
        glm::vec3 center = (bbMin + bbMax) * 0.5f;
        glm::vec3 halfExt = (bbMax - bbMin) * 0.5f;
        if (halfExt.x < 0.01f && halfExt.y < 0.01f && halfExt.z < 0.01f)
            halfExt = glm::vec3(1.0f);

        float fovY = glm::radians(45.0f);
        float aspect = 16.0f / 9.0f;
        float fovX = 2.0f * std::atan(std::tan(fovY * 0.5f) * aspect);

        float distY = halfExt.y / std::tan(fovY * 0.5f);
        float distX = halfExt.x / std::tan(fovX * 0.5f);
        float distZ = halfExt.z / std::tan(fovX * 0.5f);
        float distance = std::max({distX, distY, distZ}) * 1.1f;
        float radius = glm::length(glm::vec3{halfExt.x, halfExt.y, halfExt.z});
        modelRadius = std::max(radius, 0.01f);

        camera.setTarget(center);
        camera.setDistance(distance);
        camera.setModelScale(modelRadius);

        for (std::size_t i = 0; i < m.meshIds.size(); ++i) {
            instances.push_back({m.meshIds[i], glm::mat4(1.0f)});
        }
    }

    void shutdown(VkDevice device) { assets.shutdown(device); }
};

}  // namespace vr
