/**
 * @file CameraUniformService.cpp
 * @brief Implementation for the CameraUniformService module.
 */
#include "renderer/frame/CameraUniformService.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

namespace vr {

CameraUniformService::Matrices CameraUniformService::buildMatrices(const Source& source) const {
    Matrices matrices{};

    glm::mat4 model = glm::translate(glm::mat4(1.0f), source.modelTranslation);
    model = glm::rotate(model, source.modelYawRadians, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, source.modelPitchRadians, glm::vec3(1.0f, 0.0f, 0.0f));
    matrices.model = model;

    matrices.view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, source.cameraDistance),
        glm::vec3(0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    const float aspect = static_cast<float>(source.swapchainExtent.width) /
                         static_cast<float>(std::max(1U, source.swapchainExtent.height));
    matrices.projection = glm::perspective(
        glm::radians(45.0f),
        aspect,
        0.1f,
        std::max(100.0f, source.sceneRadius * 50.0f)
    );
    matrices.projection[1][1] *= -1.0f;

    return matrices;
}

} // namespace vr


