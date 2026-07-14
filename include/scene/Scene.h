#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace vr {

struct Mesh {
    std::string name;
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    glm::mat4 model = glm::mat4(1.0f);
};

struct Material {
    std::string name;
    glm::vec3 albedo = glm::vec3(1.0f);
    float metallic = 0.0f;
    float roughness = 1.0f;
    float ao = 1.0f;
};

struct Light {
    glm::vec3 position = glm::vec3(0.0f, 5.0f, 0.0f);
    float intensity = 10.0f;
    glm::vec3 color = glm::vec3(1.0f);
    float radius = 10.0f;
};

struct Camera {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
};

struct Scene {
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Light> lights;
    Camera camera;
};

} // namespace vr
