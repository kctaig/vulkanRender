#pragma once

#include "renderer/passes/RenderPassBase.h"

namespace vr {

class ShadowPass final : public RenderPassBase {
public:
    void setup() override;
    void execute() override;
};

} // namespace vr
