/**
 * @file LightingPass.h
 * @brief Declarations for the LightingPass module.
 */
#pragma once

#include "renderer/passes/RenderPassBase.h"

namespace vr {

class LightingPass final : public RenderPassBase {
public:
    [[nodiscard]] std::string_view name() const override;
    void setup(RenderGraph::PassBuilder& builder) override;
    void execute() override;
};

} // namespace vr


