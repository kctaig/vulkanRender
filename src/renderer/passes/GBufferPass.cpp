#include "renderer/passes/GBufferPass.h"

#include <iostream>

namespace vr {

void GBufferPass::setup() {
    std::cout << "[Pass][GBuffer] setup\n";
}

void GBufferPass::execute() {
    std::cout << "[Pass][GBuffer] execute\n";
}

} // namespace vr
