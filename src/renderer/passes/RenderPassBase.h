#pragma once

namespace vr {

class RenderPassBase {
public:
    virtual ~RenderPassBase() = default;
    virtual void setup() = 0;
    virtual void execute() = 0;
};

} // namespace vr
