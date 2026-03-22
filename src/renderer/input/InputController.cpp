/**
 * @file InputController.cpp
 * @brief Implementation for the InputController module.
 */
#include "renderer/input/InputController.h"

#include <algorithm>

#include <Windowsx.h>

namespace vr {

InputController::HandleResult InputController::handleWindowInput(
    State& state,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    bool uiCapturingMouse,
    bool sceneAllowsMouseInteraction
) const {
    switch (message) {
    case WM_CLOSE:
        state.windowRunning = false;
        PostQuitMessage(0);
        return {.handled = true, .value = 0};
    case WM_SIZE:
        state.windowWidth = static_cast<unsigned int>(LOWORD(lParam));
        state.windowHeight = static_cast<unsigned int>(HIWORD(lParam));
        if (wParam != SIZE_MINIMIZED) {
            state.framebufferResized = true;
        }
        return {.handled = true, .value = 0};
    case WM_RBUTTONDOWN:
        if (uiCapturingMouse && !sceneAllowsMouseInteraction) {
            state.rightDragActive = false;
            return {.handled = true, .value = 0};
        }
        state.rightDragActive = true;
        state.lastMousePosition = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        return {.handled = true, .value = 0};
    case WM_RBUTTONUP:
        state.rightDragActive = false;
        return {.handled = true, .value = 0};
    case WM_LBUTTONDOWN:
        if (uiCapturingMouse && !sceneAllowsMouseInteraction) {
            state.leftDragActive = false;
            return {.handled = true, .value = 0};
        }
        state.leftDragActive = true;
        state.lastMousePosition = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        return {.handled = true, .value = 0};
    case WM_LBUTTONUP:
        state.leftDragActive = false;
        return {.handled = true, .value = 0};
    case WM_MOUSEMOVE: {
        const POINT currentPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const float deltaX = static_cast<float>(currentPoint.x - state.lastMousePosition.x);
        const float deltaY = static_cast<float>(currentPoint.y - state.lastMousePosition.y);
        state.lastMousePosition = currentPoint;

        if (uiCapturingMouse && !sceneAllowsMouseInteraction) {
            state.rightDragActive = false;
            state.leftDragActive = false;
            return {.handled = true, .value = 0};
        }

        if (state.rightDragActive) {
            state.modelYawRadians += deltaX * 0.01f;
            state.modelPitchRadians += deltaY * 0.01f;
        }
        if (state.leftDragActive) {
            state.modelTranslation.x += deltaX * 0.005f;
            state.modelTranslation.y -= deltaY * 0.005f;
        }
        return {.handled = true, .value = 0};
    }
    case WM_MOUSEWHEEL: {
        if (uiCapturingMouse && !sceneAllowsMouseInteraction) {
            return {.handled = true, .value = 0};
        }
        const short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        state.cameraDistance -= static_cast<float>(wheelDelta) / static_cast<float>(WHEEL_DELTA) * 0.2f;
        state.cameraDistance = std::clamp(state.cameraDistance, 1.0f, std::max(20.0f, state.sceneRadius * 20.0f));
        return {.handled = true, .value = 0};
    }
    default:
        return {.handled = false, .value = 0};
    }
}

} // namespace vr


