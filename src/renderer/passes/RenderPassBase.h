/**
 * @file RenderPassBase.h
 * @brief Declarations for the RenderPassBase module.
 */
#pragma once

#include <string_view>

#include "renderer/framegraph/RenderGraph.h"

namespace vr {

class RenderPassBase : public RenderGraph::IPass {
public:
    virtual ~RenderPassBase() = default;
    [[nodiscard]] virtual std::string_view name() const = 0;
    virtual void setup(RenderGraph::PassBuilder& builder) = 0;
    virtual void execute() = 0;
};

} // namespace vr


