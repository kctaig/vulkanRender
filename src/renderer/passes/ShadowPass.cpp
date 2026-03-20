#include "renderer/passes/ShadowPass.h"

#include <iostream>

namespace vr {

void ShadowPass::setup() {
    std::cout << "[Pass][Shadow] setup\n";
}

void ShadowPass::execute() {
    std::cout << "[Pass][Shadow] execute (PCF/PCSS hook)\n";
}

} // namespace vr
