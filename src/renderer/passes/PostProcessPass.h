#pragma once

#include "renderer/passes/RenderPassBase.h"

namespace vr {

class PostProcessPass final : public RenderPassBase {
public:
    void setup() override;
    void execute() override;
};

} // namespace vr
