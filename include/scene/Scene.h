#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

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

struct Material {
    std::string name;
    glm::vec3 albedo = glm::vec3(1.0f);
    float metallic = 0.0f;
    float roughness = 1.0f;
    float ao = 1.0f;
};

struct MeshInstance {
    glm::mat4 transform = glm::mat4(1.0f);
};

class Scene {
  public:
    Camera camera;

    std::vector<DirectionalLight> directionalLights;
    std::vector<PointLight> pointLights;
    std::vector<Material> materials;
};

}  // namespace vr
