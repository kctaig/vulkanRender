#pragma once

#include "renderer/Renderer.h"

namespace vr {

class Application {
  public:
    int run(int argc, char** argv);

  private:
    Renderer renderer_;
};

}  // namespace vr
