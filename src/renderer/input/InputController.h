/**
 * @file InputController.h
 * @brief Declarations for the InputController module.
 */
#pragma once

#include <Windows.h>

#include <glm/glm.hpp>

namespace vr {

class InputController {
public:
    /** @brief Result wrapper indicating whether a window message was consumed. */
    struct HandleResult {
        bool handled = false;
        LRESULT value = 0;
    };

    /** @brief Mutable renderer/input state that can be updated by input handling. */
    struct State {
        bool& windowRunning;
        bool& rightDragActive;
        bool& leftDragActive;
        unsigned int& windowWidth;
        unsigned int& windowHeight;
        bool& framebufferResized;
        POINT& lastMousePosition;
        glm::vec3& modelTranslation;
        float& modelYawRadians;
        float& modelPitchRadians;
        float& cameraDistance;
        float sceneRadius = 1.0f;
    };

    /**
     * @brief Handles a single Win32 input message and updates interaction state.
     * @param state Mutable view of renderer interaction state.
     * @param message Win32 message id.
     * @param wParam First message parameter.
     * @param lParam Second message parameter.
     * @param uiCapturingMouse True when ImGui currently owns mouse input.
     * @param sceneAllowsMouseInteraction True when scene viewport accepts interaction.
     * @return Message handling result and optional LRESULT value.
     */
    HandleResult handleWindowInput(
        State& state,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        bool uiCapturingMouse,
        bool sceneAllowsMouseInteraction
    ) const;
};

} // namespace vr


