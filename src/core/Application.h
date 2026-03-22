/**
 * @file Application.h
 * @brief Application entry-layer declarations.
 */
#pragma once

#include "renderer/Renderer.h"

namespace vr {

/**
 * @brief Top-level application controller.
 *
 * Owns the renderer instance and coordinates startup, the main render loop,
 * and shutdown sequencing.
 */
class Application {
public:
    /**
     * @brief Runs the application lifecycle.
     * @param argc Number of command-line arguments.
     * @param argv Command-line argument vector.
     * @return Process exit code, where 0 indicates success.
     */
    int run(int argc, char** argv);

private:
    Renderer renderer_;
};

} // namespace vr

