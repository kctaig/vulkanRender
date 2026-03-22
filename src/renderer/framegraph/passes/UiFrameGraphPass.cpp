/**
 * @file UiFrameGraphPass.cpp
 * @brief Implementation for the UiFrameGraphPass module.
 */
#include "renderer/framegraph/passes/UiFrameGraphPass.h"

#include <utility>

namespace vr {

UiFrameGraphPass::UiFrameGraphPass(std::function<void()> executeCallback)
    : executeCallback_(std::move(executeCallback)) {}

std::string_view UiFrameGraphPass::name() const {
    return "ImGuiPass";
}

void UiFrameGraphPass::setup(RenderGraph::PassBuilder& builder) {
    builder.reads("SceneColor").writes("SwapchainColor");
}

void UiFrameGraphPass::execute() {
    if (executeCallback_) {
        executeCallback_();
    }
}

} // namespace vr


