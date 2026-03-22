/**
 * @file SceneFrameGraphPass.h
 * @brief Declarations for the SceneFrameGraphPass module.
 */
#pragma once

#include <functional>
#include <string_view>

#include "renderer/framegraph/RenderGraph.h"

namespace vr {

class SceneFrameGraphPass final : public RenderGraph::IPass {
public:
    explicit SceneFrameGraphPass(std::function<void()> executeCallback);

    [[nodiscard]] std::string_view name() const override;
    void setup(RenderGraph::PassBuilder& builder) override;
    void execute() override;

private:
    std::function<void()> executeCallback_;
};

} // namespace vr


