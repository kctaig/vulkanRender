#pragma once

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vr {

class Camera {
  public:
    void setPerspective(float fov, float aspect, float nearPlane, float farPlane) {
        fov_ = fov;
        aspect_ = aspect;
        nearPlane_ = nearPlane;
        farPlane_ = farPlane;
    }

    void setTarget(glm::vec3 target) {
        target_ = target;
    }
    void setDistance(float d) {
        distance_ = std::clamp(d, 0.1f, maxDistance_);
    }
    void setMaxDistance(float d) {
        maxDistance_ = std::max(1.0f, d);
    }

    void rotate(float deltaYaw, float deltaPitch) {
        yaw_ += deltaYaw;
        pitch_ = std::clamp(pitch_ + deltaPitch, -1.5f, 1.5f);
    }

    void zoom(float delta) {
        distance_ = std::clamp(distance_ - delta, 0.1f, maxDistance_);
    }

    void pan(glm::vec2 delta) {
        glm::vec3 right = glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::vec3 up = glm::cross(right, forward());
        target_ += right * delta.x + up * delta.y;
    }

    [[nodiscard]] glm::mat4 viewMatrix() const {
        glm::vec3 pos = target_ - forward() * distance_;
        return glm::lookAt(pos, target_, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    [[nodiscard]] glm::mat4 projectionMatrix() const {
        return glm::perspective(fov_, aspect_, nearPlane_, farPlane_);
    }

    [[nodiscard]] glm::vec3 position() const {
        return target_ - forward() * distance_;
    }

    [[nodiscard]] float distance() const {
        return distance_;
    }
    [[nodiscard]] float nearPlane() const {
        return nearPlane_;
    }
    [[nodiscard]] float farPlane() const {
        return farPlane_;
    }
    [[nodiscard]] float fov() const {
        return fov_;
    }
    [[nodiscard]] float aspect() const {
        return aspect_;
    }

  private:
    [[nodiscard]] glm::vec3 forward() const {
        return glm::vec3(
            std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_), -std::cos(yaw_) * std::cos(pitch_)
        );
    }

    glm::vec3 target_ = glm::vec3(0.0f);
    float distance_ = 3.5f;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float fov_ = glm::radians(45.0f);
    float aspect_ = 16.0f / 9.0f;
    float nearPlane_ = 0.1f;
    float farPlane_ = 100.0f;
    float maxDistance_ = 100.0f;
};

}  // namespace vr
