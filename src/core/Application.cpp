#include "core/Application.h"

#include <iostream>

namespace vr {

int Application::run(int argc, char** argv) {
    constexpr unsigned int width = 1600;
    constexpr unsigned int height = 900;

    if (!renderer_.initialize(width, height)) {
        std::cerr << "Renderer initialize failed\n";
        return 1;
    }

    if (argc > 1 && argv[1] != nullptr) {
        renderer_.importModel(argv[1]);
    }

    renderer_.mainLoop();
    renderer_.shutdown();

    return 0;
}

}  // namespace vr
