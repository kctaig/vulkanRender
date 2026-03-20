#include "renderer/passes/LightingPass.h"

#include <iostream>

namespace vr {

void LightingPass::setup() {
    std::cout << "[Pass][Lighting] setup\n";
}

void LightingPass::execute() {
    std::cout << "[Pass][Lighting] execute (Deferred + PBR)\n";
}

} // namespace vr
