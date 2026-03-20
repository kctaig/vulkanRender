#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace vr {

struct MeshVertexInput {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec2 uv = glm::vec2(0.0f);
};

struct MeshInputData {
    std::vector<MeshVertexInput> vertices;
    std::vector<std::uint32_t> indices;
};

bool loadObjMesh(const std::string& filePath, MeshInputData& outMesh);

} // namespace vr
