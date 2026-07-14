#pragma once

#include <cstdint>
#include <limits>
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

    std::vector<Instance> instances;
    std::vector<DirectionalLight> directionalLights;
    std::vector<PointLight> pointLights;

    /// One-shot import: loads model + textures into assets, adds an instance.
    /// Auto-adjusts camera to frame the model.
    void importModel(VulkanContext& ctx, const std::string& path) {
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
        float radius = glm::length(bbMax - bbMin) * 0.5f;
        if (radius < 0.01f) radius = 1.0f;

        camera.setTarget(center);
        camera.setDistance(radius * 2.5f);
        camera.setMaxDistance(radius * 20.0f);

        for (std::size_t i = 0; i < m.meshIds.size(); ++i) {
            instances.push_back({m.meshIds[i], glm::mat4(1.0f)});
        }
    }

    void shutdown(VkDevice device) { assets.shutdown(device); }
};

}  // namespace vr
