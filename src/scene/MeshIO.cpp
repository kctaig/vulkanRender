#include "scene/MeshIO.h"

#include <algorithm>
#include <fstream>
#include <future>
#include <sstream>
#include <string>
#include <thread>
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
    std::vector<std::vector<FaceVertexKey>> faces;

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
            std::vector<FaceVertexKey> faceKeys;
            std::string token;
            while (lineStream >> token) {
                if (!token.empty()) {
                    FaceVertexKey key = parseFaceToken(token);
                    if (key.positionIndex >= 0 && static_cast<std::size_t>(key.positionIndex) < positions.size()) {
                        faceKeys.push_back(key);
                    }
                }
            }

            if (faceKeys.size() >= 3) {
                faces.push_back(std::move(faceKeys));
            }
        }
    }

    if (faces.empty()) {
        return false;
    }

    const std::size_t faceCount = faces.size();
    const std::size_t hardwareThreads = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    const std::size_t workerCount = std::min(hardwareThreads, faceCount);

    std::vector<std::vector<MeshVertexInput>> workerVertices(workerCount);
    std::vector<std::vector<std::uint32_t>> workerIndices(workerCount);
    std::vector<std::future<void>> tasks;
    tasks.reserve(workerCount);

    for (std::size_t worker = 0; worker < workerCount; ++worker) {
        tasks.push_back(std::async(std::launch::async, [&, worker]() {
            const std::size_t start = faceCount * worker / workerCount;
            const std::size_t end = faceCount * (worker + 1) / workerCount;

            auto& localVertices = workerVertices[worker];
            auto& localIndices = workerIndices[worker];

            for (std::size_t faceIndex = start; faceIndex < end; ++faceIndex) {
                const auto& face = faces[faceIndex];
                std::vector<std::uint32_t> localFaceIndices;
                localFaceIndices.reserve(face.size());

                for (const FaceVertexKey& key : face) {
                    MeshVertexInput vertex{};
                    vertex.position = positions[static_cast<std::size_t>(key.positionIndex)];

                    if (key.normalIndex >= 0 && static_cast<std::size_t>(key.normalIndex) < normals.size()) {
                        vertex.normal = normals[static_cast<std::size_t>(key.normalIndex)];
                    }

                    if (key.uvIndex >= 0 && static_cast<std::size_t>(key.uvIndex) < uvs.size()) {
                        vertex.uv = uvs[static_cast<std::size_t>(key.uvIndex)];
                    }

                    localFaceIndices.push_back(static_cast<std::uint32_t>(localVertices.size()));
                    localVertices.push_back(vertex);
                }

                for (std::size_t i = 1; i + 1 < localFaceIndices.size(); ++i) {
                    localIndices.push_back(localFaceIndices[0]);
                    localIndices.push_back(localFaceIndices[i]);
                    localIndices.push_back(localFaceIndices[i + 1]);
                }
            }
        }));
    }

    for (auto& task : tasks) {
        task.get();
    }

    std::size_t totalVertexCount = 0;
    std::size_t totalIndexCount = 0;
    for (std::size_t worker = 0; worker < workerCount; ++worker) {
        totalVertexCount += workerVertices[worker].size();
        totalIndexCount += workerIndices[worker].size();
    }

    outMesh.vertices.reserve(totalVertexCount);
    outMesh.indices.reserve(totalIndexCount);

    std::uint32_t vertexBase = 0;
    for (std::size_t worker = 0; worker < workerCount; ++worker) {
        outMesh.vertices.insert(outMesh.vertices.end(), workerVertices[worker].begin(), workerVertices[worker].end());
        for (std::uint32_t localIndex : workerIndices[worker]) {
            outMesh.indices.push_back(localIndex + vertexBase);
        }
        vertexBase += static_cast<std::uint32_t>(workerVertices[worker].size());
    }

    return !outMesh.vertices.empty() && !outMesh.indices.empty();
}

} // namespace vr
