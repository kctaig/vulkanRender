/**
 * @file GBufferPass.cpp
 * @brief Implementation for the GBufferPass module.
 */
#include "renderer/passes/GBufferPass.h"

#include <iostream>

namespace vr {

std::string_view GBufferPass::name() const {
    return "GBufferPass";
}

void GBufferPass::setup(RenderGraph::PassBuilder& builder) {
    builder.writes("GBufferPosition").writes("GBufferNormal").writes("GBufferAlbedo").writes("Depth");
    std::cout << "[Pass][GBuffer] setup\n";
}

void GBufferPass::execute() {
    std::cout << "[Pass][GBuffer] execute\n";
}

} // namespace vr


