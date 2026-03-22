/**
 * @file ShadowPass.cpp
 * @brief Implementation for the ShadowPass module.
 */
#include "renderer/passes/ShadowPass.h"

#include <iostream>

namespace vr {

std::string_view ShadowPass::name() const {
    return "ShadowPass";
}

void ShadowPass::setup(RenderGraph::PassBuilder& builder) {
    builder.writes("ShadowMap");
    std::cout << "[Pass][Shadow] setup\n";
}

void ShadowPass::execute() {
    std::cout << "[Pass][Shadow] execute (PCF/PCSS hook)\n";
}

} // namespace vr


