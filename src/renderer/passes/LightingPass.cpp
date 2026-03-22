/**
 * @file LightingPass.cpp
 * @brief Implementation for the LightingPass module.
 */
#include "renderer/passes/LightingPass.h"

#include <iostream>

namespace vr {

std::string_view LightingPass::name() const {
    return "LightingPass";
}

void LightingPass::setup(RenderGraph::PassBuilder& builder) {
    builder.reads("GBufferPosition").reads("GBufferNormal").reads("GBufferAlbedo").writes("SceneColor");
    std::cout << "[Pass][Lighting] setup\n";
}

void LightingPass::execute() {
    std::cout << "[Pass][Lighting] execute (Deferred + PBR)\n";
}

} // namespace vr


