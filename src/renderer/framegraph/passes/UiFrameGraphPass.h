/**
 * @file UiFrameGraphPass.h
 * @brief Declarations for the UiFrameGraphPass module.
 */
#pragma once

#include <functional>
#include <string_view>

#include "renderer/framegraph/RenderGraph.h"

namespace vr {

class UiFrameGraphPass final : public RenderGraph::IPass {
public:
    explicit UiFrameGraphPass(std::function<void()> executeCallback);

    [[nodiscard]] std::string_view name() const override;
    void setup(RenderGraph::PassBuilder& builder) override;
    void execute() override;

private:
    std::function<void()> executeCallback_;
};

} // namespace vr


