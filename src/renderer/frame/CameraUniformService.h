/**
 * @file CameraUniformService.h
 * @brief Declarations for the CameraUniformService module.
 */
#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace vr {

class CameraUniformService {
public:
    struct Source {
        glm::vec3 modelTranslation = glm::vec3(0.0f);
        float modelYawRadians = 0.0f;
        float modelPitchRadians = 0.0f;
        float cameraDistance = 3.5f;
        VkExtent2D swapchainExtent{};
        float sceneRadius = 1.0f;
    };

    struct Matrices {
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
    };

    Matrices buildMatrices(const Source& source) const;
};

} // namespace vr


