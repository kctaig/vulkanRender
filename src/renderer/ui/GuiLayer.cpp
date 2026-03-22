/**
 * @file GuiLayer.cpp
 * @brief Implementation for the GuiLayer module.
 */
#include "renderer/ui/GuiLayer.h"

#include <algorithm>

#include <imgui.h>

namespace vr {

void GuiLayer::build(BuildContext& context) const {
    if (context.sceneViewHovered == nullptr ||
        context.sceneViewFocused == nullptr ||
        context.sceneViewportX == nullptr ||
        context.sceneViewportY == nullptr ||
        context.sceneViewportWidth == nullptr ||
        context.sceneViewportHeight == nullptr ||
        context.modelTranslation == nullptr ||
        context.modelYawRadians == nullptr ||
        context.modelPitchRadians == nullptr ||
        context.cameraDistance == nullptr ||
        context.lightingDebugMode == nullptr ||
        context.deferredLightCount == nullptr ||
        context.deferredLightIntensity == nullptr ||
        context.useProceduralMaterialMaps == nullptr ||
        context.materialMetallic == nullptr ||
        context.materialRoughness == nullptr ||
        context.materialAo == nullptr ||
        context.iblIntensity == nullptr ||
        context.outputLines == nullptr ||
        context.autoScrollOutput == nullptr ||
        context.smoothedFrameTimeMs == nullptr ||
        context.smoothedFps == nullptr) {
        return;
    }

    *context.sceneViewHovered = false;
    *context.sceneViewFocused = false;
    *context.sceneViewportX = 0;
    *context.sceneViewportY = 0;
    *context.sceneViewportWidth = static_cast<int>(context.swapchainExtent.width);
    *context.sceneViewportHeight = static_cast<int>(context.swapchainExtent.height);

    ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    const ImGuiWindowFlags sceneWindowFlags = ImGuiWindowFlags_NoBackground |
                                              ImGuiWindowFlags_NoScrollbar |
                                              ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin("Scene", nullptr, sceneWindowFlags)) {
        *context.sceneViewHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        *context.sceneViewFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
        const ImVec2 sceneRegionMin(windowPos.x + contentMin.x, windowPos.y + contentMin.y);
        ImVec2 sceneRegionSize = ImGui::GetContentRegionAvail();
        if (sceneRegionSize.x < 8.0f || sceneRegionSize.y < 8.0f) {
            sceneRegionSize = ImVec2(8.0f, 8.0f);
        }

        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        const float viewportOriginX = mainViewport != nullptr ? mainViewport->Pos.x : 0.0f;
        const float viewportOriginY = mainViewport != nullptr ? mainViewport->Pos.y : 0.0f;
        *context.sceneViewportX = std::max(0, static_cast<int>(sceneRegionMin.x - viewportOriginX));
        *context.sceneViewportY = std::max(0, static_cast<int>(sceneRegionMin.y - viewportOriginY));
        *context.sceneViewportWidth = std::max(1, static_cast<int>(sceneRegionSize.x));
        *context.sceneViewportHeight = std::max(1, static_cast<int>(sceneRegionSize.y));

        if (context.sceneTextureId != nullptr) {
            ImGui::Image(context.sceneTextureId, sceneRegionSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        } else {
            ImGui::InvisibleButton("##SceneViewportRegion", sceneRegionSize);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::Begin("Stage");
    ImGui::Text("Right Drag: Rotate");
    ImGui::Text("Left Drag: Move");
    ImGui::Text("Mouse Wheel: Zoom");
    ImGui::SliderFloat3("Model Translate", &context.modelTranslation->x, -5.0f, 5.0f);
    ImGui::SliderFloat("Yaw", context.modelYawRadians, -3.14f, 3.14f);
    ImGui::SliderFloat("Pitch", context.modelPitchRadians, -3.14f, 3.14f);

    const char* debugViewItems[] = {"Final", "Position", "Normal", "Albedo"};
    *context.lightingDebugMode = std::clamp(*context.lightingDebugMode, 0, static_cast<int>(IM_ARRAYSIZE(debugViewItems) - 1));
    if (ImGui::Combo("Deferred Debug", context.lightingDebugMode, debugViewItems, IM_ARRAYSIZE(debugViewItems))) {
        if (context.onAppendOutput) {
            context.onAppendOutput(std::string("Deferred debug view: ") + debugViewItems[*context.lightingDebugMode]);
        }
    }

    ImGui::SliderInt("Deferred Light Count", context.deferredLightCount, 1, 32);
    ImGui::SliderFloat("Deferred Light Intensity", context.deferredLightIntensity, 0.1f, 8.0f);

    ImGui::SeparatorText("PBR Material");
    ImGui::Checkbox("Use Procedural Material Maps", context.useProceduralMaterialMaps);
    ImGui::SliderFloat("Metallic", context.materialMetallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness", context.materialRoughness, 0.04f, 1.0f);
    ImGui::SliderFloat("AO", context.materialAo, 0.0f, 1.0f);
    ImGui::SliderFloat("IBL Intensity", context.iblIntensity, 0.0f, 4.0f);

    const float cameraDistanceMax = std::max(20.0f, context.sceneRadius * 20.0f);
    ImGui::SliderFloat("Camera Distance", context.cameraDistance, 1.0f, cameraDistanceMax);

    if (ImGui::Button("Import Model (.obj)")) {
        if (context.onOpenModelDialog && context.onReloadModelPath) {
            std::string selectedPath;
            if (context.onOpenModelDialog(selectedPath)) {
                context.onReloadModelPath(selectedPath);
            } else if (context.onAppendOutput) {
                context.onAppendOutput("Model import canceled");
            }
        }
    }
    if (context.modelImportInProgress) {
        ImGui::SameLine();
        ImGui::TextUnformatted("Loading...");
    }

    if (ImGui::Button("Reset")) {
        *context.modelTranslation = glm::vec3(0.0f);
        *context.modelYawRadians = 0.0f;
        *context.modelPitchRadians = 0.0f;
        *context.cameraDistance = std::clamp(context.sceneRadius * 2.8f, 1.0f, cameraDistanceMax);
        if (context.onAppendOutput) {
            context.onAppendOutput("Camera/model transform reset");
        }
    }

    const float currentFrameTimeMs = 1000.0f / std::max(ImGui::GetIO().Framerate, 1.0f);
    *context.smoothedFrameTimeMs = (*context.smoothedFrameTimeMs <= 0.0f)
        ? currentFrameTimeMs
        : (*context.smoothedFrameTimeMs * 0.9f + currentFrameTimeMs * 0.1f);
    *context.smoothedFps = (*context.smoothedFrameTimeMs > 1e-3f) ? (1000.0f / *context.smoothedFrameTimeMs) : 0.0f;

    ImGui::Text("Frame time %.3f ms (%.1f FPS)", currentFrameTimeMs, ImGui::GetIO().Framerate);
    ImGui::Text("Smoothed %.3f ms (%.1f FPS)", *context.smoothedFrameTimeMs, *context.smoothedFps);
    ImGui::End();

    ImGui::Begin("Output");
    if (ImGui::Button("Clear")) {
        context.outputLines->clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto Scroll", context.autoScrollOutput);
    ImGui::Separator();

    ImGui::BeginChild("OutputScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const std::string& line : *context.outputLines) {
        ImGui::TextUnformatted(line.c_str());
    }
    if (*context.autoScrollOutput && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace vr


