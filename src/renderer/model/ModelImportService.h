/**
 * @file ModelImportService.h
 * @brief Declarations for the ModelImportService module.
 */
#pragma once

#include <future>
#include <functional>
#include <string>

#include <vulkan/vulkan.h>

#include "scene/MeshIO.h"

namespace vr {

class ModelImportService {
public:
    enum class PollStatus {
        NoUpdate,
        InvalidTask,
        FailedEmpty,
        Ready,
    };

    struct PollResult {
        PollStatus status = PollStatus::NoUpdate;
        std::string path;
        MeshInputData meshData;
    };

    bool inProgress() const {
        return importInProgress_;
    }

    bool startAsyncImport(VkDevice device, const std::string& path, const std::function<void(std::string)>& appendOutput);
    PollResult poll();

private:
    std::future<MeshInputData> pendingTask_;
    bool importInProgress_ = false;
    std::string pendingPath_;
};

} // namespace vr


