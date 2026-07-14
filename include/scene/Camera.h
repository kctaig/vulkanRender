#pragma once

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vr {

/// Free orbit camera — no range limits. Movement speed scales with model size
/// so big models feel the same as small ones.
class Camera {
  public:
    // --- Base sensitivity (scaled by modelSize) ---
    float rotateSpeed = 0.3f;  // degrees per pixel
    float dollySpeed  = 10.0f; // per scroll-notch / WASD frame
    float panSpeed    = 3.0f;  // per pixel / WASD frame

    // --- Model scale (set once after import, affects speed only) ---
    void setModelScale(float size) {
        modelScale_ = std::max(size, 0.01f);
    }

    // --- Projection ---
    void setPerspective(float fov, float aspect, float nearP, float farP) {
        fov_ = fov;
        aspect_ = aspect;
        nearPlane_ = nearP;
        farPlane_ = farP;
    }

    // --- Target ---
    void setTarget(glm::vec3 t) { target_ = t; }
    void setDistance(float d) { distance_ = d; }

    // --- Orbit: constant angle per pixel ---
    void rotate(float dx, float dy) {
        float degX = dx * rotateSpeed;
        float degY = dy * rotateSpeed;
        yaw_ = glm::mod(yaw_ + glm::radians(degX), glm::radians(360.0f));
        pitch_ = std::clamp(pitch_ + glm::radians(degY), -1.5f, 1.5f);
    }

    // --- Dolly: proportional to model scale, no limits ---
    void dolly(float delta) {
        distance_ -= delta * dollySpeed * modelScale_ * 0.01f;
        if (distance_ < 0.0001f) distance_ = 0.0001f;
    }

    // --- Pan: proportional to model scale, no limits ---
    void pan(float dx, float dy) {
        float s = panSpeed * modelScale_ * 0.01f;
        glm::vec3 r = glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::vec3 u = glm::cross(r, forward());
        target_ += r * dx * s + u * dy * s;
    }

    // --- Matrices ---
    [[nodiscard]] glm::mat4 viewMatrix() const {
        glm::vec3 pos = target_ - forward() * distance_;
        return glm::lookAt(pos, target_, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    [[nodiscard]] glm::mat4 projectionMatrix() const {
        return glm::perspective(fov_, aspect_, nearPlane_, farPlane_);
    }

    [[nodiscard]] glm::vec3 position() const { return target_ - forward() * distance_; }
    [[nodiscard]] float distance() const { return distance_; }

  private:
    [[nodiscard]] glm::vec3 forward() const {
        return glm::vec3(std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_),
                         -std::cos(yaw_) * std::cos(pitch_));
    }

    glm::vec3 target_{0.0f};
    float distance_ = 3.5f;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float fov_ = glm::radians(45.0f);
    float aspect_ = 16.0f / 9.0f;
    float nearPlane_ = 0.1f;
    float farPlane_ = 100.0f;
    float modelScale_ = 1.0f;
};

}  // namespace vr
