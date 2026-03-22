/**
 * @file PostProcessPass.cpp
 * @brief Implementation for the PostProcessPass module.
 */
#include "renderer/passes/PostProcessPass.h"

#include <iostream>

namespace vr {

std::string_view PostProcessPass::name() const {
    return "PostProcessPass";
}

void PostProcessPass::setup(RenderGraph::PassBuilder& builder) {
    builder.reads("SceneColor").writes("PostProcessColor");
    std::cout << "[Pass][Post] setup\n";
}

void PostProcessPass::execute() {
    std::cout << "[Pass][Post] execute (HDR -> ToneMap -> TAA -> Bloom)\n";
}

} // namespace vr


