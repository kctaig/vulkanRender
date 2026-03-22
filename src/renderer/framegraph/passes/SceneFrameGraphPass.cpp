/**
 * @file SceneFrameGraphPass.cpp
 * @brief Implementation for the SceneFrameGraphPass module.
 */
#include "renderer/framegraph/passes/SceneFrameGraphPass.h"

#include <utility>

namespace vr {

SceneFrameGraphPass::SceneFrameGraphPass(std::function<void()> executeCallback)
    : executeCallback_(std::move(executeCallback)) {}

std::string_view SceneFrameGraphPass::name() const {
    return "ScenePass";
}

void SceneFrameGraphPass::setup(RenderGraph::PassBuilder& builder) {
    builder.writes("SceneColor").writes("GBufferPosition").writes("GBufferNormal").writes("GBufferAlbedo");
}

void SceneFrameGraphPass::execute() {
    if (executeCallback_) {
        executeCallback_();
    }
}

} // namespace vr


