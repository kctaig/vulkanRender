#pragma once

#include "renderer/Renderer.h"

namespace vr {

class Application {
public:
    int run();

private:
    Renderer renderer_;
};

} // namespace vr
