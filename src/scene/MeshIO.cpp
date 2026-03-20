#include "scene/MeshIO.h"

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vr {

namespace {

struct FaceVertexKey {
    int positionIndex = -1;
    int uvIndex = -1;
    int normalIndex = -1;

    bool operator==(const FaceVertexKey& other) const {
        return positionIndex == other.positionIndex && uvIndex == other.uvIndex && normalIndex == other.normalIndex;
    }
};

struct FaceVertexKeyHash {
    std::size_t operator()(const FaceVertexKey& key) const {
        std::size_t hash = std::hash<int>{}(key.positionIndex);
        hash ^= std::hash<int>{}(key.uvIndex) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<int>{}(key.normalIndex) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};

int parseOptionalObjIndex(const std::string& token) {
    if (token.empty()) {
        return -1;
    }

    const int indexValue = std::stoi(token);
    if (indexValue <= 0) {
        return -1;
    }

    return indexValue - 1;
}

FaceVertexKey parseFaceToken(const std::string& token) {
    FaceVertexKey key;
    std::size_t firstSlash = token.find('/');

    if (firstSlash == std::string::npos) {
        key.positionIndex = parseOptionalObjIndex(token);
        return key;
    }

    key.positionIndex = parseOptionalObjIndex(token.substr(0, firstSlash));

    std::size_t secondSlash = token.find('/', firstSlash + 1);
    if (secondSlash == std::string::npos) {
        key.uvIndex = parseOptionalObjIndex(token.substr(firstSlash + 1));
        return key;
    }

    key.uvIndex = parseOptionalObjIndex(token.substr(firstSlash + 1, secondSlash - firstSlash - 1));
    key.normalIndex = parseOptionalObjIndex(token.substr(secondSlash + 1));
    return key;
}

} // namespace

bool loadObjMesh(const std::string& filePath, MeshInputData& outMesh) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    outMesh.vertices.clear();
    outMesh.indices.clear();

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;

    std::unordered_map<FaceVertexKey, std::uint32_t, FaceVertexKeyHash> vertexCache;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream lineStream(line);
        std::string prefix;
        lineStream >> prefix;

        if (prefix == "v") {
            glm::vec3 position{};
            lineStream >> position.x >> position.y >> position.z;
            positions.push_back(position);
        } else if (prefix == "vt") {
            glm::vec2 uv{};
            lineStream >> uv.x >> uv.y;
            uv.y = 1.0f - uv.y;
            uvs.push_back(uv);
        } else if (prefix == "vn") {
            glm::vec3 normal{};
            lineStream >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } else if (prefix == "f") {
            std::vector<std::uint32_t> faceIndices;
            std::string token;
            while (lineStream >> token) {
                if (!token.empty()) {
                    FaceVertexKey key = parseFaceToken(token);
                    if (key.positionIndex < 0 || static_cast<std::size_t>(key.positionIndex) >= positions.size()) {
                        continue;
                    }

                    auto found = vertexCache.find(key);
                    if (found != vertexCache.end()) {
                        faceIndices.push_back(found->second);
                        continue;
                    }

                    MeshVertexInput vertex{};
                    vertex.position = positions[static_cast<std::size_t>(key.positionIndex)];

                    if (key.normalIndex >= 0 && static_cast<std::size_t>(key.normalIndex) < normals.size()) {
                        vertex.normal = normals[static_cast<std::size_t>(key.normalIndex)];
                    }

                    if (key.uvIndex >= 0 && static_cast<std::size_t>(key.uvIndex) < uvs.size()) {
                        vertex.uv = uvs[static_cast<std::size_t>(key.uvIndex)];
                    }

                    const std::uint32_t vertexIndex = static_cast<std::uint32_t>(outMesh.vertices.size());
                    outMesh.vertices.push_back(vertex);
                    vertexCache.emplace(key, vertexIndex);
                    faceIndices.push_back(vertexIndex);
                }
            }

            if (faceIndices.size() < 3) {
                continue;
            }

            for (std::size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                outMesh.indices.push_back(faceIndices[0]);
                outMesh.indices.push_back(faceIndices[i]);
                outMesh.indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    return !outMesh.vertices.empty() && !outMesh.indices.empty();
}

} // namespace vr
