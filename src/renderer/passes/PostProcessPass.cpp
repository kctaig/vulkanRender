#include "renderer/passes/PostProcessPass.h"

#include <iostream>

namespace vr {

void PostProcessPass::setup() {
    std::cout << "[Pass][Post] setup\n";
}

void PostProcessPass::execute() {
    std::cout << "[Pass][Post] execute (HDR -> ToneMap -> TAA -> Bloom)\n";
}

} // namespace vr
