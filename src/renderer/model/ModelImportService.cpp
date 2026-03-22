/**
 * @file ModelImportService.cpp
 * @brief Implementation for the ModelImportService module.
 */
#include "renderer/model/ModelImportService.h"

#include <chrono>

namespace vr {

bool ModelImportService::startAsyncImport(
    VkDevice device,
    const std::string& path,
    const std::function<void(std::string)>& appendOutput
) {
    if (device == VK_NULL_HANDLE || importInProgress_) {
        return false;
    }

    pendingPath_ = path;
    importInProgress_ = true;

    if (appendOutput) {
        appendOutput(std::string("Importing model in background: ") + path);
    }

    pendingTask_ = std::async(std::launch::async, [path]() {
        MeshInputData meshData;
        loadObjMesh(path, meshData);
        return meshData;
    });

    return true;
}

ModelImportService::PollResult ModelImportService::poll() {
    if (!importInProgress_) {
        return {};
    }

    if (!pendingTask_.valid()) {
        importInProgress_ = false;
        return PollResult{
            .status = PollStatus::InvalidTask,
            .path = pendingPath_,
        };
    }

    if (pendingTask_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return {};
    }

    MeshInputData meshData = pendingTask_.get();
    importInProgress_ = false;

    if (meshData.vertices.empty() || meshData.indices.empty()) {
        return PollResult{
            .status = PollStatus::FailedEmpty,
            .path = pendingPath_,
        };
    }

    return PollResult{
        .status = PollStatus::Ready,
        .path = pendingPath_,
        .meshData = std::move(meshData),
    };
}

} // namespace vr


