/**
 * @file MeshIO.h
 * @brief OBJ mesh import data structures and API.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace vr {

/** @brief Vertex record produced by OBJ parsing before renderer conversion. */
struct MeshVertexInput {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec2 uv = glm::vec2(0.0f);
};

/** @brief Imported mesh payload containing linearized vertices and indices. */
struct MeshInputData {
    std::vector<MeshVertexInput> vertices;
    std::vector<std::uint32_t> indices;
};

/**
 * @brief Loads an OBJ file into mesh input buffers.
 * @param filePath Absolute or relative path to the OBJ file.
 * @param outMesh Output structure populated with parsed vertex/index data.
 * @return True when parsing succeeds and output is non-empty; otherwise false.
 */
bool loadObjMesh(const std::string& filePath, MeshInputData& outMesh);

} // namespace vr

